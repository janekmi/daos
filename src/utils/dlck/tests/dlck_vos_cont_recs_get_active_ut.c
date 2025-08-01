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
#include <daos_types.h>
#include <daos_srv/d_vector.h>
#include <daos_srv/dlck.h>
#include <daos_srv/vos_types.h>

#include "dlck_tests_common.h"

/** non-public definitions */

int
dlck_rec_get_active_cb(daos_handle_t ih, vos_iter_entry_t *entry, vos_iter_type_t type,
		       vos_iter_param_t *param, void *cb_arg, unsigned int *acts);

/** mocks */

#define COH_MOCK_INIT {.cookie = 0x300413}
#define ENTRY_MOCK    ((vos_iter_entry_t *)0xE4141)
#define PARAM_MOCK    ((vos_iter_param_t *)0x5A4A3)
#define ACTS_MOCK     ((unsigned int *)0xAC15)
#define VOS_ITER_MOCK ((struct vos_iterator *)0xC0511E4)
#define DV_MOCK       ((d_vector_t *)0xDCE0C4)

/** globals */

static const daos_handle_t    Coh = COH_MOCK_INIT;
static struct dlck_stats      Stats;
static const vos_iter_param_t Param = {
    .ip_hdl   = COH_MOCK_INIT,
    .ip_epr   = {.epr_hi = DAOS_EPOCH_MAX},
    .ip_flags = VOS_IT_FOR_CHECK,
};
static const struct vos_iter_anchors Anchors_zeroed;
static const struct dlck_iter_bundle Bundle = {.coh = COH_MOCK_INIT, .ds = &Stats, .dv = DV_MOCK};
static const daos_handle_t           Ih     = {.cookie = (uint64_t)VOS_ITER_MOCK};

/** mocks */

struct vos_iterator;
struct dtx_handle;

int
dlck_obj_get_active(daos_handle_t coh, struct vos_iterator *iter, d_vector_t *dv)
{
	assert_int_equal(coh.cookie, Coh.cookie);
	assert_ptr_equal(iter, VOS_ITER_MOCK);
	assert_ptr_equal(dv, DV_MOCK);
	return mock_type(int);
}

int
dlck_irec_get_active(daos_handle_t coh, struct vos_iterator *iter, d_vector_t *dv)
{
	assert_int_equal(coh.cookie, Coh.cookie);
	assert_ptr_equal(iter, VOS_ITER_MOCK);
	assert_ptr_equal(dv, DV_MOCK);
	return mock_type(int);
}

int
dlck_sv_add_if_active(daos_handle_t coh, struct vos_iterator *iter, d_vector_t *dv)
{
	assert_int_equal(coh.cookie, Coh.cookie);
	assert_ptr_equal(iter, VOS_ITER_MOCK);
	assert_ptr_equal(dv, DV_MOCK);
	return mock_type(int);
}

int
dlck_ev_add_if_active(daos_handle_t coh, struct vos_iterator *iter, d_vector_t *dv)
{
	assert_int_equal(coh.cookie, Coh.cookie);
	assert_ptr_equal(iter, VOS_ITER_MOCK);
	assert_ptr_equal(dv, DV_MOCK);
	return mock_type(int);
}

int
vos_iterate(vos_iter_param_t *param, vos_iter_type_t type, bool recursive,
	    struct vos_iter_anchors *anchors, vos_iter_cb_t pre_cb, vos_iter_cb_t post_cb,
	    void *arg, struct dtx_handle *dth)
{
	struct dlck_iter_bundle *bundle = arg;

	assert_ptr_not_equal(param, NULL);
	assert_true(memcmp(param, &Param, sizeof(Param)) == 0);
	assert_int_equal(type, VOS_ITER_OBJ);
	assert_true(recursive);
	assert_ptr_not_equal(anchors, NULL);
	assert_true(memcmp(anchors, &Anchors_zeroed, sizeof(Anchors_zeroed)) == 0);
	assert_ptr_equal(pre_cb, dlck_rec_get_active_cb);
	assert_ptr_equal(post_cb, NULL);
	assert_ptr_not_equal(bundle, NULL);
	assert_true(memcmp(bundle, &Bundle, sizeof(Bundle)) == 0);
	assert_ptr_equal(dth, NULL);

	return mock_type(int);
}

/** tests */

static void
test_vos_iterate_fails(void **unused)
{
	int rc;

	will_return(vos_iterate, GENERIC_ERROR_RC);
	rc = dlck_vos_cont_recs_get_active(Coh, DV_MOCK, &Stats);
	assert_int_equal(rc, GENERIC_ERROR_RC);
}

#define DLCK_REC_GET_ACTIVE_CB(RC, TYPE)                                                           \
	(RC) =                                                                                     \
	    dlck_rec_get_active_cb(Ih, ENTRY_MOCK, (TYPE), PARAM_MOCK, (void *)&Bundle, ACTS_MOCK)

static void
test_dlck_obj_get_active_fails(void **unused)
{
	int rc;

	will_return(dlck_obj_get_active, GENERIC_ERROR_RC);
	DLCK_REC_GET_ACTIVE_CB(rc, VOS_ITER_OBJ);
	assert_int_equal(rc, GENERIC_ERROR_RC);
}

static void
test_dlck_irec_get_active_AKEY_fails(void **unused)
{
	int rc;

	will_return(dlck_irec_get_active, GENERIC_ERROR_RC);
	DLCK_REC_GET_ACTIVE_CB(rc, VOS_ITER_AKEY);
	assert_int_equal(rc, GENERIC_ERROR_RC);
}

static void
test_dlck_irec_get_active_DKEY_fails(void **unused)
{
	int rc;

	will_return(dlck_irec_get_active, GENERIC_ERROR_RC);
	DLCK_REC_GET_ACTIVE_CB(rc, VOS_ITER_DKEY);
	assert_int_equal(rc, GENERIC_ERROR_RC);
}

static void
test_dlck_sv_add_if_active_fails(void **unused)
{
	int rc;

	will_return(dlck_sv_add_if_active, GENERIC_ERROR_RC);
	DLCK_REC_GET_ACTIVE_CB(rc, VOS_ITER_SINGLE);
	assert_int_equal(rc, GENERIC_ERROR_RC);
}

static void
test_dlck_ev_add_if_active_fails(void **unused)
{
	int rc;

	will_return(dlck_ev_add_if_active, GENERIC_ERROR_RC);
	DLCK_REC_GET_ACTIVE_CB(rc, VOS_ITER_RECX);
	assert_int_equal(rc, GENERIC_ERROR_RC);
}

static const struct CMUnitTest tests_all[] = {
    {"D100: vos_iterate() fails", test_vos_iterate_fails, NULL, NULL},
    {"D101: dlck_obj_get_active() fails", test_dlck_obj_get_active_fails, NULL, NULL},
    {"D102: dlck_irec_get_active() fails [AKEY]", test_dlck_irec_get_active_AKEY_fails, NULL, NULL},
    {"D103: dlck_irec_get_active() fails [DKEY]", test_dlck_irec_get_active_DKEY_fails, NULL, NULL},
    {"D104: dlck_sv_add_if_active() fails", test_dlck_sv_add_if_active_fails, NULL, NULL},
    {"D105: dlck_ev_add_if_active() fails", test_dlck_ev_add_if_active_fails, NULL, NULL},
};

int
main(int argc, char **argv)
{
	const char *test_name = "dlck_vos_cont_recs_get_active() tests";

	return cmocka_run_group_tests_name(test_name, tests_all, NULL, NULL);
}
