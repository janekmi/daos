/**
 * (C) Copyright 2019-2023 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
/**
 * dtx: XXX
 */
#include <daos_types.h>
#include <daos_srv/vos.h>
#include "dtx_internal.h"

/** VOS reserves highest two minor epoch values for internal use so we must
 *  limit the number of dtx sub modifications to avoid conflict.
 */
#define DTX_SUB_MOD_MAX	(((uint16_t)-1) - 2)

/* Return the epoch uncertainty upper bound. */
static daos_epoch_t
dtx_epoch_bound(struct dtx_epoch *epoch)
{
	daos_epoch_t limit;

	if (!(epoch->oe_flags & DTX_EPOCH_UNCERTAIN))
		/*
		 * We are told that the epoch has no uncertainty, even if it's
		 * still within the potential uncertainty window.
		 */
		return epoch->oe_value;

	limit = d_hlc_epsilon_get_bound(epoch->oe_first);
	if (epoch->oe_value >= limit)
		/*
		 * The epoch is already out of the potential uncertainty
		 * window.
		 */
		return epoch->oe_value;

	return limit;
}

static void
dtx_shares_init(struct dtx_handle *dth)
{
	D_INIT_LIST_HEAD(&dth->dth_share_cmt_list);
	D_INIT_LIST_HEAD(&dth->dth_share_abt_list);
	D_INIT_LIST_HEAD(&dth->dth_share_act_list);
	D_INIT_LIST_HEAD(&dth->dth_share_tbd_list);
	dth->dth_share_tbd_count = 0;
	dth->dth_shares_inited = 1;
}

/**
 * Init local dth handle.
 */
static int
dtx_handle_init(struct dtx_id *dti, daos_handle_t coh, struct dtx_epoch *epoch,
		uint16_t sub_modification_cnt, uint32_t pm_ver,
		daos_unit_oid_t *leader_oid, struct dtx_id *dti_cos,
		int dti_cos_cnt, struct dtx_memberships *mbs, bool leader,
		bool solo, bool sync, bool dist, bool migration, bool ignore_uncommitted,
		bool resent, bool prepared, bool drop_cmt, struct dtx_handle *dth)
{
	if (sub_modification_cnt > DTX_SUB_MOD_MAX) {
		D_ERROR("Too many modifications in a single transaction:"
			"%u > %u\n", sub_modification_cnt, DTX_SUB_MOD_MAX);
		return -DER_OVERFLOW;
	}
	dth->dth_modification_cnt = sub_modification_cnt;

	dtx_shares_init(dth);

	dth->dth_xid = *dti;
	dth->dth_coh = coh;

	dth->dth_leader_oid = *leader_oid;
	dth->dth_ver = pm_ver;
	dth->dth_refs = 1;
	dth->dth_mbs = mbs;

	dth->dth_pinned = 0;
	dth->dth_cos_done = 0;
	dth->dth_resent = resent ? 1 : 0;
	dth->dth_solo = solo ? 1 : 0;
	dth->dth_drop_cmt = drop_cmt ? 1 : 0;
	dth->dth_modify_shared = 0;
	dth->dth_active = 0;
	dth->dth_touched_leader_oid = 0;
	dth->dth_local_tx_started = 0;
	dth->dth_dist = dist ? 1 : 0;
	dth->dth_for_migration = migration ? 1 : 0;
	dth->dth_ignore_uncommitted = ignore_uncommitted ? 1 : 0;
	dth->dth_prepared = prepared ? 1 : 0;
	dth->dth_aborted = 0;
	dth->dth_already = 0;
	dth->dth_need_validation = 0;

	dth->dth_dti_cos = dti_cos;
	dth->dth_dti_cos_count = dti_cos_cnt;
	dth->dth_ent = NULL;
	dth->dth_flags = leader ? DTE_LEADER : 0;

	if (sync) {
		dth->dth_flags |= DTE_BLOCK;
		dth->dth_sync = 1;
	} else {
		dth->dth_sync = 0;
	}

	dth->dth_op_seq = 0;
	dth->dth_oid_cnt = 0;
	dth->dth_oid_cap = 0;
	dth->dth_oid_array = NULL;

	dth->dth_dkey_hash = 0;

	if (daos_is_zero_dti(dti))
		return 0;

	if (!dtx_epoch_chosen(epoch)) {
		D_ERROR("initializing DTX "DF_DTI" with invalid epoch: value="
			DF_U64" first="DF_U64" flags=%x\n",
			DP_DTI(dti), epoch->oe_value, epoch->oe_first,
			epoch->oe_flags);
		return -DER_INVAL;
	}
	dth->dth_epoch = epoch->oe_value;
	dth->dth_epoch_bound = dtx_epoch_bound(epoch);

	if (dth->dth_modification_cnt == 0)
		return 0;

	return vos_dtx_rsrvd_init(dth);
}

void
dtx_renew_epoch(struct dtx_epoch *epoch, struct dtx_handle *dth)
{
	dth->dth_epoch = epoch->oe_value;
	dth->dth_epoch_bound = dtx_epoch_bound(epoch);
}

#ifndef XXX_NO_LEADER
/**
 * Prepare the leader DTX handle in DRAM.
 *
 * \param coh		[IN]	Container handle.
 * \param dti		[IN]	The DTX identifier.
 * \param epoch		[IN]	Epoch for the DTX.
 * \param sub_modification_cnt
 *			[IN]	Sub modifications count
 * \param pm_ver	[IN]	Pool map version for the DTX.
 * \param leader_oid	[IN]	The object ID is used to elect the DTX leader.
 * \param dti_cos	[IN]	The DTX array to be committed because of shared.
 * \param dti_cos_cnt	[IN]	The @dti_cos array size.
 * \param tgts		[IN]	targets for distribute transaction.
 * \param tgt_cnt	[IN]	number of targets (not count the leader itself).
 * \param flags		[IN]	See dtx_flags.
 * \param mbs		[IN]	DTX participants information.
 * \param p_dlh		[OUT]	Pointer to the DTX handle.
 *
 * \return			Zero on success, negative value if error.
 */
int
dtx_leader_begin(daos_handle_t coh, struct dtx_id *dti,
		 struct dtx_epoch *epoch, uint16_t sub_modification_cnt,
		 uint32_t pm_ver, daos_unit_oid_t *leader_oid,
		 struct dtx_id *dti_cos, int dti_cos_cnt,
		 struct daos_shard_tgt *tgts, int tgt_cnt, uint32_t flags,
		 struct dtx_memberships *mbs, struct dtx_leader_handle **p_dlh)
{
	struct dtx_leader_handle	*dlh;
	struct dtx_handle		*dth;
	int				 rc;
	int				 i;

	D_ALLOC(dlh, sizeof(*dlh) + sizeof(struct dtx_sub_status) * tgt_cnt);
	if (dlh == NULL)
		return -DER_NOMEM;

	if (tgt_cnt > 0) {
		dlh->dlh_future = ABT_FUTURE_NULL;
		dlh->dlh_subs = (struct dtx_sub_status *)(dlh + 1);
		for (i = 0; i < tgt_cnt; i++) {
			dlh->dlh_subs[i].dss_tgt = tgts[i];
			if (unlikely(tgts[i].st_flags & DTF_DELAY_FORWARD))
				dlh->dlh_delay_sub_cnt++;
		}
		dlh->dlh_normal_sub_cnt = tgt_cnt - dlh->dlh_delay_sub_cnt;
	}

	dth = &dlh->dlh_handle;
	rc = dtx_handle_init(dti, coh, epoch, sub_modification_cnt, pm_ver,
			     leader_oid, dti_cos, dti_cos_cnt, mbs, true,
			     (flags & DTX_SOLO) ? true : false,
			     (flags & DTX_SYNC) ? true : false,
			     (flags & DTX_DIST) ? true : false,
			     (flags & DTX_FOR_MIGRATION) ? true : false, false,
			     (flags & DTX_RESEND) ? true : false,
			     (flags & DTX_PREPARED) ? true : false,
			     (flags & DTX_DROP_CMT) ? true : false, dth);
	if (rc == 0 && sub_modification_cnt > 0)
		rc = vos_dtx_attach(dth, false, (flags & DTX_PREPARED) ? true : false);

	D_DEBUG(DB_IO, "Start DTX "DF_DTI" sub modification %d, ver %u, leader "
		DF_UOID", dti_cos_cnt %d, tgt_cnt %d, flags %x: "DF_RC"\n",
		DP_DTI(dti), sub_modification_cnt, dth->dth_ver,
		DP_UOID(*leader_oid), dti_cos_cnt, tgt_cnt, flags, DP_RC(rc));

	if (rc != 0)
		D_FREE(dlh);
	else
		*p_dlh = dlh;

	return rc;
}
#endif /* XXX_NO_LEADER */

/**
 * Prepare the DTX handle in DRAM.
 *
 * \param coh		[IN]	Container handle.
 * \param dti		[IN]	The DTX identifier.
 * \param epoch		[IN]	Epoch for the DTX.
 * \param sub_modification_cnt
 *			[IN]	Sub modifications count.
 * \param pm_ver	[IN]	Pool map version for the DTX.
 * \param leader_oid	[IN]    The object ID is used to elect the DTX leader.
 * \param dti_cos	[IN]	The DTX array to be committed because of shared.
 * \param dti_cos_cnt	[IN]	The @dti_cos array size.
 * \param flags		[IN]	See dtx_flags.
 * \param mbs		[IN]	DTX participants information.
 * \param p_dth		[OUT]	Pointer to the DTX handle.
 *
 * \return			Zero on success, negative value if error.
 */
int
dtx_begin(daos_handle_t coh, struct dtx_id *dti,
	  struct dtx_epoch *epoch, uint16_t sub_modification_cnt,
	  uint32_t pm_ver, daos_unit_oid_t *leader_oid,
	  struct dtx_id *dti_cos, int dti_cos_cnt, uint32_t flags,
	  struct dtx_memberships *mbs, struct dtx_handle **p_dth)
{
	struct dtx_handle	*dth;
	int			 rc;

	D_ALLOC(dth, sizeof(*dth));
	if (dth == NULL)
		return -DER_NOMEM;

	rc = dtx_handle_init(dti, coh, epoch, sub_modification_cnt,
			     pm_ver, leader_oid, dti_cos, dti_cos_cnt, mbs,
			     false, false, false,
			     (flags & DTX_DIST) ? true : false,
			     (flags & DTX_FOR_MIGRATION) ? true : false,
			     (flags & DTX_IGNORE_UNCOMMITTED) ? true : false,
			     (flags & DTX_RESEND) ? true : false, false, false, dth);
	if (rc == 0 && sub_modification_cnt > 0)
		rc = vos_dtx_attach(dth, false, false);

	D_DEBUG(DB_IO, "Start DTX "DF_DTI" sub modification %d, ver %u, "
		"dti_cos_cnt %d, flags %x: "DF_RC"\n",
		DP_DTI(dti), sub_modification_cnt,
		dth->dth_ver, dti_cos_cnt, flags, DP_RC(rc));

	if (rc != 0)
		D_FREE(dth);
	else
		*p_dth = dth;

	return rc;
}

static void
dtx_shares_fini(struct dtx_handle *dth)
{
	struct dtx_share_peer	*dsp;

	if (!dth->dth_shares_inited)
		return;

	while ((dsp = d_list_pop_entry(&dth->dth_share_cmt_list,
				       struct dtx_share_peer,
				       dsp_link)) != NULL)
		dtx_dsp_free(dsp);

	while ((dsp = d_list_pop_entry(&dth->dth_share_abt_list,
				       struct dtx_share_peer,
				       dsp_link)) != NULL)
		dtx_dsp_free(dsp);

	while ((dsp = d_list_pop_entry(&dth->dth_share_act_list,
				       struct dtx_share_peer,
				       dsp_link)) != NULL)
		dtx_dsp_free(dsp);

	while ((dsp = d_list_pop_entry(&dth->dth_share_tbd_list,
				       struct dtx_share_peer,
				       dsp_link)) != NULL)
		dtx_dsp_free(dsp);

	dth->dth_share_tbd_count = 0;
}

#ifndef XXX_NO_LEADER

/**
 * Stop the leader thandle.
 *
 * \param dlh		[IN]	The DTX handle on leader node.
 * \param cont		[IN]	Per-thread container cache.
 * \param result	[IN]	Operation result.
 *
 * \return			Zero on success, negative value if error.
 */
int
dtx_leader_end(struct dtx_leader_handle *dlh, struct ds_cont_hdl *coh, int result)
{
	struct ds_cont_child		*cont = coh->sch_cont;
	struct dtx_handle		*dth = &dlh->dlh_handle;
	struct dtx_entry		*dte;
	struct dtx_memberships		*mbs;
	size_t				 size;
	uint32_t			 flags;
	int				 status = -1;
	int				 rc = 0;
	bool				 aborted = false;
	bool				 unpin = false;

	D_ASSERT(cont != NULL);

	dtx_shares_fini(dth);

	if (daos_is_zero_dti(&dth->dth_xid) || unlikely(result == -DER_ALREADY))
		goto out;

	if (unlikely(coh->sch_closed)) {
		D_ERROR("Cont hdl "DF_UUID" is closed/evicted unexpectedly\n",
			DP_UUID(coh->sch_uuid));
		if (result == -DER_AGAIN || result == -DER_INPROGRESS || result == -DER_TIMEDOUT ||
		    result == -DER_STALE || daos_crt_network_error(result))
			result = -DER_IO;
		goto abort;
	}

	/* For solo transaction, the validation has already been processed inside vos
	 * when necessary. That is enough, do not need to revalid again.
	 */
	if (dth->dth_solo)
		goto out;

	if (dth->dth_need_validation) {
		/* During waiting for bulk data transfer or other non-leaders, the DTX
		 * status may be changes by others (such as DTX resync or DTX refresh)
		 * by race. Let's check it before handling the case of 'result < 0' to
		 * avoid aborting 'ready' one.
		 */
		status = vos_dtx_validation(dth);
		if (unlikely(status == DTX_ST_COMMITTED || status == DTX_ST_COMMITTABLE ||
			     status == DTX_ST_COMMITTING))
			D_GOTO(out, result = -DER_ALREADY);
	}

	if (result < 0)
		goto abort;

	switch (status) {
	case -1:
		break;
	case DTX_ST_PREPARED:
		if (likely(!dth->dth_aborted))
			break;
		/* Fall through */
	case DTX_ST_INITED:
	case DTX_ST_PREPARING:
		aborted = true;
		result = -DER_AGAIN;
		goto out;
	case DTX_ST_ABORTED:
	case DTX_ST_ABORTING:
		aborted = true;
		result = -DER_INPROGRESS;
		goto out;
	default:
		D_ASSERTF(0, "Unexpected DTX "DF_DTI" status %d\n", DP_DTI(&dth->dth_xid), status);
	}

	if ((!dth->dth_active && dth->dth_dist) || dth->dth_prepared || dtx_batched_ult_max == 0) {
		/* We do not know whether some other participants have
		 * some active entry for this DTX, consider distributed
		 * transaction case, the other participants may execute
		 * different operations. Sync commit the DTX for safe.
		 */
		dth->dth_sync = 1;
		goto sync;
	}

	/* For standalone modification, if leader modified nothing, then
	 * non-leader(s) must be the same, unpin the DTX via dtx_abort().
	 */
	if (!dth->dth_active) {
		unpin = true;
		D_GOTO(abort, result = 0);
	}

	if (DAOS_FAIL_CHECK(DAOS_DTX_SKIP_PREPARE))
		D_GOTO(abort, result = 0);

	if (DAOS_FAIL_CHECK(DAOS_DTX_MISS_ABORT))
		D_GOTO(abort, result = -DER_IO);

	if (DAOS_FAIL_CHECK(DAOS_DTX_MISS_COMMIT))
		dth->dth_sync = 1;

	/* For synchronous DTX, do not add it into CoS cache, otherwise,
	 * we may have no way to remove it from the cache.
	 */
	if (dth->dth_sync)
		goto sync;

	D_ASSERT(dth->dth_mbs != NULL);

	size = sizeof(*dte) + sizeof(*mbs) + dth->dth_mbs->dm_data_size;
	D_ALLOC(dte, size);
	if (dte == NULL) {
		dth->dth_sync = 1;
		goto sync;
	}

	mbs = (struct dtx_memberships *)(dte + 1);
	memcpy(mbs, dth->dth_mbs, size - sizeof(*dte));

	dte->dte_xid = dth->dth_xid;
	dte->dte_ver = dth->dth_ver;
	dte->dte_refs = 1;
	dte->dte_mbs = mbs;

	/* Use the new created @dte instead of dth->dth_dte that will be
	 * released after dtx_leader_end().
	 */

	if (!(mbs->dm_flags & DMF_SRDG_REP))
		flags = DCF_EXP_CMT;
	else if (dth->dth_modify_shared)
		flags = DCF_SHARED;
	else
		flags = 0;
	rc = dtx_add_cos(cont, dte, &dth->dth_leader_oid,
			 dth->dth_dkey_hash, dth->dth_epoch, flags);
	dtx_entry_put(dte);
	if (rc == 0) {
		if (!DAOS_FAIL_CHECK(DAOS_DTX_NO_COMMITTABLE)) {
			vos_dtx_mark_committable(dth);
			if (cont->sc_dtx_committable_count >
			    DTX_THRESHOLD_COUNT) {
				struct dss_module_info	*dmi;

				dmi = dss_get_module_info();
				sched_req_wakeup(dmi->dmi_dtx_cmt_req);
			}
		}
	} else {
		dth->dth_sync = 1;
	}

sync:
	if (dth->dth_sync) {
		/*
		 * TBD: We need to reserve some space to guarantee that the local commit can be
		 *	done successfully. That is not only for sync commit, but also for async
		 *	batched commit.
		 */
		vos_dtx_mark_committable(dth);
		dte = &dth->dth_dte;
		rc = dtx_commit(cont, &dte, NULL, 1);
		if (rc != 0)
			D_WARN(DF_UUID": Fail to sync commit DTX "DF_DTI": "DF_RC"\n",
			       DP_UUID(cont->sc_uuid), DP_DTI(&dth->dth_xid), DP_RC(rc));

		/*
		 * NOTE: The semantics of 'sync' commit does not guarantee that all
		 *	 participants of the DTX can commit it on each local target
		 *	 successfully, instead, we try to commit the DTX immediately
		 *	 after all participants claiming 'prepared'. But even if we
		 *	 failed to commit it, we will not rollback the commit since
		 *	 the DTX has been marked as 'committable' and may has been
		 *	 accessed by others. The subsequent dtx_cleanup logic will
		 *	 handle (re-commit) current failed commit.
		 */
		D_GOTO(out, result = 0);
	}

abort:
	/* If some remote participant ask retry. We do not make such participant
	 * to locally retry for avoiding related forwarded RPC timeout, instead,
	 * The leader will trigger retry globally without abort 'prepared' ones.
	 */
	if (unpin || (result < 0 && result != -DER_AGAIN && !dth->dth_solo)) {
		/* 1. Drop partial modification for distributed transaction.
		 * 2. Remove the pinned DTX entry.
		 */
		vos_dtx_cleanup(dth, true);
		dtx_abort(cont, &dth->dth_dte, dth->dth_epoch);
		aborted = true;
	}

out:
	if (unlikely(result == -DER_ALREADY))
		result = 0;

	if (!daos_is_zero_dti(&dth->dth_xid)) {
		if (result < 0) {
			/* 1. Drop partial modification for distributed transaction.
			 * 2. Remove the pinned DTX entry.
			 */
			if (!aborted)
				vos_dtx_cleanup(dth, true);

			/* For solo DTX, just let client retry for DER_AGAIN case. */
			if (result == -DER_AGAIN && dth->dth_solo)
				result = -DER_INPROGRESS;
		}

		vos_dtx_rsrvd_fini(dth);
		vos_dtx_detach(dth);
	}

	D_ASSERTF(result <= 0, "unexpected return value %d\n", result);

	/* If piggyback DTX has been done everywhere, then need to handle CoS cache.
	 * It is harmless to keep some partially committed DTX entries in CoS cache.
	 */
	if (result == 0 && dth->dth_cos_done) {
		int	i;

		for (i = 0; i < dth->dth_dti_cos_count; i++)
			dtx_del_cos(cont, &dth->dth_dti_cos[i],
				    &dth->dth_leader_oid, dth->dth_dkey_hash);
	}

	D_DEBUG(DB_IO, "Stop the DTX "DF_DTI" ver %u, dkey %lu, %s, cos %d/%d: result "DF_RC"\n",
		DP_DTI(&dth->dth_xid), dth->dth_ver, (unsigned long)dth->dth_dkey_hash,
		dth->dth_sync ? "sync" : "async", dth->dth_dti_cos_count,
		dth->dth_cos_done ? dth->dth_dti_cos_count : 0, DP_RC(result));

	D_FREE(dth->dth_oid_array);
	D_FREE(dlh);

	return result;
}
#endif /* XXX_NO_LEADER */

int
dtx_end(struct dtx_handle *dth, struct ds_cont_child *cont, int result)
{
	D_ASSERT(dth != NULL);

	dtx_shares_fini(dth);

	if (daos_is_zero_dti(&dth->dth_xid))
		goto out;

	if (result < 0) {
		if (dth->dth_dti_cos_count > 0 && !dth->dth_cos_done) {
			int	rc;

			/* NOTE: For non-leader participant, even if we fail to make
			 *	 related modification for some reason, we still need
			 *	 to commit the piggyback DTXs those may have already
			 *	 been committed on other participants.
			 *	 For leader case, it is not important even if we fail
			 *	 to commit them, because they are still in CoS cache,
			 *	 and can be committed next time.
			 */
			rc = vos_dtx_commit(cont->sc_hdl, dth->dth_dti_cos,
					    dth->dth_dti_cos_count, NULL);
			if (rc < 0)
				D_ERROR(DF_UUID": Fail to DTX CoS commit: %d\n",
					DP_UUID(cont->sc_uuid), rc);
			else
				dth->dth_cos_done = 1;
		}

		/* 1. Drop partial modification for distributed transaction.
		 * 2. Remove the pinned DTX entry.
		 */
		vos_dtx_cleanup(dth, true);
	}

	D_DEBUG(DB_IO,
		"Stop the DTX "DF_DTI" ver %u, dkey %lu: "DF_RC"\n",
		DP_DTI(&dth->dth_xid), dth->dth_ver,
		(unsigned long)dth->dth_dkey_hash, DP_RC(result));

	D_ASSERTF(result <= 0, "unexpected return value %d\n", result);

	vos_dtx_rsrvd_fini(dth);
	vos_dtx_detach(dth);

out:
	D_FREE(dth->dth_oid_array);
	D_FREE(dth);

	return result;
}
