/**
 * (C) Copyright 2025 Hewlett Packard Enterprise Development LP
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#define D_LOGFAC DD_FAC(tests)

#include <stdio.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>

#include <daos_errno.h>

#include "../dlck_args.h"
#include "../dlck_cmds.h"
#include "../dlck_engine.h"
#include "../dlck_internal.h"
#include "../dlck_pool.h"

#define NO_FILES_RC      (-DER_ENOENT)
#define GENERIC_ERROR_RC (-DER_MISC)
#define RANDOM_INT_A     17
#define RANDOM_INT_B     45
#define OPEN_MTX_PTR     ((void *)0x310)
#define TGT_ID           0
#define NR_TARGETS       4
#define POOL_HANDLE      0x6001
#define CONT_HANDLE      0xc041
#define CONT_HANDLE2     0xc042
#define REC_LID          78
#define REC_UMOFF        0x0440ff

int
arg_alloc(struct dlck_engine *engine, int idx, void *ctrl_ptr, void **output_arg);
int
arg_free(void *ctrl_ptr, void **arg);
void
exec_one(void *arg);

static int
			   mock_printf(const char *fmt, ...);

/** globals */

static char                Storage_path[] = "/mock/storage/path";
static const char                Co_uuid_str[]  = "caab6a56-8cbf-46c2-924b-5620334c0214";

static struct dlck_control Ctrl;
static struct dlck_file    File1;
static struct dlck_file    File2;
static struct dlck_engine  Engine;
static struct dlck_xstream Xs;
static struct xstream_arg        Xa;
static const struct dlck_dtx_rec Rec = {.lid = REC_LID, .umoff = REC_UMOFF};
static struct co_uuid_list_elem  Co_uuid_elem1;
static struct co_uuid_list_elem  Co_uuid_elem2;

static void
ctrl_default()
{
	memset(&Ctrl, 0, sizeof(Ctrl));
	Ctrl.common.write_mode = true;

	/** files - one file by default */
	memset(&File1, 0, sizeof(File1));
	memset(&File2, 0, sizeof(File2));
	D_INIT_LIST_HEAD(&Ctrl.files.list);
	d_list_add(&File1.link, &Ctrl.files.list);
	File1.targets = (1 << TGT_ID);

	Ctrl.engine.storage_path = Storage_path;
	Ctrl.engine.nr_targets   = NR_TARGETS;
	Ctrl.print.dp_printf = mock_printf;

	memset(&Engine, 0, sizeof(Engine));
	memset(&Xs, 0, sizeof(Xs));
	Engine.xss = &Xs;
	Engine.open_mtx = OPEN_MTX_PTR;

	memset(&Xa, 0, sizeof(Xa));
	Xa.ctrl   = &Ctrl;
	Xa.engine = &Engine;
	Xa.xs     = &Xs;

	memset(&Co_uuid_elem1, 0, sizeof(Co_uuid_elem1));
	memset(&Co_uuid_elem2, 0, sizeof(Co_uuid_elem2));
}

/** mocks */

static int
mock_printf(const char *fmt, ...)
{
	function_called();
	return 0;
}

int
dlck_pool_mkdir(const char *storage_path, uuid_t po_uuid)
{
	assert_ptr_equal(storage_path, Storage_path);
	char *po_uuid_exp = mock_type(char *);
	assert_ptr_equal(po_uuid, po_uuid_exp);

	return mock_type(int);
}

int
dlck_engine_start(struct dlck_args_engine *args, struct dlck_engine **engine_ptr)
{
	int rc;

	assert_ptr_equal(args, &Ctrl.engine);

	rc = mock_type(int);

	if (rc == DER_SUCCESS) {
		*engine_ptr = &Engine;
	}

	return rc;
}

int
dlck_engine_exec_all(struct dlck_engine *engine, dlck_ult_func exec_one_fn,
		     arg_alloc_fn_t arg_alloc_fn, void *custom, arg_free_fn_t arg_free_fn)
{
	assert_ptr_equal(engine, &Engine);
	assert_ptr_equal(exec_one_fn, exec_one);
	assert_ptr_equal(arg_alloc_fn, arg_alloc);
	assert_ptr_equal(arg_free_fn, arg_free);
	assert_ptr_equal(custom, &Ctrl);

	return mock_type(int);
}

int
dlck_engine_stop(struct dlck_engine *engine)
{
	assert_ptr_equal(engine, &Engine);

	return mock_type(int);
}

void *
__real_d_calloc(size_t count, size_t eltsize);

void *
__wrap_d_calloc(size_t count, size_t eltsize)
{
	int rc = mock_type(int);

	if (rc == ENOMEM) {
		errno = ENOMEM;
		return NULL;
	}

	return __real_d_calloc(count, eltsize);
}

int
dlck_engine_xstream_init(struct dlck_xstream *xs)
{
	assert_ptr_equal(xs, &Xs);

	return mock_type(int);
}

int
dlck_engine_xstream_fini(struct dlck_xstream *xs)
{
	assert_ptr_equal(xs, &Xs);

	return mock_type(int);
}

int
dlck_abt_pool_open(ABT_mutex mtx, const char *storage_path, uuid_t po_uuid, int tgt_id,
		   daos_handle_t *poh)
{
	assert_ptr_equal(mtx, OPEN_MTX_PTR);
	assert_ptr_equal(storage_path, Storage_path);
	assert_ptr_equal(po_uuid, File1.po_uuid);
	assert_int_equal(tgt_id, TGT_ID);

	int rc = mock_type(int);

	if (rc == DER_SUCCESS) {
		(*poh).cookie = POOL_HANDLE;
	}

	return rc;
}

int
dlck_abt_pool_close(ABT_mutex mtx, daos_handle_t poh)
{
	assert_ptr_equal(mtx, OPEN_MTX_PTR);
	assert_int_equal(poh.cookie, POOL_HANDLE);

	return mock_type(int);
}

int
__wrap_vos_cont_open(daos_handle_t poh, uuid_t co_uuid, daos_handle_t *coh)
{
	assert_int_equal(poh.cookie, POOL_HANDLE);
	char *co_uuid_exp = mock_type(char *);
	assert_ptr_equal(co_uuid, co_uuid_exp);

	int rc = mock_type(int);

	if (rc == DER_SUCCESS) {
		if (co_uuid == Ctrl.common.co_uuid || co_uuid == Co_uuid_elem1.uuid) {
			(*coh).cookie = CONT_HANDLE;
		} else {
			(*coh).cookie = CONT_HANDLE2;
		}
	}

	return rc;
}

int
__wrap_vos_cont_close(daos_handle_t coh)
{
	assert_int_equal(coh.cookie, mock_type(uint64_t));

	return mock_type(int);
}

int
__wrap_dlck_vos_cont_recs_get_active(daos_handle_t coh, d_vector_t *dv, struct dlck_stats *ds)
{
	int rc;

	assert_int_equal(coh.cookie, mock_type(uint64_t));
	assert_ptr_not_equal(dv, NULL);
	assert_int_equal(dv->dv_entry_size, sizeof(struct dlck_dtx_rec));
	assert_int_equal(dv->dv_segment_capacity,
			 D_VECTOR_SEGMENT_RAW_CAPACITY / dv->dv_entry_size);
	assert_true(d_list_empty(&dv->dv_list));
	assert_ptr_equal(ds, &Xa.stats);

	rc = mock_type(int);

	if (rc != DER_SUCCESS) {
		return rc;
	}

	rc = d_vector_append(dv, &Rec);
	assert_int_equal(rc, DER_SUCCESS);

	return DER_SUCCESS;
}

int
__wrap_dlck_dtx_act_recs_remove(daos_handle_t coh)
{
	assert_int_equal(coh.cookie, mock_type(uint64_t));

	return mock_type(int);
}

int
__wrap_dlck_dtx_act_recs_set(daos_handle_t coh, d_vector_t *dv)
{
	struct dlck_dtx_rec *rec;
	d_vector_segment_t  *dvs;
	uint32_t             idx;

	assert_int_equal(coh.cookie, mock_type(uint64_t));

	d_vector_for_each_entry(rec, dvs, idx, &dv->dv_list) {
		assert_int_equal(rec->lid, REC_LID);
		assert_int_equal(rec->umoff, REC_UMOFF);
	}

	return mock_type(int);
}

int
dlck_pool_cont_list(daos_handle_t poh, d_list_t *co_uuids)
{
	int rc;

	assert_int_equal(poh.cookie, mock_type(uint64_t));

	rc = mock_type(int);
	if (rc != DER_SUCCESS) {
		return rc;
	}

	d_list_add(&Co_uuid_elem1.link, co_uuids);
	d_list_add(&Co_uuid_elem2.link, co_uuids);

	return DER_SUCCESS;
}

int
dlck_pool_cont_list_free(d_list_t *co_uuids)
{
	D_INIT_LIST_HEAD(co_uuids);

	return mock_type(int);
}

/** tests */

static void
test_null(void **unused)
{
	int rc;

	rc = dlck_dtx_act_recs_recover(NULL);
	assert_int_equal(rc, -DER_INVAL);
}

/**
 * Make sure a message is printed in case the write mode is not enabled.
 */
static void
test_not_write_mode(void **unused)
{
	int rc;

	ctrl_default();
	Ctrl.common.write_mode = false;
	expect_function_call(mock_printf);
	/** detach files from the list - stop early */
	D_INIT_LIST_HEAD(&Ctrl.files.list);

	rc = dlck_dtx_act_recs_recover(&Ctrl);
	assert_int_equal(rc, NO_FILES_RC);
}

static void
test_no_files(void **unused)
{
	int rc;

	ctrl_default();
	/** remove files from the list */
	D_INIT_LIST_HEAD(&Ctrl.files.list);

	rc = dlck_dtx_act_recs_recover(&Ctrl);
	assert_int_equal(rc, NO_FILES_RC);
}

static void
test_two_files(void **unused)
{
	int rc;

	ctrl_default();
	d_list_add(&File2.link, &Ctrl.files.list);

	/** reverse order */
	will_return(dlck_pool_mkdir, File2.po_uuid);
	will_return(dlck_pool_mkdir, DER_SUCCESS);
	will_return(dlck_pool_mkdir, File1.po_uuid);
	will_return(dlck_pool_mkdir, DER_SUCCESS);
	will_return(dlck_engine_start, GENERIC_ERROR_RC); /** stop early */
	rc = dlck_dtx_act_recs_recover(&Ctrl);
	assert_int_equal(rc, GENERIC_ERROR_RC);
}

static void
test_pool_mkdir_fails(void **unused)
{
	int rc;

	ctrl_default();

	will_return(dlck_pool_mkdir, File1.po_uuid);
	will_return(dlck_pool_mkdir, GENERIC_ERROR_RC);
	rc = dlck_dtx_act_recs_recover(&Ctrl);
	assert_int_equal(rc, GENERIC_ERROR_RC);
}

static void
test_engine_start_fails(void **unused)
{
	int rc;

	ctrl_default();

	will_return(dlck_pool_mkdir, File1.po_uuid);
	will_return(dlck_pool_mkdir, DER_SUCCESS);
	will_return(dlck_engine_start, GENERIC_ERROR_RC);
	rc = dlck_dtx_act_recs_recover(&Ctrl);
	assert_int_equal(rc, GENERIC_ERROR_RC);
}

static void
test_exec_all_fails(void **unused)
{
	int rc;

	ctrl_default();

	will_return(dlck_pool_mkdir, File1.po_uuid);
	will_return(dlck_pool_mkdir, DER_SUCCESS);
	will_return(dlck_engine_start, DER_SUCCESS);
	will_return(dlck_engine_exec_all, GENERIC_ERROR_RC);
	will_return(dlck_engine_stop, DER_SUCCESS);
	rc = dlck_dtx_act_recs_recover(&Ctrl);
	assert_int_equal(rc, GENERIC_ERROR_RC);
}

static void
test_args_alloc_fails(void **unused)
{
	void *output = NULL;
	int rc;

	ctrl_default();

	will_return(__wrap_d_calloc, ENOMEM);
	rc = arg_alloc(&Engine, 0, &Ctrl, &output);
	assert_int_equal(rc, -DER_NOMEM);
	assert_ptr_equal(output, NULL);
}

static void
test_args_alloc_success(void **unused)
{
	struct xstream_arg *arg = NULL;
	int rc;

	ctrl_default();

	will_return(__wrap_d_calloc, 0);
	rc = arg_alloc(&Engine, 0, &Ctrl, (void **)&arg);
	assert_int_equal(rc, DER_SUCCESS);
	assert_ptr_not_equal(arg, NULL);
	assert_ptr_equal(arg->ctrl, &Ctrl);
	assert_ptr_equal(arg->engine, &Engine);
	assert_ptr_equal(arg->xs, &Xs);
	assert_int_equal(arg->rc, DER_SUCCESS);

	(void)arg_free(&Ctrl, (void **)&arg);
}

static void
test_args_free_success(void **unused)
{
	struct xstream_arg *arg = NULL;
	int                 rc;

	ctrl_default();
	Ctrl.stats.touched = RANDOM_INT_A;

	will_return(__wrap_d_calloc, 0);
	rc = arg_alloc(&Engine, 0, &Ctrl, (void **)&arg);
	assert_int_equal(rc, DER_SUCCESS);
	assert_ptr_not_equal(arg, NULL);

	arg->stats.touched = RANDOM_INT_B;

	rc = arg_free(&Ctrl, (void **)&arg);
	assert_int_equal(rc, DER_SUCCESS);
	assert_ptr_equal(arg, NULL);
	assert_int_equal(Ctrl.stats.touched, RANDOM_INT_A + RANDOM_INT_B);
}

static void
test_xstream_init_fails(void **unused)
{
	ctrl_default();

	will_return(dlck_engine_xstream_init, GENERIC_ERROR_RC);
	exec_one(&Xa);
	assert_int_equal(Xa.rc, GENERIC_ERROR_RC);
}

static void
test_pool_open_fails(void **unused)
{
	ctrl_default();

	will_return(dlck_engine_xstream_init, DER_SUCCESS);
	will_return(dlck_abt_pool_open, GENERIC_ERROR_RC);
	will_return(dlck_engine_xstream_fini, DER_SUCCESS);
	exec_one(&Xa);
	assert_int_equal(Xa.rc, GENERIC_ERROR_RC);
}

static void
test_cont_open_fails(void **unused)
{
	int rc;

	ctrl_default();

	rc = uuid_parse(Co_uuid_str, Ctrl.common.co_uuid);
	assert_int_equal(rc, 0);

	will_return(dlck_engine_xstream_init, DER_SUCCESS);
	will_return(dlck_abt_pool_open, DER_SUCCESS);
	will_return(__wrap_vos_cont_open, Ctrl.common.co_uuid);
	will_return(__wrap_vos_cont_open, GENERIC_ERROR_RC);
	will_return(dlck_abt_pool_close, DER_SUCCESS);
	will_return(dlck_engine_xstream_fini, DER_SUCCESS);
	exec_one(&Xa);
	assert_int_equal(Xa.rc, GENERIC_ERROR_RC);
}

static void
test_cont_recs_get_active_fails(void **unused)
{
	int rc;

	ctrl_default();

	rc = uuid_parse(Co_uuid_str, Ctrl.common.co_uuid);
	assert_int_equal(rc, 0);

	will_return(dlck_engine_xstream_init, DER_SUCCESS);
	will_return(dlck_abt_pool_open, DER_SUCCESS);
	will_return(__wrap_vos_cont_open, Ctrl.common.co_uuid);
	will_return(__wrap_vos_cont_open, DER_SUCCESS);
	will_return(__wrap_dlck_vos_cont_recs_get_active, CONT_HANDLE);
	will_return(__wrap_dlck_vos_cont_recs_get_active, GENERIC_ERROR_RC);
	will_return(__wrap_vos_cont_close, CONT_HANDLE);
	will_return(__wrap_vos_cont_close, DER_SUCCESS);
	will_return(dlck_abt_pool_close, DER_SUCCESS);
	will_return(dlck_engine_xstream_fini, DER_SUCCESS);
	exec_one(&Xa);
	assert_int_equal(Xa.rc, GENERIC_ERROR_RC);
}

static void
test_xstream_fini_fails(void **unused)
{
	int rc;

	ctrl_default();

	Ctrl.common.write_mode = false;

	rc = uuid_parse(Co_uuid_str, Ctrl.common.co_uuid);
	assert_int_equal(rc, 0);

	will_return(dlck_engine_xstream_init, DER_SUCCESS);
	will_return(dlck_abt_pool_open, DER_SUCCESS);
	will_return(__wrap_vos_cont_open, Ctrl.common.co_uuid);
	will_return(__wrap_vos_cont_open, DER_SUCCESS);
	will_return(__wrap_dlck_vos_cont_recs_get_active, CONT_HANDLE);
	will_return(__wrap_dlck_vos_cont_recs_get_active, DER_SUCCESS);
	will_return(__wrap_d_calloc, 0);
	will_return(__wrap_vos_cont_close, CONT_HANDLE);
	will_return(__wrap_vos_cont_close, DER_SUCCESS);
	will_return(dlck_abt_pool_close, DER_SUCCESS);
	will_return(dlck_engine_xstream_fini, GENERIC_ERROR_RC);
	exec_one(&Xa);
	assert_int_equal(Xa.rc, GENERIC_ERROR_RC);
}

static void
test_not_write_mode_success(void **unused)
{
	int rc;

	ctrl_default();

	Ctrl.common.write_mode = false;

	rc = uuid_parse(Co_uuid_str, Ctrl.common.co_uuid);
	assert_int_equal(rc, 0);

	will_return(dlck_engine_xstream_init, DER_SUCCESS);
	will_return(dlck_abt_pool_open, DER_SUCCESS);
	will_return(__wrap_vos_cont_open, Ctrl.common.co_uuid);
	will_return(__wrap_vos_cont_open, DER_SUCCESS);
	will_return(__wrap_dlck_vos_cont_recs_get_active, CONT_HANDLE);
	will_return(__wrap_dlck_vos_cont_recs_get_active, DER_SUCCESS);
	will_return(__wrap_d_calloc, 0);
	will_return(__wrap_vos_cont_close, CONT_HANDLE);
	will_return(__wrap_vos_cont_close, DER_SUCCESS);
	will_return(dlck_abt_pool_close, DER_SUCCESS);
	will_return(dlck_engine_xstream_fini, DER_SUCCESS);
	exec_one(&Xa);
	assert_int_equal(Xa.rc, DER_SUCCESS);
}

static void
test_dtx_act_recs_remove_fails(void **unused)
{
	int rc;

	ctrl_default();

	rc = uuid_parse(Co_uuid_str, Ctrl.common.co_uuid);
	assert_int_equal(rc, 0);

	will_return(dlck_engine_xstream_init, DER_SUCCESS);
	will_return(dlck_abt_pool_open, DER_SUCCESS);
	will_return(__wrap_vos_cont_open, Ctrl.common.co_uuid);
	will_return(__wrap_vos_cont_open, DER_SUCCESS);
	will_return(__wrap_dlck_vos_cont_recs_get_active, CONT_HANDLE);
	will_return(__wrap_dlck_vos_cont_recs_get_active, DER_SUCCESS);
	will_return(__wrap_d_calloc, 0);
	will_return(__wrap_dlck_dtx_act_recs_remove, CONT_HANDLE);
	will_return(__wrap_dlck_dtx_act_recs_remove, GENERIC_ERROR_RC);
	will_return(__wrap_vos_cont_close, CONT_HANDLE);
	will_return(__wrap_vos_cont_close, DER_SUCCESS);
	will_return(dlck_abt_pool_close, DER_SUCCESS);
	will_return(dlck_engine_xstream_fini, DER_SUCCESS);
	exec_one(&Xa);
	assert_int_equal(Xa.rc, GENERIC_ERROR_RC);
}

static void
test_dtx_act_recs_set_fails(void **unused)
{
	int rc;

	ctrl_default();

	rc = uuid_parse(Co_uuid_str, Ctrl.common.co_uuid);
	assert_int_equal(rc, 0);

	will_return(dlck_engine_xstream_init, DER_SUCCESS);
	will_return(dlck_abt_pool_open, DER_SUCCESS);
	will_return(__wrap_vos_cont_open, Ctrl.common.co_uuid);
	will_return(__wrap_vos_cont_open, DER_SUCCESS);
	will_return(__wrap_dlck_vos_cont_recs_get_active, CONT_HANDLE);
	will_return(__wrap_dlck_vos_cont_recs_get_active, DER_SUCCESS);
	will_return(__wrap_d_calloc, 0);
	will_return(__wrap_dlck_dtx_act_recs_remove, CONT_HANDLE);
	will_return(__wrap_dlck_dtx_act_recs_remove, DER_SUCCESS);
	will_return(__wrap_dlck_dtx_act_recs_set, CONT_HANDLE);
	will_return(__wrap_dlck_dtx_act_recs_set, GENERIC_ERROR_RC);
	will_return(__wrap_vos_cont_close, CONT_HANDLE);
	will_return(__wrap_vos_cont_close, DER_SUCCESS);
	will_return(dlck_abt_pool_close, DER_SUCCESS);
	will_return(dlck_engine_xstream_fini, DER_SUCCESS);
	exec_one(&Xa);
	assert_int_equal(Xa.rc, GENERIC_ERROR_RC);
}

static void
test_single_cont_success(void **unused)
{
	int rc;

	ctrl_default();

	rc = uuid_parse(Co_uuid_str, Ctrl.common.co_uuid);
	assert_int_equal(rc, 0);

	will_return(dlck_engine_xstream_init, DER_SUCCESS);
	will_return(dlck_abt_pool_open, DER_SUCCESS);
	will_return(__wrap_vos_cont_open, Ctrl.common.co_uuid);
	will_return(__wrap_vos_cont_open, DER_SUCCESS);
	will_return(__wrap_dlck_vos_cont_recs_get_active, CONT_HANDLE);
	will_return(__wrap_dlck_vos_cont_recs_get_active, DER_SUCCESS);
	will_return(__wrap_d_calloc, 0);
	will_return(__wrap_dlck_dtx_act_recs_remove, CONT_HANDLE);
	will_return(__wrap_dlck_dtx_act_recs_remove, DER_SUCCESS);
	will_return(__wrap_dlck_dtx_act_recs_set, CONT_HANDLE);
	will_return(__wrap_dlck_dtx_act_recs_set, DER_SUCCESS);
	will_return(__wrap_vos_cont_close, CONT_HANDLE);
	will_return(__wrap_vos_cont_close, DER_SUCCESS);
	will_return(dlck_abt_pool_close, DER_SUCCESS);
	will_return(dlck_engine_xstream_fini, DER_SUCCESS);
	exec_one(&Xa);
	assert_int_equal(Xa.rc, DER_SUCCESS);
}

static void
test_pool_cont_list_fails(void **unused)
{
	ctrl_default();

	will_return(dlck_engine_xstream_init, DER_SUCCESS);
	will_return(dlck_abt_pool_open, DER_SUCCESS);
	will_return(dlck_pool_cont_list, POOL_HANDLE);
	will_return(dlck_pool_cont_list, GENERIC_ERROR_RC);
	will_return(dlck_abt_pool_close, DER_SUCCESS);
	will_return(dlck_engine_xstream_fini, DER_SUCCESS);
	exec_one(&Xa);
	assert_int_equal(Xa.rc, GENERIC_ERROR_RC);
}

static void
test_pool_cont_list_free_fails(void **unused)
{
	const unsigned char *co_uuids[]     = {Co_uuid_elem2.uuid, Co_uuid_elem1.uuid};
	const uint64_t       cont_handles[] = {CONT_HANDLE2, CONT_HANDLE};
	assert_int_equal(ARRAY_SIZE(co_uuids), ARRAY_SIZE(cont_handles));

	ctrl_default();

	will_return(dlck_engine_xstream_init, DER_SUCCESS);
	will_return(dlck_abt_pool_open, DER_SUCCESS);
	will_return(dlck_pool_cont_list, POOL_HANDLE);
	will_return(dlck_pool_cont_list, DER_SUCCESS);

	for (int i = 0; i < ARRAY_SIZE(co_uuids); ++i) {
		uint64_t cont_handle = cont_handles[i];

		will_return(__wrap_vos_cont_open, co_uuids[i]);
		will_return(__wrap_vos_cont_open, DER_SUCCESS);
		will_return(__wrap_dlck_vos_cont_recs_get_active, cont_handle);
		will_return(__wrap_dlck_vos_cont_recs_get_active, DER_SUCCESS);
		will_return(__wrap_d_calloc, 0);
		will_return(__wrap_dlck_dtx_act_recs_remove, cont_handle);
		will_return(__wrap_dlck_dtx_act_recs_remove, DER_SUCCESS);
		will_return(__wrap_dlck_dtx_act_recs_set, cont_handle);
		will_return(__wrap_dlck_dtx_act_recs_set, DER_SUCCESS);
		will_return(__wrap_vos_cont_close, cont_handle);
		will_return(__wrap_vos_cont_close, DER_SUCCESS);
	}

	will_return(dlck_pool_cont_list_free, GENERIC_ERROR_RC);
	will_return(dlck_abt_pool_close, DER_SUCCESS);
	will_return(dlck_engine_xstream_fini, DER_SUCCESS);
	exec_one(&Xa);
	assert_int_equal(Xa.rc, GENERIC_ERROR_RC);
}

static const struct CMUnitTest tests_all[] = {
    {"D100: null", test_null, NULL, NULL},
    {"D101: !write_mode", test_not_write_mode, NULL, NULL},
    {"D102: no files", test_no_files, NULL, NULL},
    {"D103: two files", test_two_files, NULL, NULL},
    {"D104: pool mkdir fails", test_pool_mkdir_fails, NULL, NULL},
    {"D105: engine start fails", test_engine_start_fails, NULL, NULL},
    {"D106: exec_all fails", test_exec_all_fails, NULL, NULL},
    {"D107: arg_alloc fails", test_args_alloc_fails, NULL, NULL},
    {"D108: arg_alloc success", test_args_alloc_success, NULL, NULL},
    {"D109: arg_free success", test_args_free_success, NULL, NULL},
    {"D110: xstream_init fails", test_xstream_init_fails, NULL, NULL},
    {"D111: pool open fails", test_pool_open_fails, NULL, NULL},
    {"D112: container open fails", test_cont_open_fails, NULL, NULL},
    {"D113: get active recs fails", test_cont_recs_get_active_fails, NULL, NULL},
    {"D114: xstream_fini fails", test_xstream_fini_fails, NULL, NULL},
    {"D115: success (!write_mode)", test_not_write_mode_success, NULL, NULL},
    {"D116: DTX active recs removal fails", test_dtx_act_recs_remove_fails, NULL, NULL},
    {"D117: DTX active recs set fails", test_dtx_act_recs_set_fails, NULL, NULL},
    {"D118: single container success", test_single_cont_success, NULL, NULL},
    {"D119: pool container list fails", test_pool_cont_list_fails, NULL, NULL},
    {"D120: pool container list free fails", test_pool_cont_list_free_fails, NULL, NULL},
};

int
main(int argc, char **argv)
{
	const char *test_name = "dlck_dtx_act_recs_recover tests";

	return cmocka_run_group_tests_name(test_name, tests_all, NULL, NULL);
}
