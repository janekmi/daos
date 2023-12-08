/**
 * (C) Copyright 2023 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
/**
 * Local transactions tests
 */
#define D_LOGFAC DD_FAC(tests)

#include <stddef.h>
#include <stdbool.h>
#include <uuid/uuid.h>
#include <daos_types.h>
#include <daos/object.h>

#include "vts_io.h"

static int start_epoch = 5;
#define BUF_SIZE 2000
static int buf_size = BUF_SIZE;

static void
local_transaction(void **state)
{
	struct io_test_args *arg = *state;
	int                  rc  = 0;
	int                  passed_rc;
	daos_key_t           dkey[4];
	daos_key_t           akey;
	daos_iod_t           iod;
	d_sg_list_t          sgl;
	d_sg_list_t          fetch_sgl;
	char                 buf[32];
	char                 buf2[32];
	daos_epoch_t         epoch = start_epoch;
	struct dtx_handle   *dth   = NULL;
	const char          *first = "Hello";
	char                 dkey_buf[4][UPDATE_DKEY_SIZE];
	char                 akey_buf[UPDATE_AKEY_SIZE];
	daos_unit_oid_t      oid;
	int                  i;

	test_args_reset(arg, VPOOL_SIZE);

	memset(&iod, 0, sizeof(iod));

	rc = d_sgl_init(&sgl, 1);
	assert_rc_equal(rc, 0);
	rc = d_sgl_init(&fetch_sgl, 1);
	assert_rc_equal(rc, 0);

	/* Set up dkey and akey */
	oid = gen_oid(arg->otype);
	for (i = 0; i < 4; i++) {
		vts_key_gen(&dkey_buf[i][0], arg->dkey_size, true, arg);
		set_iov(&dkey[i], &dkey_buf[i][0],
			is_daos_obj_type_set(arg->otype, DAOS_OT_DKEY_UINT64));
	}
	vts_key_gen(&akey_buf[0], arg->akey_size, true, arg);
	set_iov(&akey, &akey_buf[0], is_daos_obj_type_set(arg->otype, DAOS_OT_AKEY_UINT64));
	iod.iod_type  = DAOS_IOD_SINGLE;
	iod.iod_name  = akey;
	iod.iod_recxs = NULL;
	iod.iod_nr    = 1;

	/* add simple update - fetch sequnece to fill the pool with some data */
	if (1) {
		const char *pre_test_data = "Aloha";

		iod.iod_size = strlen(pre_test_data);
		d_iov_set(&sgl.sg_iovs[0], (void *)pre_test_data, iod.iod_size);

		rc = vos_obj_update(arg->ctx.tc_co_hdl, oid, epoch++, 0, 0, &dkey[2], 1, &iod, NULL,
				    &sgl);
		assert_rc_equal(rc, 0);
		/* prepare fetch buffer with distinct initial content */
		d_iov_set(&fetch_sgl.sg_iovs[0], (void *)buf, sizeof(buf));
		iod.iod_size = UINT64_MAX;
		memset(buf, 'x', sizeof(buf));
		/* set expected fetch result */
		memcpy(buf2, pre_test_data, strlen(pre_test_data));

		rc =
		    vos_obj_fetch(arg->ctx.tc_co_hdl, oid, epoch, 0, &dkey[2], 1, &iod, &fetch_sgl);
		assert_rc_equal(rc, 0);

		printf("size=" DF_U64 "\n", iod.iod_size);
		printf("dkey[2] buf  = %.*s\n", (int)strlen(pre_test_data), buf);
		printf("dkey[2] buf2 = %.*s\n", (int)strlen(pre_test_data), buf2);
		fflush(stdout);
		assert_int_equal(iod.iod_size, (int)strlen(pre_test_data));
		assert_int_equal(fetch_sgl.sg_iovs[0].iov_len, (int)strlen(pre_test_data));
		assert_memory_equal(buf, buf2, fetch_sgl.sg_iovs[0].iov_len);

		/* prepare fetch buffer with distinct initial content */
		iod.iod_size = UINT64_MAX;
		d_iov_set(&fetch_sgl.sg_iovs[0], (void *)buf, sizeof(buf));
		memset(buf, 'x', sizeof(buf));
		/* set expected fetch result - no modification to the given buffer */
		memset(buf2, 'x', sizeof(buf2));
		rc =
		    vos_obj_fetch(arg->ctx.tc_co_hdl, oid, epoch, 0, &dkey[3], 1, &iod, &fetch_sgl);
		assert_rc_equal(rc, 0);
		printf("size=" DF_U64 "\n", iod.iod_size);
		fflush(stdout);
		/** The punched key is not meant to be found hence the output size == 0 */
		assert_int_equal(iod.iod_size, 0);
		assert_int_equal(fetch_sgl.sg_iovs[0].iov_len, 0);
		/** and SGL buffers are not affected by the fetch operation */
		assert_memory_equal(buf, buf2, sizeof(buf));
	}

	for (i = 0; i < 2; i++) {
		iod.iod_size = strlen(first);
		d_iov_set(&sgl.sg_iovs[0], (void *)first, iod.iod_size);

		rc = dtx_begin(arg->ctx.tc_po_hdl, NULL, NULL, 256, 0, NULL, NULL, 0, DTX_LOCAL,
			       NULL, &dth);
		assert_rc_equal(rc, 0);

		rc = vos_obj_update_ex(arg->ctx.tc_co_hdl, oid, epoch++, 0, 0, &dkey[0], 1, &iod,
				       NULL, &sgl, dth);
		assert_rc_equal(rc, 0);

		rc = vos_obj_update_ex(arg->ctx.tc_co_hdl, oid, epoch++, 0, 0, &dkey[1], 1, &iod,
				       NULL, &sgl, dth);
		assert_rc_equal(rc, 0);

		rc = vos_obj_punch(arg->ctx.tc_co_hdl, oid, epoch++, 0, 0, &dkey[1], 0, NULL, dth);
		assert_rc_equal(rc, 0);

		if (i == 0) {
			/** Abort first time */
			memset(buf2, 'x', sizeof(buf2));
			passed_rc = -DER_EXIST;
		} else {
			/** Commit second time */
			passed_rc = 0;
			memcpy(buf2, first, strlen(first));
		}

		rc = dtx_end(dth, NULL, passed_rc);
		assert_rc_equal(rc, passed_rc);

		d_iov_set(&fetch_sgl.sg_iovs[0], (void *)buf, sizeof(buf));
		iod.iod_size = UINT64_MAX;
		memset(buf, 'x', sizeof(buf));
		rc =
		    vos_obj_fetch(arg->ctx.tc_co_hdl, oid, epoch, 0, &dkey[0], 1, &iod, &fetch_sgl);
		assert_rc_equal(rc, 0);
		printf("size=" DF_U64 "\n", iod.iod_size);
		fflush(stdout);
		if (i == 0) {
			/** On abort,
			 * the value is not meant to be found hence the output size == 0 */
			assert_int_equal(iod.iod_size, 0);
		} else {
			/** On commit, the value is expected to be available */
			printf("dkey[0] buf  = %.*s\n", (int)strlen(first), buf);
			printf("dkey[0] buf2 = %.*s\n", (int)strlen(first), buf2);
			fflush(stdout);
			assert_int_equal(iod.iod_size, (int)strlen(first));
			assert_int_equal(fetch_sgl.sg_iovs[0].iov_len, (int)strlen(first));
			assert_memory_equal(buf, buf2, fetch_sgl.sg_iovs[0].iov_len);
		}

		d_iov_set(&fetch_sgl.sg_iovs[0], (void *)buf, sizeof(buf));
		iod.iod_size = UINT64_MAX;
		memset(buf, 'x', sizeof(buf));
		memset(buf2, 'x', sizeof(buf2));
		rc =
		    vos_obj_fetch(arg->ctx.tc_co_hdl, oid, epoch, 0, &dkey[1], 1, &iod, &fetch_sgl);
		assert_rc_equal(rc, 0);
		printf("size=" DF_U64 "\n", iod.iod_size);
		fflush(stdout);
		/** The punched key is not meant to be found hence the output size == 0 */
		assert_int_equal(iod.iod_size, 0);
		assert_int_equal(fetch_sgl.sg_iovs[0].iov_len, 0);
		/** and SGL buffers are not affected by the fetch operation */
		assert_memory_equal(buf, buf2, sizeof(buf));
		printf("pass #%d finished\n", i);
		fflush(stdout);

		epoch++;
	}

	d_sgl_fini(&sgl, false);
	d_sgl_fini(&fetch_sgl, false);
	start_epoch = epoch + 1;
}

static const struct CMUnitTest local_tests_all[] = {
    {"DTX100: Local transaction test", local_transaction, NULL, NULL},
};

int
run_local_tests(const char *cfg)
{
	char test_name[DTS_CFG_MAX];
	int  rc;

	if (DAOS_ON_VALGRIND)
		buf_size = 100;

	rc = cmocka_run_group_tests_name(test_name, local_tests_all, setup_io, teardown_io);

	return rc;
}
