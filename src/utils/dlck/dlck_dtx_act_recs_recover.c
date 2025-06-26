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
#include <daos_version.h>

#include <libpmemobj.h>

#include "dlck_args.h"

#define LONG_SLEEP (60 * 30) /** 30 minutes */

int
xxx_vos_preallocate(const char *path, uuid_t uuid, daos_size_t scm_size);

#define CO_UUIDS_GROW_BY 10

static unsigned int flags =
    VOS_POF_EXCL | VOS_POF_EXTERNAL_FLUSH | VOS_POF_FOR_FEATURE_FLAG;

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

struct xstream_t {
	ABT_xstream xstream;
	ABT_pool pool;
	ABT_thread thread;
	void *thread_arg;
};

#define XSTREAMS_MAX 10

struct xstream_t Xstreams[XSTREAMS_MAX];
int xstream_id;

#define DSS_DEEP_STACK_SZ 65536

static void
start_ult_in_existing_pool(void (*ult_func)(void *), void *arg, ABT_pool pool, bool deep_stack, ABT_thread *ult)
{
	ABT_thread_attr attr;
	int rc;

	if (deep_stack) {
		rc = ABT_thread_attr_create(&attr);
		assert(rc == 0);
		rc = ABT_thread_attr_set_stacksize(attr, DSS_DEEP_STACK_SZ);
		assert(rc == 0);
	} else {
		attr = ABT_THREAD_ATTR_NULL;
	}

	rc = ABT_thread_create(pool, ult_func, arg, attr, ult);
	assert(rc == 0);
	
	if (deep_stack) {
		ABT_thread_attr_free(&attr);
		assert(rc == 0);
	}	
}

static void
start_ult(void (*thread_func)(void *), void *arg, bool deep_stack)
{
	assert(xstream_id < XSTREAMS_MAX);
	struct xstream_t *xs = &Xstreams[xstream_id++];
	
	int rc;

	xs->thread_arg = arg;

	rc = ABT_xstream_create(ABT_SCHED_NULL, &xs->xstream);
	assert(rc == 0);
	rc = ABT_xstream_get_main_pools(xs->xstream, 1, &xs->pool);
	assert(rc == 0);

	start_ult_in_existing_pool(thread_func, xs, xs->pool, deep_stack, &xs->thread);
}

static void
nvme_polling(void *arg)
{
	struct bio_xs_context *xsctx = arg;
	int rc;

	do {
		rc = bio_nvme_poll(xsctx);
		if (rc != 0) {
			printf("nvme_polling: bio_nvme_poll -> %d\n", rc);
		}
		ABT_thread_yield();
	} while(true);
}

ABT_mutex ULT_mutex;
ABT_cond ULT_barrier;

static void
abt_signal(void)
{
	ABT_mutex_lock(ULT_mutex);
	ABT_cond_signal(ULT_barrier);
	ABT_mutex_unlock(ULT_mutex);
}

static void
abt_wait(void)
{
	ABT_mutex_lock(ULT_mutex);
	ABT_cond_wait(ULT_barrier, ULT_mutex);
	ABT_mutex_unlock(ULT_mutex);
}

static void
system_service_ult(void *unused)
{
	struct dss_module_info *dmi;
	int tag = DAOS_SERVER_TAG - DAOS_TGT_TAG;
	int xs_id = 0;
	int tgt_id = -1;
	int rc;

	(void) dss_tls_init(tag, xs_id, tgt_id);

	pthread_setname_np(pthread_self(), "daos_sys_0");

	dmi = dss_get_module_info();
	assert(dmi != NULL);

	rc = bio_xsctxt_alloc(&dmi->dmi_nvme_ctxt, BIO_SYS_TGT_ID, false);
	assert(rc == 0);

	abt_signal();

	do {
		rc = bio_nvme_poll(dmi->dmi_nvme_ctxt);
		assert(rc != 1); /** work was done? */
	} while(true);
}

static void
start_system_service()
{
	start_ult(system_service_ult, NULL, true);

	abt_wait();
}

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

unsigned int dss_sys_xs_nr = 3;

/** main XS id of (vos) tgt_id */
#define DSS_MAIN_XS_ID(tgt_id) ((tgt_id) + dss_sys_xs_nr)

struct thread_args {
	char *path;
	int tgt_id;
};

#define DSS_IO_XS_NAME_FMT "daos_io_%d"

static void
xstream_test(void *arg)
{
	struct xstream_t *xs = arg;
	struct thread_args *targs = xs->thread_arg;
	uuid_t uuid = {0};
	daos_handle_t poh     = DAOS_HDL_INVAL;
	int rc;

	/**
	 * for xstream:
	 * - dss_tls_init
	 * - bio_xsctxt_alloc
	 */

	struct dss_module_info *dmi;
	int tag = DAOS_SERVER_TAG;
	int xs_id = DSS_MAIN_XS_ID(targs->tgt_id);
	int tgt_id = targs->tgt_id;

	(void) dss_tls_init(tag, xs_id, tgt_id);

	char name[DSS_XS_NAME_LEN];
	rc = snprintf(name, DSS_XS_NAME_LEN, DSS_IO_XS_NAME_FMT, tgt_id);
	assert(rc >= 0);
	pthread_setname_np(pthread_self(), name);

	dmi = dss_get_module_info();
	assert(dmi != NULL);

	rc = bio_xsctxt_alloc(&dmi->dmi_nvme_ctxt, tgt_id, false);
	assert(rc == 0);

	start_ult_in_existing_pool(nvme_polling, dmi->dmi_nvme_ctxt, xs->pool, true, NULL);
	
	/** attempt opening the pool */
	rc = uuid_parse(pool_uuid, uuid);
	assert(rc == 0);
	
	rc = dlck_recreate(targs->path, uuid);
	assert(rc == 0);

	rc = vos_pool_open(targs->path, uuid, flags, &poh);
	assert(rc == 0);

	/** one of the pools will get an additional container */
	if (tgt_id == 1) {
		rc = uuid_parse(cont2_uuid, uuid);
		assert(rc == 0);
		rc = vos_cont_create(poh, uuid);
		assert(rc == 0);
	}

	xxx(poh);

	abt_signal();

	sleep(LONG_SLEEP);
}

static void
xstream_all_ult(struct dlck_args *args)
{
	struct thread_args targs;

	targs.path = args->files[0];
	targs.tgt_id = 0;
	start_ult(xstream_test, &targs, true);
	abt_wait();

	targs.path = args->files[1];
	targs.tgt_id = 1;
	start_ult(xstream_test, &targs, true);
	abt_wait();

	targs.path = args->files[2];
	targs.tgt_id = 2;
	start_ult(xstream_test, &targs, true);
	abt_wait();
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
	 * dss_xstreams_init:
	 * - start system service XS
	 * - start main IO service XS
	 */

	const unsigned int instance_idx = 0;
	int tag = DAOS_SERVER_TAG - DAOS_TGT_TAG;
	unsigned int targets = 4;
	rc = d_tm_init(instance_idx, dlck_metrics_region_size(targets), D_TM_SERVER_PROCESS);
	assert(rc == 0);

	rc = register_dbtree_classes();
	assert(rc == 0);

	rc = ABT_init(0, NULL);
	assert(rc == ABT_SUCCESS);
	rc = ABT_mutex_create(&ULT_mutex);
	assert(rc == ABT_SUCCESS);
	rc = ABT_cond_create(&ULT_barrier);
	assert(rc == ABT_SUCCESS);
	
	int numa_node = 0;
	int mem_size = 5120;
	unsigned int hugepage_size = 2;
	bool bypass_health_chk = false;
	rc = bio_nvme_init(nvme_conf, numa_node, mem_size, hugepage_size, targets, bypass_health_chk);
	assert(rc == 0);
	
	dss_register_key(&daos_srv_modkey);
	dss_register_key(&vos_module_key);
	rc = vos_srv_module.sm_init();
	assert(rc == 0);
	
	rc = vos_standalone_tls_init(tag);
	assert(rc == 0);
	
	rc = dlck_sys_db_init(sys_db_path);
	assert(rc == 0);

	/** start system service XS */
	start_system_service();
	
	/** try xstream 0 and 1 */
	
	xstream_all_ult(args);
	(void) xstream_all_ult;

	sleep(LONG_SLEEP);

	return 0;
}
