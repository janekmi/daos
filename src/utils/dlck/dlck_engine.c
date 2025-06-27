/**
 * (C) Copyright 2025 Hewlett Packard Enterprise Development LP
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#include <libgen.h>
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
#include "dlck_engine.h"

#define LONG_SLEEP (60 * 30) /** 30 minutes */

int
xxx_vos_preallocate(const char *path, uuid_t uuid, daos_size_t scm_size);

// #define CO_UUIDS_GROW_BY 10

// static unsigned int flags =
//     VOS_POF_EXCL | VOS_POF_EXTERNAL_FLUSH | VOS_POF_FOR_FEATURE_FLAG;

static int
dlck_engine_alloc(struct dlck_args *args, struct dlck_engine **engine_ptr)
{
	struct dlck_engine *engine;

	D_ALLOC_PTR(engine);
	if (engine == NULL) {
		return ENOMEM;
	}

	/** each of the targets will get its own xstream + 1 for daos_sys */
	D_ALLOC_ARRAY(engine->xss, args->common.targets + 1);
	if (engine->xss == NULL) {
		D_FREE(engine);
		return ENOMEM;
	}

	engine->targets = args->common.targets;

	*engine_ptr = engine;

	return 0;
}

/**
 * XXX should be shared with the DAOS engine.
 */
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

/** XXX should be shared with the DAOS engine */
#define DSS_DEEP_STACK_SZ 65536

static int
abt_attr_default_create(ABT_thread_attr *attr)
{
	int rc;

	rc = ABT_thread_attr_create(attr);
	if (rc != 0) {
		/** XXX translate ABT return code */
		return rc;
	}
	
	rc = ABT_thread_attr_set_stacksize(*attr, DSS_DEEP_STACK_SZ);
	if (rc != 0) {
		/** XXX translate ABT return code */
		return rc;
	}

	return 0;
}

static int
dlck_abt_init(struct dlck_engine *engine)
{
	int rc;

	rc = ABT_init(0, NULL);
	if(rc != ABT_SUCCESS) {
		/** XXX translate ABT return code */
		return rc;
	}

	return 0;
}

/**
 * XXX should be shared with dss_sys_db_init().
 */
int
dlck_sys_db_init(struct dlck_args *args)
{
	int	 rc;
	char	*sys_db_path = NULL;
	char	*nvme_conf_path = NULL;

	if (!bio_nvme_configured(SMD_DEV_TYPE_META))
		goto db_init;

	if (args->common.nvme_conf == NULL) {
		D_ERROR("nvme conf path not set\n");
		return -DER_INVAL;
	}

	D_STRNDUP(nvme_conf_path, args->common.nvme_conf, PATH_MAX);
	if (nvme_conf_path == NULL)
		return -DER_NOMEM;
	D_STRNDUP(sys_db_path, dirname(nvme_conf_path), PATH_MAX);
	D_FREE(nvme_conf_path);
	if (sys_db_path == NULL)
		return -DER_NOMEM;

db_init:
	rc = vos_db_init(bio_nvme_configured(SMD_DEV_TYPE_META) ? sys_db_path : args->common.storage_path);
	if (rc)
		goto out;

	rc = smd_init(vos_db_get());
	if (rc)
		vos_db_fini();
out:
	D_FREE(sys_db_path);

	return rc;
}

// static int
// dlck_recreate(const char *path, uuid_t uuid)
// {
// 	struct smd_pool_info *pool_info = NULL;
// 	int rc;

// 	rc = smd_pool_get_info(uuid, &pool_info);
// 	if (rc != 0) {
// 		return rc;
// 	}

// 	rc = xxx_vos_preallocate(path, uuid, pool_info->spi_scm_sz);
// 	if (rc != 0) {
// 		goto out;
// 	}
	
// out:
// 	smd_pool_free_info(pool_info);

// 	return rc;
// }

const char pool_uuid[] = "3676cebe-bc38-4add-b2a6-bc2025f7e277";
const char pool2_uuid[] = "07e9e5fb-4388-4e81-9d07-cdd139899739";
const char cont2_uuid[] = "001a010c-4b51-4855-a5cb-fbf582b37000";

struct xstream_t {
	ABT_xstream xstream;
	ABT_pool pool;
	ABT_thread thread;
	void *thread_arg;
};

static int
xstream_create(struct dlck_xstream *xs)
{
	int rc;

	rc = ABT_xstream_create(ABT_SCHED_NULL, &xs->xstream);
	if (rc != ABT_SUCCESS) {
		/** XXX translate ABT error */
		return rc;
	}
	rc = ABT_xstream_get_main_pools(xs->xstream, 1, &xs->pool);
	if (rc != ABT_SUCCESS) {
		/** XXX translate ABT error */
		return rc;
	}
	rc = ABT_eventual_create(sizeof(int), &xs->rc_init);
	if (rc != ABT_SUCCESS) {
		/** XXX translate ABT error */
		return rc;
	}

	return 0;
}

/**
 * XXX missing teardown
 */
static int
xstream_ult_create(ABT_pool pool, void (*func)(void *), void *arg, struct dlck_ult *ult)
{
	ABT_thread_attr attr;
	int rc;

	rc = abt_attr_default_create(&attr);
	if (rc) {
		return rc;
	}

	rc = ABT_thread_create(pool, func, arg, attr, &ult->thread);
	if (rc != ABT_SUCCESS) {
		/** XXX translate ABT error */
		return rc;
	}

	/** XXX teardown attr */

	return DER_SUCCESS;
}

static void
nvme_polling(void *arg)
{
	struct bio_xs_context *xsctx = arg;

	do {
		(void) bio_nvme_poll(xsctx);
		ABT_thread_yield();
	} while(true);
}

unsigned int dss_sys_xs_nr = 3;

/** main XS id of (vos) tgt_id */
#define DSS_MAIN_XS_ID(tgt_id) ((tgt_id) + dss_sys_xs_nr)

/** XXX should be shared with the DAOS engine */
#define DSS_SYS_XS_NAME_FMT	"daos_sys_%d"
#define DSS_IO_XS_NAME_FMT "daos_io_%d"

static void
xstream_init_ult(void *arg)
{
	struct dlck_xstream *xs = arg;
	struct dss_module_info *dmi;
	int tag;
	int xs_id;
	int tgt_id = xs->tgt_id;
	char name[DSS_XS_NAME_LEN];
	int rc;

	if (tgt_id < 0) {
		tag = DAOS_SERVER_TAG - DAOS_TGT_TAG;
		xs_id = 0;

		rc = snprintf(name, DSS_XS_NAME_LEN, DSS_SYS_XS_NAME_FMT, 0);
		if (rc != 0) {
			goto fail;
		}
	} else {
		tag = DAOS_SERVER_TAG;
		xs_id = DSS_MAIN_XS_ID(tgt_id);

		rc = snprintf(name, DSS_XS_NAME_LEN, DSS_IO_XS_NAME_FMT, tgt_id);
		if (rc != 0) {
			goto fail;
		}
	}

	(void) pthread_setname_np(pthread_self(), name);

	/**
	 * for xstream:
	 * - dss_tls_init
	 * - bio_xsctxt_alloc
	 * - thread_create(dss_nvme_poll_ult)
	 */

	(void) dss_tls_init(tag, xs_id, tgt_id);

	dmi = dss_get_module_info();
	D_ASSERT(dmi != NULL);

	if (bio_nvme_configured(SMD_DEV_TYPE_META)) {
		rc = bio_xsctxt_alloc(&dmi->dmi_nvme_ctxt, tgt_id, false);
		if (rc != 0) {
			goto fail;
		}

		xstream_ult_create(xs->pool, nvme_polling, dmi->dmi_nvme_ctxt, &xs->nvme_poll);
	}

fail:
	ABT_eventual_set(xs->rc_init, &rc, sizeof(int));
}

/**
 * XXX missing teardown
 */
static int
xstream_start(struct dlck_xstream *xs)
{
	struct dlck_ult ult;
	int rc;
	int *rc_ptr;

	rc = xstream_create(xs);
	if (rc != 0) {
		return rc;
	}

	rc = xstream_ult_create(xs->pool, xstream_init_ult, NULL, &ult);
	if (rc != 0) {
		return rc;
	}
	
	ABT_eventual_wait(xs->rc_init, (void **)&rc_ptr);
	rc = *rc_ptr;
	if (rc != 0) {
		/** XXX translate ABT return code */
		return rc;
	}
	
	rc = ABT_thread_join(ult.thread);
	if (rc != 0) {
		/** XXX translate ABT return code */
		return rc;
	}

	/** ABT_thread free */
	
	return rc;
}

extern struct dss_module vos_srv_module;

extern struct dss_module_key vos_module_key;

/**
 * XXX teardown missing
 */
static int
xstream_start_all(struct dlck_args *args, struct dlck_engine *engine)
{
	struct dlck_xstream *xs;
	int rc;

	/** start daos_sys_0 */
	xs = &engine->xss[engine->targets]; /** there is one more XS than targets */
	xs->tgt_id = -1;
	rc = xstream_start(xs);
	if (rc != 0) {
		return rc;
	}

	/** XXX the user may ask to process a subset of targets */

	/** start daos_io_X */
	for (int i = 0; i < engine->targets; ++i) {
		xs = &engine->xss[engine->targets];
		xs->tgt_id = i;
		rc = xstream_start(xs);
		if (rc != 0) {
			return rc;
		}
	}

	return 0;
}

static uint64_t
dlck_metrics_region_size(int num_tgts)
{
	const uint64_t	est_std_metrics = 1024; /* high estimate to allow for pool links */
	const uint64_t	est_tgt_metrics = 128; /* high estimate */

	return (est_std_metrics + est_tgt_metrics * num_tgts) * D_TM_METRIC_SIZE;
}

/**
 * XXX TODO:
 * - clean up on fail before return
 */
int
dlck_engine_start(struct dlck_args *args, struct dlck_engine **engine_ptr)
{
	struct dlck_engine *engine;
	const struct dlck_args_common *argsc = &args->common;
	const bool bypass_health_chk = false;
	int tag = DAOS_SERVER_TAG - DAOS_TGT_TAG;
	const unsigned instance_idx = 0;
	int           rc;
	
	rc = dlck_engine_alloc(args, &engine);
	if (rc != 0) {
		return rc;
	}

	/**
	 * List of steps executed by the DAOS engine while starting:
	 * - d_tm_init
	 * - register_dbtree_classes
	 * - ABT_init
	 * - bio_nvme_init
	 * - dss_module_init_all -> vos init?
	 * - vos_standalone_tls_init
	 * - dss_sys_db_init
	 * - dss_xstreams_init:
	 *   - start system service XS
	 *   - start main IO service XS
	 */

	/** XXX is it still necessary? */
	rc = d_tm_init(instance_idx, dlck_metrics_region_size(argsc->targets), D_TM_SERVER_PROCESS);
	if (rc != 0) {
		return rc;
	}

	rc = register_dbtree_classes();
	if (rc != 0) {
		return rc;
	}

	rc = dlck_abt_init(engine);
	if (rc != 0) {
		return rc;
	}
	
	rc = bio_nvme_init(argsc->nvme_conf, argsc->numa_node, argsc->nvme_mem_size, argsc->nvme_hugepage_size, argsc->targets, bypass_health_chk);
	if (rc != 0) {
		return rc;
	}
	
	dss_register_key(&daos_srv_modkey);
	dss_register_key(&vos_module_key);
	rc = vos_srv_module.sm_init();
	if (rc != 0) {
		return rc;
	}
	
	rc = vos_standalone_tls_init(tag);
	if (rc != 0) {
		return rc;
	}
	
	rc = dlck_sys_db_init(args);
	if (rc != 0) {
		return rc;
	}

	rc = xstream_start_all(args, engine);
	if (rc != 0) {
		return rc;
	}

	return 0;
}
