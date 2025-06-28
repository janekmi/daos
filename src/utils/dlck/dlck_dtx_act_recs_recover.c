/**
 * (C) Copyright 2025 Hewlett Packard Enterprise Development LP
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#include <stdlib.h>
#include <stdio.h>
#include <daos/mem.h>
#include <daos/btree_class.h>
#include <gurt/telemetry_producer.h>
#include <daos_srv/vos.h>
#include <daos_srv/dlck.h>
#include <daos_srv/daos_mgmt_srv.h>
#include <daos_version.h>

#include <libpmemobj.h>

#include "dlck_args.h"
#include "dlck_engine.h"
#include "dlck_common.h"


// const char pool_uuid[] = "3676cebe-bc38-4add-b2a6-bc2025f7e277";
// const char pool2_uuid[] = "07e9e5fb-4388-4e81-9d07-cdd139899739";
// const char cont2_uuid[] = "001a010c-4b51-4855-a5cb-fbf582b37000";

struct entry {
	d_list_t link;
	uuid_t   uuid;
};

/**
 * Just add the container's UUID to the provided array.
 */
static int
cont_list(daos_handle_t ih, vos_iter_entry_t *entry, vos_iter_type_t type, vos_iter_param_t *param,
	  void *cb_arg, unsigned int *acts)
{
	d_list_t     *co_uuids = cb_arg;
	struct entry *ent;
	D_ALLOC_PTR(ent);
	if (ent == NULL) {
		return ENOMEM;
	}
	uuid_copy(ent->uuid, entry->ie_couuid);
	d_list_add(&ent->link, co_uuids);
	return 0;
}

void test(daos_handle_t poh)
{
	d_list_t                co_uuids = D_LIST_HEAD_INIT(co_uuids);
	struct entry           *ent;
	int                     num = 0;

	/** loop over containers */
	vos_iter_param_t        param   = {0};
	struct vos_iter_anchors anchors = {0};
	int rc;

	param.ip_hdl        = poh;
	param.ip_epr.epr_hi = DAOS_EPOCH_MAX;
	param.ip_flags      = VOS_IT_FOR_CHECK;

	rc =
	    vos_iterate(&param, VOS_ITER_COUUID, false, &anchors, cont_list, NULL, &co_uuids, NULL);
	assert(rc == 0);

	d_list_for_each_entry(ent, &co_uuids, link) {
		++num;
	}
	assert(num > 0);

	// assert(co_uuids.da_len > 0);

	// char uuid_str[UUID_STR_LEN];
	// uuid_unparse(dlck_array_entry(&co_uuids, 0), uuid_str);
	// printf("%s\n", uuid_str);
}
struct xstream_arg {
	struct dlck_args *args;
	struct dlck_xstream *xs;
	ABT_mutex *open_mtx;
	int rc;
};

static void
exec_one(void *arg)
{
	struct xstream_arg *xa = arg;
	struct dlck_file *file;
	daos_handle_t poh;
	int rc;

	rc = dlck_engine_xstream_init(xa->xs);
	if (rc != 0) {
		xa->rc = rc;
		return;
	}

	d_list_for_each_entry(file, &xa->args->common.files, link) {
		if ((file->targets & (1 << xa->xs->tgt_id)) == 0) {
			continue;
		}

		ABT_mutex_lock(*xa->open_mtx);
		rc = dlck_pool_open(xa->args->common.storage_path, file, xa->xs->tgt_id, &poh);
		ABT_mutex_unlock(*xa->open_mtx);
		if (rc != 0) {
			xa->rc = rc;
			return;
		}
		
		test(poh);

		ABT_mutex_lock(*xa->open_mtx);
		rc = vos_pool_close(poh);
		if (rc != 0) {
			xa->rc = rc;
			return;
		}
		ABT_mutex_unlock(*xa->open_mtx);
	}

	rc = dlck_engine_xstream_fini(xa->xs);
	if (rc != 0) {
		xa->rc = rc;
		return;
	}

	return;
}

/**
 * XXX error handling
 */
static int
exec_all(struct dlck_args *args, struct dlck_engine *engine)
{
	ABT_mutex open_mtx;
	struct dlck_ult *ults;
	struct xstream_arg *xargs;
	struct xstream_arg *xa;
	int rc;

	rc = ABT_mutex_create(&open_mtx);
	if (rc != 0) {
		return rc;
	}

	D_ALLOC_ARRAY(ults, engine->targets);
	if (ults == NULL) {
		return ENOMEM;
	}

	D_ALLOC_ARRAY(xargs, engine->targets);
	if (xargs == NULL) {
		return ENOMEM;
	}

	for (int i = 0; i < engine->targets; ++i) {
		/** prepare arguments */
		xa = &xargs[i];
		xa->args = args;
		xa->xs = &engine->xss[i];
		xa->open_mtx = &open_mtx;

		/** start an ULT */
		rc = dlck_ult_create(engine->xss[i].pool, exec_one, xa, &ults[i]);
		if (rc != 0) {
			return rc;
		}
	}

	for (int i = 0; i < engine->targets; ++i) {
		rc= ABT_thread_join(ults[i].thread);
		if (rc != 0) {
			return rc;
		}
		
		rc = ABT_thread_free(&ults[i].thread);
		if (rc != 0) {
			return rc;
		}
	}

	D_FREE(xargs);
	D_FREE(ults);

	rc = ABT_mutex_free(&open_mtx);
	if (rc != 0) {
		return rc;
	}

	return 0;
}

/**
 * XXX teardown
 */
static int
pool_mkdir_all(struct dlck_args *args, struct dlck_engine *engine)
{
	struct dlck_file *file;
	int rc;

	d_list_for_each_entry(file, &args->common.files, link) {
		rc = dlck_pool_mkdir(args->common.storage_path, file);
		if (rc != 0) {
			return rc;
		}
	}

	return 0;
}

/**
 * XXX teardown
 */
int
dlck_dtx_act_recs_recover(struct dlck_args *args)
{
	struct dlck_engine *engine = NULL;
	int rc;

	rc = dlck_engine_start(args, &engine);
	if (rc != 0) {
		return rc;
	}

	rc = pool_mkdir_all(args, engine);
	if (rc != 0) {
		return rc;
	}
	
	rc = exec_all(args, engine);
	if (rc != 0) {
		return rc;
	}
	
	rc = dlck_engine_stop(engine);
	if (rc != 0) {
		return rc;
	}

	return 0;
}
