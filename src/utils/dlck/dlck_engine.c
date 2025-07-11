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
#include <engine/srv_internal.h>

#include "dlck_args.h"
#include "dlck_engine.h"

int
dss_register_dbtree_classes(void);

extern struct dss_module     vos_srv_module;
extern struct dss_module_key vos_module_key;

/**
 * Allocate an engine.
 *
 * \param[in]	targets		Number of targets.
 * \param[out]	engine_ptr	Allocated engine.
 *
 * \retval DER_SUCCESS	Success.
 * \retval -DER_NOMEM	Out of memory.
 */
static int
dlck_engine_alloc(unsigned targets, struct dlck_engine **engine_ptr)
{
	struct dlck_engine *engine;

	D_ALLOC_PTR(engine);
	if (engine == NULL) {
		return -DER_NOMEM;
	}

	/** each of the targets will get its own xstream + 1 for daos_sys */
	D_ALLOC_ARRAY(engine->xss, targets + 1);
	if (engine->xss == NULL) {
		D_FREE(engine);
		return -DER_NOMEM;
	}

	engine->targets = targets;

	*engine_ptr = engine;

	return DER_SUCCESS;
}

/**
 * Free an engine.
 *
 * \param[in]	engine	An engine to free.
 */
static void
dlck_engine_free(struct dlck_engine *engine)
{
	D_FREE(engine->xss);
	D_FREE(engine);
}

/**
 * Poll for NVMe operations.
 *
 * \param[in]	arg	ABT_eventual too wait for.
 */
static void
nvme_polling(void *arg)
{
	ABT_eventual           *done = arg;
	ABT_bool                is_ready;
	struct dss_module_info *dmi;
	int                     rc;

	dmi = dss_get_module_info();
	D_ASSERT(dmi != NULL);

	do {
		(void)bio_nvme_poll(dmi->dmi_nvme_ctxt);
		ABT_thread_yield();

		rc = ABT_eventual_test(*done, NULL, &is_ready);
		if (rc != 0) {
			return;
		}
	} while (is_ready == ABT_FALSE);
}

int
dlck_engine_xstream_init(struct dlck_xstream *xs)
{
	int                     tag;
	int                     tgt_id = xs->tgt_id;
	int                     xs_id;
	char                    name[DSS_XS_NAME_LEN];
	void                   *tls;
	struct dss_module_info *dmi;
	int                     rc;

	if (tgt_id < 0) {
		tag   = DAOS_SERVER_TAG - DAOS_TGT_TAG;
		xs_id = 0;

		rc = snprintf(name, DSS_XS_NAME_LEN, DSS_SYS_XS_NAME_FMT, 0);
	} else {
		tag   = DAOS_SERVER_TAG;
		xs_id = DSS_MAIN_XS_ID_NO_HELPER_POOL(tgt_id, DSS_SYS_XS_NR_DEFAULT);

		rc = snprintf(name, DSS_XS_NAME_LEN, DSS_IO_XS_NAME_FMT, tgt_id);
	}

	/**
	 * >= DSS_XS_NAME_LEN	the output was truncated
	 * < 0			other error
	 */
	if (rc < 0 || rc >= DSS_XS_NAME_LEN) {
		return -DER_INVAL;
	}

	(void)pthread_setname_np(pthread_self(), name);

	tls = dss_tls_init(tag, xs_id, tgt_id);
	if (tls == NULL) {
		/** Note:  dss_tls_init() returns NULL also on other issues */
		return -DER_NOMEM;
	}

	if (bio_nvme_configured(SMD_DEV_TYPE_META)) {
		dmi = dss_get_module_info();
		D_ASSERT(dmi != NULL);

		rc = bio_xsctxt_alloc(&dmi->dmi_nvme_ctxt, tgt_id, false);
		if (rc != DER_SUCCESS) {
			return rc;
		}

		rc = ABT_eventual_create(0, &xs->nvme_poll_done);
		if (rc != ABT_SUCCESS) {
			dss_tls_fini(tls);
			return dss_abterr2der(rc);
		}

		rc = dlck_ult_create(xs->pool, nvme_polling, &xs->nvme_poll_done, &xs->nvme_poll);
		if (rc != DER_SUCCESS) {
			ABT_eventual_free(&xs->nvme_poll_done);
			dss_tls_fini(tls);
			return rc;
		}
	}

	return DER_SUCCESS;
}

static void
dlck_engine_xstream_init_ult(void *arg)
{
	struct dlck_xstream *xs = arg;

	xs->rc_init = dlck_engine_xstream_init(xs);
}

int
dlck_engine_xstream_fini(struct dlck_xstream *xs)
{
	void *tls = dss_tls_get();
	int   rc  = DER_SUCCESS;

	D_ASSERT(tls != NULL);

	if (bio_nvme_configured(SMD_DEV_TYPE_META)) {
		rc = ABT_eventual_set(xs->nvme_poll_done, NULL, 0);
		rc = dss_abterr2der(rc);
		if (rc != DER_SUCCESS) {
			goto fail;
		}

		rc = ABT_thread_join(xs->nvme_poll.thread);
		rc = dss_abterr2der(rc);
		if (rc != DER_SUCCESS) {
			goto fail;
		}

		rc = ABT_thread_free(&xs->nvme_poll.thread);
		rc = dss_abterr2der(rc);
		if (rc != DER_SUCCESS) {
			/**
			 * After the NVMe polling thread joined we can safely free TLS irrespective
			 * of the error occurred while freeing the thread.
			 */
		}
	}

	dss_tls_fini(tls);

fail:
	/**
	 * In case of a fail we can't join/free the NVMe polling thread nor free TLS which may
	 * result in a SIGSEGV. The best we can do is to leave the resources as they are and pass
	 * error the caller.
	 */

	return rc;
}

/**
 * Create and initialize daos_sys_0 execution stream (XS) and create all daos_io_* XSes.
 * No daos_io_* initialization here yet. They ought to be initialized by the first ULT run in them.
 *
 * \param[in,out]	engine	Engine to start the xstream with.
 *
 * \retval DER_SUCCESS	Success.
 * \retval -DER_*	Error.
 */
static int
xstream_start_all(struct dlck_engine *engine)
{
	struct dlck_xstream *xs;
	struct dlck_ult      daos_sys_init;
	int                  rc;

	/** create and initialize daos_sys_0 execution stream (XS) */
	xs         = &engine->xss[engine->targets]; /** there is one more XS than targets */
	xs->tgt_id = -1;
	rc         = dlck_xstream_create(xs);
	if (rc != 0) {
		return rc;
	}

	rc = dlck_ult_create(xs->pool, dlck_engine_xstream_init_ult, xs, &daos_sys_init);
	if (rc != DER_SUCCESS) {
		/** ULT has not been created - the daos_sys_0 XS can be safely freed */
		(void)dlck_xstream_free(xs);
		return dss_abterr2der(rc);
	}

	/** wait for the daos_sys_0 initialization to conclude */
	rc = ABT_thread_join(daos_sys_init.thread);
	if (rc != ABT_SUCCESS) {
		/** ULT has not joined - cannot safely free the daos_sys_0 XS */
		return dss_abterr2der(rc);
	}

	rc = ABT_thread_free(&daos_sys_init.thread);
	if (rc != ABT_SUCCESS) {
		/** ULT has joined - the daos_sys_0 XS can be safely freed */
		(void)dlck_xstream_free(xs);
		return dss_abterr2der(rc);
	}

	if (xs->rc_init != DER_SUCCESS) {
		/** ULT has joined - the daos_sys_0 XS can be safely freed */
		(void)dlck_xstream_free(xs);
		return xs->rc_init;
	}

	/** create all daos_io_* execution streams (XS) */
	for (int i = 0; i < engine->targets; ++i) {
		xs         = &engine->xss[i];
		xs->tgt_id = i;
		rc         = dlck_xstream_create(xs);
		if (rc != 0) {
			goto fail;
		}
	}

	return 0;

fail:
	/** free all daos_io_* and daos_sys_0 XS */
	for (int i = 0; i <= engine->targets; ++i) {
		xs = &engine->xss[i];
		(void)dlck_xstream_free(xs);
	}

	return rc;
}

int
dlck_engine_start(struct dlck_args_engine *args, struct dlck_engine **engine_ptr)
{
	struct dlck_engine *engine;
	const bool          bypass_health_chk = false;
	int                 tag               = DAOS_SERVER_TAG - DAOS_TGT_TAG;
	int                 rc;

	rc = dlck_engine_alloc(args->targets, &engine);
	if (rc != DER_SUCCESS) {
		return rc;
	}

	rc = dss_register_dbtree_classes();
	if (rc != DER_SUCCESS) {
		goto fail_engine_free;
	}

	rc = dlck_abt_init(engine);
	if (rc != DER_SUCCESS) {
		goto fail_engine_free;
	}

	rc = bio_nvme_init(args->nvme_conf, args->numa_node, args->nvme_mem_size,
			   args->nvme_hugepage_size, args->targets, bypass_health_chk);
	if (rc != DER_SUCCESS) {
		goto fail_abt_fini;
	}

	dss_register_key(&daos_srv_modkey);
	dss_register_key(&vos_module_key);
	rc = vos_srv_module.sm_init();
	if (rc != DER_SUCCESS) {
		goto fail_unregister_keys;
	}

	rc = vos_standalone_tls_init(tag);
	if (rc != DER_SUCCESS) {
		goto fail_vos_sm_fini;
	}

	rc = vos_init(args->nvme_conf, args->storage_path);
	if (rc != DER_SUCCESS) {
		goto fail_vos_tls_fini;
	}

	rc = xstream_start_all(engine);
	if (rc != DER_SUCCESS) {
		goto fail_vos_fini;
	}

	*engine_ptr = engine;

	return 0;

fail_vos_fini:
	vos_db_fini();
fail_vos_tls_fini:
	vos_standalone_tls_fini();
fail_vos_sm_fini:
	(void)vos_srv_module.sm_fini();
fail_unregister_keys:
	dss_unregister_key(&vos_module_key);
	dss_unregister_key(&daos_srv_modkey);
	bio_nvme_fini();
fail_abt_fini:
	(void)dlck_abt_fini(engine);
fail_engine_free:
	dlck_engine_free(engine);

	return rc;
}

int
dlck_engine_stop(struct dlck_engine *engine)
{
	struct dlck_xstream *xs = &engine->xss[engine->targets];
	int                  rc;

	if (bio_nvme_configured(SMD_DEV_TYPE_META)) {
		rc = ABT_eventual_set(xs->nvme_poll_done, NULL, 0);
		if (rc != 0) {
			return rc;
		}

		rc = ABT_thread_join(xs->nvme_poll.thread);
		if (rc != 0) {
			return rc;
		}

		rc = ABT_thread_free(&xs->nvme_poll.thread);
		if (rc != 0) {
			return rc;
		}
	}

	rc = ABT_mutex_free(&engine->open_mtx);
	if (rc != 0) {
		return rc;
	}

	return 0;
}

/**
 * XXX error handling
 */
int
dlck_engine_exec_all(struct dlck_engine *engine, dlck_ult_func exec_one,
		     arg_alloc_fn_t arg_alloc_fn, void *input_arg, arg_free_fn_t arg_free_fn)
{
	struct dlck_ult *ults;
	void           **ult_args;
	int              rc;

	D_ALLOC_ARRAY(ults, engine->targets);
	if (ults == NULL) {
		return ENOMEM;
	}

	D_ALLOC_ARRAY(ult_args, engine->targets);
	if (ult_args == NULL) {
		return ENOMEM;
	}

	for (int i = 0; i < engine->targets; ++i) {
		/** prepare arguments */
		rc = arg_alloc_fn(engine, i, input_arg, &ult_args[i]);
		if (rc != 0) {
			return rc;
		}

		/** start an ULT */
		rc = dlck_ult_create(engine->xss[i].pool, exec_one, ult_args[i], &ults[i]);
		if (rc != 0) {
			return rc;
		}
	}

	for (int i = 0; i < engine->targets; ++i) {
		rc = ABT_thread_join(ults[i].thread);
		if (rc != 0) {
			return rc;
		}

		rc = ABT_thread_free(&ults[i].thread);
		if (rc != 0) {
			return rc;
		}

		rc = arg_free_fn(&ult_args[i]);
		if (rc != 0) {
			return rc;
		}
	}

	D_FREE(ult_args);
	D_FREE(ults);

	return 0;
}
