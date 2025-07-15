/**
 * (C) Copyright 2025 Hewlett Packard Enterprise Development LP.
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
#define D_LOGFAC DD_FAC(tests)

#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <cmocka.h>
#include <getopt.h>
#include <sys/stat.h>
#include <argp.h>

#include <daos_errno.h>
#include <daos/debug.h>
#include <daos/dtx.h>
#include <daos_srv/vos.h>
#include <daos_srv/dtx_srv.h>

#include "../dlck_args.h"
#include "../dlck_engine.h"
#include "../dlck_pool.h"

#define SRAND_SEED 0x1234

struct vos_test_ctx {
	daos_handle_t tc_po_hdl;
	daos_handle_t tc_co_hdl;
	int           tc_step;
};

#define VPOOL_SIZE (1024 * 1024 * 10) /** 10MiB */

static const char Dti1_uuid_str[] = "0faccb2b-d498-49d4-aeee-0668e929e000";
static const char Dti2_uuid_str[] = "525c6a15-8bc9-4918-a8fa-98b959ce6575";
static const char Dti3_uuid_str[] = "25c6a150-8bc9-4918-a8fa-b959ce657503";

struct xstream_arg {
	struct dlck_args_engine *args_engine;
	struct dlck_args_files  *args_files;
	uuid_t                  *co_uuids;

	struct dlck_engine      *engine;
	struct dlck_xstream     *xs;
	struct dlck_file        *file;
	uuid_t                   co_uuid;
	unsigned int             seed;
	int                      rc;
};

static void
test_setup(struct vos_test_ctx *tcx, uuid_t co_uuid)
{
	int rc;

	rc = vos_cont_create(tcx->tc_po_hdl, co_uuid);
	assert_int_equal(rc, 0);

	rc = vos_cont_open(tcx->tc_po_hdl, co_uuid, &tcx->tc_co_hdl);
	assert_int_equal(rc, 0);
}

static void
test_teardown(struct vos_test_ctx *tcx)
{
	int rc;

	rc = vos_cont_close(tcx->tc_co_hdl);
	assert_int_equal(rc, 0);

	rc = vos_pool_close(tcx->tc_po_hdl);
	assert_int_equal(rc, 0);
}

static void
simple_dtx(daos_handle_t coh, const char *dti_uuid_str, daos_unit_oid_t oid, daos_key_t *dkey,
	   daos_iod_t *iod, d_sg_list_t *sgl)
{
	struct dtx_id             dti        = {0};
	struct dtx_epoch          epoch      = {0};
	daos_unit_oid_t           leader_oid = {0};
	// uint32_t           flags      = 0;
	struct dtx_leader_handle *dlh;
	struct dtx_handle        *dth;
	int                       rc;

	rc = uuid_parse(dti_uuid_str, dti.dti_uuid);
	assert_int_equal(rc, 0);
	dti.dti_hlc = d_hlc_get();

	epoch.oe_value = d_hlc_get();

	// rc = dtx_begin(coh, &dti, &epoch, 1, 0, &leader_oid, NULL, 0, flags, NULL, &dth);
	// assert_int_equal(rc, 0);

	rc = dtx_leader_begin(coh, &dti, &epoch, 1, 0, &leader_oid, NULL, 0, NULL, 0, 0, NULL, NULL,
			      &dlh);
	assert_int_equal(rc, 0);

	dth = &dlh->dlh_handle;

	rc = dtx_sub_init(dth, &oid, 0);
	assert_int_equal(rc, 0);

	rc = vos_obj_update_ex(coh, oid, 0, 0, 0, dkey, 1, iod, NULL, sgl, dth);
	assert_int_equal(rc, 0);

	rc = dtx_end(dth, NULL, DER_SUCCESS);
	assert_int_equal(rc, 0);

	printf("[DTX %s] Commit? (y or [n]): ", dti_uuid_str);
	char op = getchar();
	if (op == 'y') {
		rc = vos_dtx_commit(coh, &dti, 1, true, NULL);
		assert_int_equal(rc, 1); /** total number of committed */
	}
	if (op != '\n') {
		(void)getchar(); /** consume the leftover \n */
	}
}

static void
run_all_tests(daos_handle_t poh, struct xstream_arg *xa)
{
	daos_unit_oid_t     oid      = {0};
	uint64_t            dkey_buf = 1;
	daos_key_t          dkey;
	uint64_t            akey_buf = 2;
	daos_key_t          akey;
	daos_iod_t          iod   = {0};
	daos_recx_t         rex   = {0};
	char               *value = "Aloha";
	d_sg_list_t         sgl;
	int                 rc;

	struct vos_test_ctx tcx;

	tcx.tc_po_hdl = poh;

	/** prepare a container */
	test_setup(&tcx, xa->co_uuid);

	/** prepare DKEY, AKEY, IOD and SGL */
	d_iov_set(&dkey, (void *)&dkey_buf, sizeof(dkey_buf));
	d_iov_set(&akey, (void *)&akey_buf, sizeof(akey_buf));
	iod.iod_name  = akey;
	iod.iod_type  = DAOS_IOD_SINGLE;
	iod.iod_recxs = NULL;
	iod.iod_nr    = 1;
	iod.iod_size  = strlen(value);
	rc            = d_sgl_init(&sgl, 1);
	assert_int_equal(rc, 0);
	d_iov_set(&sgl.sg_iovs[0], (void *)value, iod.iod_size);

	/** 1st DTX */
	simple_dtx(tcx.tc_co_hdl, Dti1_uuid_str, oid, &dkey, &iod, &sgl);

	/** 2nd DTX (just a different OID) */
	oid.id_pub.lo = 1;
	simple_dtx(tcx.tc_co_hdl, Dti2_uuid_str, oid, &dkey, &iod, &sgl);

	/** 3rd DTX (EV) */
	oid.id_pub.lo = 2;
	rex.rx_idx    = 0;
	rex.rx_nr     = 1;
	iod.iod_type  = DAOS_IOD_ARRAY;
	iod.iod_recxs = &rex;
	simple_dtx(tcx.tc_co_hdl, Dti3_uuid_str, oid, &dkey, &iod, &sgl);

	/** quick teardown */
	d_sgl_fini(&sgl, false);
	test_teardown(&tcx);
}

static void
exec_one(void *arg)
{
	struct xstream_arg *xa = arg;
	struct dlck_file   *file;
	daos_handle_t       poh;
	int                 rc;

	rc = dlck_engine_xstream_init(xa->xs);
	if (rc != DER_SUCCESS) {
		xa->rc = rc;
		return;
	}

	d_list_for_each_entry(file, &xa->args_files->list, link) {
		/** do not process the given file if the target is excluded */
		if ((file->targets & (1 << xa->xs->tgt_id)) == 0) {
			continue;
		}

		ABT_mutex_lock(xa->engine->open_mtx);
		rc = dlck_pool_open(xa->args_engine->storage_path, file->po_uuid, xa->xs->tgt_id,
				    &poh);
		ABT_mutex_unlock(xa->engine->open_mtx);
		if (rc != DER_SUCCESS) {
			xa->rc = rc;
			break;
		}

		run_all_tests(poh, xa);

		ABT_mutex_lock(xa->engine->open_mtx);
		rc = vos_pool_close(poh);
		ABT_mutex_unlock(xa->engine->open_mtx);
		if (rc != DER_SUCCESS) {
			xa->rc = rc;
			break;
		}
	}

	if (xa->rc != DER_SUCCESS) {
		goto fail_xstream_fini;
	}

	rc = dlck_engine_xstream_fini(xa->xs);
	if (rc != DER_SUCCESS) {
		xa->rc = rc;
	}

	return;

fail_xstream_fini:
	(void)dlck_engine_xstream_fini(xa->xs);
}

static int
arg_alloc(struct dlck_engine *engine, int idx, void *input_arg, void **output_arg)
{
	struct xstream_arg *input_xa = input_arg;
	struct xstream_arg *xa;

	D_ALLOC_PTR(xa);
	if (xa == NULL) {
		return ENOMEM;
	}

	xa->args_engine = input_xa->args_engine;
	xa->args_files  = input_xa->args_files;
	xa->engine      = engine;
	xa->xs          = &engine->xss[idx];
	xa->file        = input_xa->file;
	uuid_copy(xa->co_uuid, xa->co_uuids[idx]);
	xa->seed = rand_r(&input_xa->seed);

	*output_arg = xa;

	return 0;
}

static int
arg_free(void **arg)
{
	D_FREE(*arg);
	*arg = NULL;

	return 0;
}

extern struct argp        argp_file;
extern struct argp        argp_engine;

static struct argp_option _automagic[] = {OPT_HEADER("Other options:", GROUP_AUTOMAGIC), {0}};

static struct argp        automagic = {_automagic, NULL};

static struct argp_child  children[] = {{&argp_file}, {&argp_engine}, {&automagic}, {0}};

struct dlck_helper_args {
	struct dlck_args_files  files;
	struct dlck_args_engine engine;
};

error_t
parser(int key, char *arg, struct argp_state *state)
{
	struct dlck_helper_args *args = state->input;

	/** state changes */
	switch (key) {
	case ARGP_KEY_INIT:
		state->child_inputs[0] = &args->files;
		state->child_inputs[1] = &args->engine;
		break;
	}

	return ARGP_ERR_UNKNOWN;
}

static struct argp argp = {NULL, parser, NULL /** usage */, NULL, children};

extern struct dss_module dtx_module;

// static const char Co_uuid_str[]   = "0faccb2b-d498-49d4-aeef-0668e929e919";

static void
random_uuid(uuid_t uuid, unsigned int *seedp)
{
	char uuid_str[UUID_STR_LEN];
	int  rc;

	for (int i = 0; i < UUID_STR_LEN - 1; ++i) {
		snprintf(&uuid_str[i], 2, "%x", rand_r(seedp) % 16);
	}

	uuid_str[8] = uuid_str[13] = uuid_str[18] = uuid_str[23] = '-';

	rc = uuid_parse(uuid_str, uuid);
	assert_int_equal(rc, 0);
}

static int
setup(struct dlck_helper_args *args, struct xstream_arg *xa)
{
	struct dlck_file   *file;
	struct dlck_engine *engine;
	unsigned int        seed = SRAND_SEED;
	int                 rc;

	/** prepare pool storage directories */
	d_list_for_each_entry(file, &args->files.list, link) {
		rc = dlck_pool_mkdir(args->engine.storage_path, file->po_uuid);
		assert_int_equal(rc, DER_SUCCESS);
	}

	/** start an engine */
	rc = dlck_engine_start(&args->engine, &engine);
	if (rc != DER_SUCCESS) {
		return rc;
	}

	D_ALLOC_ARRAY(xa->co_uuids, args->engine.targets);
	if (xa->co_uuids == NULL) {
		rc = -DER_NOMEM;
		goto fail_engine_stop;
	}

	for (int i = 0; i < args->engine.targets; ++i) {
		random_uuid(xa->co_uuids[i], &seed);
	}

	/** register DTX module key */
	dss_register_key(dtx_module.sm_key);

	xa->engine      = engine;
	xa->args_engine = &args->engine;
	xa->args_files  = &args->files;
	xa->seed        = seed;

	return DER_SUCCESS;

fail_engine_stop:
	(void)dlck_engine_stop(engine);
	return rc;
}

static int
teardown(struct dlck_engine *engine)
{
	int rc;

	dss_unregister_key(dtx_module.sm_key);

	rc = dlck_engine_stop(engine);

	return rc;
}

int
main(int argc, char **argv)
{
	struct dlck_helper_args args   = {0};
	struct xstream_arg      xa     = {0};

	int                     rc;

	argp_parse(&argp, argc, argv, 0, 0, &args);

	rc = setup(&args, &xa);
	if (rc != DER_SUCCESS) {
		goto fail_args_free;
	}

	rc = dlck_engine_exec_all(xa.engine, exec_one, arg_alloc, &xa, arg_free);
	if (rc != DER_SUCCESS) {
		goto fail_teardown;
	}

	rc = teardown(xa.engine);
	if (rc != DER_SUCCESS) {
		goto fail_args_free;
	}

	/** XXX args free */

	return 0;

fail_teardown:
	(void)teardown(xa.engine);
fail_args_free:
	/** XXX args free */
	return rc;
}
