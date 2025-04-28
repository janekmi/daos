/**
 * (C) Copyright 2025 Hewlett Packard Enterprise Development LP.
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
#define D_LOGFAC DD_FAC(telem)

#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <cmocka.h>
#include <getopt.h>
#include <sys/stat.h>
#include <daos/dtx.h>
#include <daos_srv/vos.h>
#include <daos_srv/dtx_srv.h>

struct vos_test_ctx {
	char         *tc_po_name;
	uuid_t        tc_po_uuid;
	uuid_t        tc_co_uuid;
	daos_handle_t tc_po_hdl;
	daos_handle_t tc_co_hdl;
	int           tc_step;
};

struct io_test_args {
	struct vos_test_ctx ctx;
	daos_unit_oid_t     oid;
	/* Optional addn container create params */
	uuid_t              addn_co_uuid;
	daos_handle_t       addn_co;
	/* testing flags, see vts_test_flags */
	daos_epoch_t        epr_lo;
	unsigned long       ta_flags;
	const char         *dkey;
	const char         *akey;
	void               *custom;
	enum daos_otype_t   otype;
	int                 akey_size;
	int                 dkey_size;
	int                 co_create_step;
	bool                checkpoint;
	bool                no_replay;
	bool                fail_replay;
	bool                fail_checkpoint;
};

#define SRAND_SEED  1743171631

#define VPOOL_SIZE  (1024 * 1024 * 10)

#define PO_UUID_STR "a367beed-8857-461c-a532-92ca618e589c"

static const char vos_path[]      = "/mnt/daos";
static const char Po_uuid_str[]   = PO_UUID_STR;
static const char Co_uuid_str[]  = "0faccb2b-d498-49d4-aeef-0668e929e919";
static const char Dti1_uuid_str[] = "0faccb2b-d498-49d4-aeee-0668e929e000";
static const char Dti2_uuid_str[] = "525c6a15-8bc9-4918-a8fa-98b959ce6575";

static void
test_cleanup()
{
	int rc;

	rc = unlink("/mnt/daos/" PO_UUID_STR "/vpool.0");
	assert_true(rc == 0 || (rc == -1 && errno == ENOENT));
	rc = rmdir("/mnt/daos/" PO_UUID_STR);
	assert_true(rc == 0 || (rc == -1 && errno == ENOENT));
	rc = unlink("/mnt/daos/daos_sys/sys_db");
	assert_true(rc == 0 || (rc == -1 && errno == ENOENT));
	rc = rmdir("/mnt/daos/daos_sys");
	assert_true(rc == 0 || (rc == -1 && errno == ENOENT));
}

int
iter_cb_nop(daos_handle_t ih, vos_iter_entry_t *entry, vos_iter_type_t type,
	    vos_iter_param_t *param, void *cb_arg, unsigned int *acts)
{
	return 0;
}

static void
run_all_tests(void)
{
	daos_size_t          psize     = VPOOL_SIZE;
	daos_size_t          meta_size = 0;
	uint64_t             dkey_buf  = 1;
	daos_key_t           dkey;
	daos_iod_t           iod      = {0};
	uint64_t             akey_buf = 2;
	daos_key_t           akey;
	char                *value = "Aloha";
	d_sg_list_t          sgl;
	int                  rc;
	char                *path;

	struct io_test_args  args;
	struct vos_test_ctx *tcx = &args.ctx;
	srand(SRAND_SEED);

	rc = uuid_parse(Po_uuid_str, tcx->tc_po_uuid);
	assert_int_equal(rc, 0);
	rc = uuid_parse(Co_uuid_str, tcx->tc_co_uuid);
	assert_int_equal(rc, 0);

	rc = asprintf(&path, "%s/%s", vos_path, Po_uuid_str);
	assert_int_not_equal(rc, -1);

	rc = mkdir(path, 0777);
	assert_int_equal(rc, 0);

	rc = asprintf(&tcx->tc_po_name, "%s/%s/vpool.0", vos_path, Po_uuid_str);
	assert_int_not_equal(rc, -1);

	rc = vos_pool_create(tcx->tc_po_name, tcx->tc_po_uuid, psize, psize, meta_size,
			     0 /* flags */, 0 /* version */, &tcx->tc_po_hdl);
	assert_int_equal(rc, 0);

	rc = vos_cont_create(tcx->tc_po_hdl, tcx->tc_co_uuid);
	assert_int_equal(rc, 0);

	rc = vos_cont_open(tcx->tc_po_hdl, tcx->tc_co_uuid, &tcx->tc_co_hdl);
	assert_int_equal(rc, 0);

	struct dtx_handle *dth;
	struct dtx_id      dti1       = {0};
	struct dtx_id      dti2       = {0};
	daos_unit_oid_t    leader_oid = {0};
	struct dtx_epoch   epoch      = {0};
	epoch.oe_value                = d_hlc_get();
	uint32_t flags                = 0;

	rc = uuid_parse(Dti_uuid_str, dti1.dti_uuid);
	assert_int_equal(rc, 0);
	dti1.dti_hlc = d_hlc_get();

	Dti_uuid_str[UUID_STR_LEN - 2] = '1';
	rc                             = uuid_parse(Dti_uuid_str, dti2.dti_uuid);
	assert_int_equal(rc, 0);
	dti2.dti_hlc = d_hlc_get();

	rc =
	    dtx_begin(tcx->tc_co_hdl, &dti1, &epoch, 1, 0, &leader_oid, NULL, 0, flags, NULL, &dth);
	assert_int_equal(rc, 0);

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

	rc = dtx_sub_init(dth, &args.oid, 0);
	assert_int_equal(rc, 0);

	rc = vos_obj_update_ex(args.ctx.tc_co_hdl, args.oid, 0, 0, 0, &dkey, 1, &iod, NULL, &sgl,
			       dth);
	assert_int_equal(rc, 0);

	rc = dtx_end(dth, NULL, DER_SUCCESS);
	assert_int_equal(rc, 0);

	/** XXX */
	rc =
	    dtx_begin(tcx->tc_co_hdl, &dti2, &epoch, 1, 0, &leader_oid, NULL, 0, flags, NULL, &dth);
	assert_int_equal(rc, 0);

	rc = dtx_sub_init(dth, &args.oid, 0);
	assert_int_equal(rc, 0);

	args.oid.id_pub.lo = 1;

	rc = vos_obj_update_ex(args.ctx.tc_co_hdl, args.oid, 0, 0, 0, &dkey, 1, &iod, NULL, &sgl,
			       dth);
	assert_int_equal(rc, 0);

	rc = dtx_end(dth, NULL, DER_SUCCESS);
	assert_int_equal(rc, 0);

	rc = vos_dtx_commit(args.ctx.tc_co_hdl, &dti1, 1, true, NULL);
	assert_int_equal(rc, 1); /** total number of committed */
	/** XXX */

	// rc = vos_dtx_commit(args.ctx.tc_co_hdl, &dti2, 1, true, NULL);
	// assert_int_equal(rc, 1); /** total number of committed */

	vos_iter_param_t        param   = {0};
	struct vos_iter_anchors anchors = {0};

	param.ip_hdl        = args.ctx.tc_co_hdl;
	param.ip_epr.epr_hi = DAOS_EPOCH_MAX;

	rc = vos_iterate(&param, VOS_ITER_OBJ, false, &anchors, iter_cb_nop, iter_cb_nop, NULL,
			 NULL);
	assert_int_equal(rc, 0);

	d_sgl_fini(&sgl, false);

	rc = vos_cont_close(tcx->tc_co_hdl);
	assert_int_equal(rc, 0);

	rc = vos_pool_close(tcx->tc_po_hdl);
	assert_int_equal(rc, 0);
}

int
main(int argc, char **argv)
{
	int rc;

	d_register_alt_assert(mock_assert);

	rc = daos_debug_init(DAOS_LOG_DEFAULT);
	if (rc) {
		print_error("Error initializing debug system\n");
		return rc;
	}

	test_cleanup();

	rc = vos_self_init(vos_path, true, BIO_STANDALONE_TGT_ID);
	if (rc) {
		print_error("Error initializing VOS instance\n");
		goto exit_0;
	}

	run_all_tests();

	vos_self_fini();

exit_0:
	daos_debug_fini();

	return 0;
}
