/**
 * (C) Copyright 2019-2023 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
/**
 * dtx: XXX
 */
#include <daos_types.h>
#include <daos_srv/dtx_srv.h>
#include <daos_srv/vos.h>

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
