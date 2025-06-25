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
#define VOS_STANDALONE
#include <daos_srv/vos.h>
#include <daos_srv/dlck.h>
#include <daos_version.h>

#include <libpmemobj.h>

#include "dlck_args.h"

int
xxx_vos_preallocate(const char *path, uuid_t uuid, daos_size_t scm_size);

#define DB_PATH_TEMPLATE "/tmp/dlck_XXXXXX"
#define CO_UUIDS_GROW_BY 10

static unsigned int flags =
    VOS_POF_EXCL | VOS_POF_EXTERNAL_FLUSH | VOS_POF_FOR_FEATURE_FLAG;

/**
 * Just add the container's UUID to the provided array.
 */
static int
cont_list(daos_handle_t ih, vos_iter_entry_t *entry, vos_iter_type_t type, vos_iter_param_t *param,
	  void *cb_arg, unsigned int *acts)
{
	struct dlck_array *co_uuids = cb_arg;
	return dlck_array_append(co_uuids, entry->ie_couuid);
}


#define XSTREAM_MAX 5 /** 4 + sys */

#define XSTREAM_SYS (XSTREAM_MAX - 1)

int Tgt_id = 0;
struct bio_xs_context *XSCTXS[XSTREAM_MAX] = {NULL};
void *TLSS[XSTREAM_MAX] = {NULL};

struct vos_tls *
dlck_tls_get()
{
	return TLSS[Tgt_id];
}

struct bio_xs_context *
dlck_xsctx_get()
{
	return XSCTXS[Tgt_id];
}

// void *
// thread_function(void *arg)
// {
// 	struct dlck_array da  = {0};
// 	struct dlck_stats ds  = {0};
// 	daos_handle_t    *coh = (daos_handle_t *)arg;
// 	int               rc;

// 	dlck_array_init(sizeof(struct dlck_dtx_rec), 10, &da);

// 	TLSS[0] = vos_standalone_tls_alloc(DAOS_TGT_TAG);

// 	rc = dlck_vos_cont_rec_get_active(*coh, &da, &ds);
// 	assert(rc == 0);

// 	printf("dda.touched = %u\n", ds.touched);

// 	vos_standalone_tls_free(TLSS[0]);

// 	return NULL;
// }

#define PMEMOBJ_CTL_SDS_AT_CREATE_NAME "sds.at_create"

static int
env_prep(void)
{
	int enabled = 0; /** disabled */
	return pmemobj_ctl_set(NULL, PMEMOBJ_CTL_SDS_AT_CREATE_NAME, &enabled);
}

static int
register_dbtree_classes(void)
{
	int rc;

	rc = dbtree_class_register(DBTREE_CLASS_IFV, BTR_FEAT_UINT_KEY | BTR_FEAT_DIRECT_KEY,
				   &dbtree_ifv_ops);
	if (rc != 0) {
		return rc;
	}

	return DER_SUCCESS;
}

int
dlck_sys_db_init(const char *sys_db_path)
{
	int	 rc;

	rc = vos_db_init(bio_nvme_configured(SMD_DEV_TYPE_META) ? sys_db_path : "XXX");
	if (rc != 0) {
		return rc;
	}

	rc = smd_init(vos_db_get());
	if (rc != 0) {
		vos_db_fini();
	}

	return rc;
}

static int
dlck_recreate(const char *path, uuid_t uuid)
{
	struct smd_pool_info *pool_info = NULL;
	int rc;

	rc = smd_pool_get_info(uuid, &pool_info);
	if (rc != 0) {
		return rc;
	}

	rc = xxx_vos_preallocate(path, uuid, pool_info->spi_scm_sz);
	if (rc != 0) {
		goto out;
	}
	
out:
	smd_pool_free_info(pool_info);

	return rc;
}

const char pool_uuid[] = "3676cebe-bc38-4add-b2a6-bc2025f7e277";
const char cont2_uuid[] = "001a010c-4b51-4855-a5cb-fbf582b37000";

static void
nvme_polling(void *unused)
{
	do {
		for (int i = 0; i < XSTREAM_MAX; ++i) {
			struct bio_xs_context *xsctx = XSCTXS[i];
			if (xsctx == NULL) {
				continue;
			}

			bio_nvme_poll(xsctx);
		}
	} while(true);
}

static int
start_nvme_polling(void)
{
	// pthread_t thread_id;
	int rc;
	
	// rc = pthread_create(&thread_id, NULL, nvme_polling, NULL);
        // assert(rc == 0);

	ABT_xstream xstream;
	ABT_pool pool;
	ABT_thread thread;

	rc = ABT_xstream_create(ABT_SCHED_NULL, &xstream);
	assert(rc == 0);
	ABT_xstream_get_main_pools(xstream, 1, &pool);
	assert(rc == 0);
	ABT_thread_create(pool, nvme_polling, NULL, ABT_THREAD_ATTR_NULL, &thread);
	assert(rc == 0);

        return 0;
}

void xxx(daos_handle_t poh)
{
	/** loop over containers */
	vos_iter_param_t        param   = {0};
	struct vos_iter_anchors anchors = {0};
	int rc;

	param.ip_hdl        = poh;
	param.ip_epr.epr_hi = DAOS_EPOCH_MAX;
	param.ip_flags      = VOS_IT_FOR_CHECK;

	struct dlck_array co_uuids = {0};
	dlck_array_init(sizeof(uuid_t), CO_UUIDS_GROW_BY, &co_uuids);

	rc =
	    vos_iterate(&param, VOS_ITER_COUUID, false, &anchors, cont_list, NULL, &co_uuids, NULL);
	assert(rc == 0);

	assert(co_uuids.da_len > 0);

	// char uuid_str[UUID_STR_LEN];
	// uuid_unparse(dlck_array_entry(&co_uuids, 0), uuid_str);
	// printf("%s\n", uuid_str);
}

extern struct dss_module vos_srv_module;

extern struct dss_module_key vos_module_key;

static int
xstream_test(const char *path, int tgt_id)
{
	uuid_t uuid = {0};
	daos_handle_t poh     = DAOS_HDL_INVAL;
	int rc;

	/**
	 * for xstream:
	 * - dss_tls_init
	 * - bio_xsctxt_alloc
	 */

	/** prepare global state (TLS + XSCTXT) */
	TLSS[tgt_id] = vos_module_key.dmk_init(DAOS_SERVER_TAG, tgt_id, tgt_id);
	if (TLSS[tgt_id] == NULL) {
		return -1;
	}

	rc = bio_xsctxt_alloc(&XSCTXS[tgt_id], tgt_id, false);
	assert(rc == 0);
	
	Tgt_id = tgt_id;
	
	/** attempt opening the pool */
	rc = uuid_parse(pool_uuid, uuid);
	assert(rc == 0);
	
	rc = dlck_recreate(path, uuid);
	assert(rc == 0);

	rc = vos_pool_open(path, uuid, flags, &poh);
	assert(rc == 0);

	return 0;
}

static void
xstream_all_ult(void *arg)
{
	struct dlck_args *args = arg;
	int rc;

	rc = xstream_test(args->files[0], 0);
	assert(rc == 0);

	rc = xstream_test(args->files[1], 1);
	assert(rc == 0);
}

static int
xstream_wait(void *args)
{
	ABT_xstream xstream;
	ABT_pool pool;
	ABT_thread thread;
	int rc;

	rc = ABT_xstream_create(ABT_SCHED_NULL, &xstream);
	assert(rc == 0);
	ABT_xstream_get_main_pools(xstream, 1, &pool);
	assert(rc == 0);
	ABT_thread_create(pool, xstream_all_ult, args, ABT_THREAD_ATTR_NULL, &thread);
	assert(rc == 0);
	
        rc = ABT_thread_join(thread);
	assert(rc == 0);
        rc = ABT_thread_free(&thread);
	assert(rc == 0);
        rc = ABT_xstream_join(xstream);
	assert(rc == 0);
        rc = ABT_xstream_free(&xstream);
	assert(rc == 0);

	return 0;
}

static uint64_t
dlck_metrics_region_size(int num_tgts)
{
	const uint64_t	est_std_metrics = 1024; /* high estimate to allow for pool links */
	const uint64_t	est_tgt_metrics = 128; /* high estimate */

	return (est_std_metrics + est_tgt_metrics * num_tgts) * D_TM_METRIC_SIZE;
}

int
dlck_dtx_act_recs_recover(struct dlck_args *args)
{
	const char *nvme_conf = "/var/daos/config/daos_control/engine0/daos_nvme.conf";
	const char *sys_db_path = "/var/daos/config/daos_control/engine0/";
	int           rc;

	rc = env_prep();
	assert(rc == 0);
	
	/**
	 * d_tm_init
	 * register_dbtree_classes
	 * ABT_init
	 * bio_nvme_init
	 * dss_module_init_all -> vos init?
	 * vos_standalone_tls_init
	 * dss_sys_db_init
	 */

	const unsigned int instance_idx = 0;
	unsigned int targets = 4;
	rc = d_tm_init(instance_idx, dlck_metrics_region_size(targets), D_TM_SERVER_PROCESS);
	assert(rc == 0);

	rc = register_dbtree_classes();
	assert(rc == 0);

	rc = ABT_init(0, NULL);
	assert(rc == ABT_SUCCESS);
	
	int numa_node = 0;
	int mem_size = 5120;
	unsigned int hugepage_size = 2;
	bool bypass_health_chk = false;
	rc = bio_nvme_init(nvme_conf, numa_node, mem_size, hugepage_size, targets, bypass_health_chk);
	assert(rc == 0);
	
	rc = vos_srv_module.sm_init();
	assert(rc == 0);
	
	rc = vos_standalone_tls_init(DAOS_SERVER_TAG - DAOS_TGT_TAG);
	assert(rc == 0);
	
	rc = dlck_sys_db_init(sys_db_path);
	assert(rc == 0);
	
	/** NVMe polling thread */
	rc = start_nvme_polling();
	assert(rc == 0);
	
	/** detach global state */
	
	vos_tls_getter_set(dlck_tls_get, &TLSS[XSTREAM_SYS]);
	vos_xsctxt_getter_set(dlck_xsctx_get, &XSCTXS[XSTREAM_SYS]);
	
	/** try xstream 0 and 1 */
	
	// xstream_all_ult(args);

	rc = xstream_wait(args);
	assert(rc == 0);

	// db_path = temp_db_path_init();
	// if (db_path == NULL) {
	// 	return -1;
	// }

	// rc = vos_self_init(db_path, true, tgt_id);
	// if (rc) {
	// 	D_ERROR("Error initializing VOS instance");
	// 	return rc;
	// }

	// bio_nvme_fini();

	// rc = bio_nvme_init(args->files[0], 0, 5120, 2, 1, false);
	// if (rc != 0) {
	// 	return rc;
	// }

	// rc = bio_xsctxt_alloc(&XSCTX, BIO_SYS_TGT_ID, true);
	// if (rc) {
	// 	return rc;
	// }

	// void bio_xsctxt_free(struct bio_xs_context *ctxt);

	// vos_xsctxt_getter_set(dlck_xsctx_get);

	// rc = uuid_parse(pool_uuid, uuid);
	// if (rc != 0) {
	// 	return rc;
	// }
	
	// rc = vos_pool_open(args->files[0], uuid, flags, &poh);
	// const char *path = "/tmp/vos-1";
	// const char *path = "/mnt/daos/20d1ef00-c89b-4f10-9fdb-31b9b086a0b7/vos-1";

	// rc = dlck_recreate(args->files[0], uuid);
	// if (rc != 0) {
	// 	return rc;
	// }
	// (void) dlck_recreate;

	// rc = vos_pool_open(args->files[0], uuid, flags, &poh0);
	// assert(rc == 0);
	
	/** XXX */
	// rc = uuid_parse(cont2_uuid, uuid);
	// assert(rc == 0);
	
	// rc = vos_cont_create(poh, uuid);
	// assert(rc == 0);
	/** XXX */
	
	// xxx(poh0);
	
	
	/** -- */
	

	
	// TLSS[1] = vos_standalone_tls_alloc(DAOS_TGT_TAG);
	// assert(TLSS[1] != NULL);

	// rc = bio_xsctxt_alloc(&XSCTXS[1], 1, true);
	// if (rc) {
	// 	return rc;
	// }
	
	// Tgt_id = 1;
	
	// rc = dlck_recreate(args->files[1], uuid);
	// if (rc != 0) {
	// 	return rc;
	// }
	// (void) dlck_recreate;
	
	// rc = vos_pool_open(args->files[1], uuid, flags, &poh1);
	// assert(rc == 0);
	
	// xxx(poh1);
	
	// Tgt_id = 0;
	// xxx(poh0);

	// Tgt_id = 1;
	// xxx(poh1);



	// assert(uuid_is_null(args.common.co_uuid) == 0);

	// rc = vos_cont_open(poh, args.common.co_uuid, &coh);
	// assert(rc == 0);

	// vos_tls_getter_set(dlck_tls_get);

	// pthread_t thread;

	// if (pthread_create(&thread, NULL, thread_function, &coh) != 0) {
	// 	perror("Failed to create thread");
	// 	return 1;
	// }

	// Wait for the thread to complete
	// pthread_join(thread, NULL);

	// struct dlck_dtx_rec_array dda = {0};

	// rc = dlck_vos_cont_rec_get_active(coh, &dda);
	// assert(rc == 0);

	// printf("dda.touched = %u\n", dda.touched);

	return 0;
}
