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
#include <stdint.h>
#include <stdbool.h>

#include <daos_srv/vos_types.h>

#include "dlck_tests_common.h"

/** mocks - just to make the module under test compile */

struct vos_container;
struct vos_cont_df;
struct evt_desc_cbs;
struct vos_pool;
struct vos_object;
uint32_t vos_agg_gap;

struct vos_tls *
vos_tls_get(bool standalone)
{
	assert_true(false);
	return NULL;
}

void
vos_lru_alloc_track(void *arg, daos_size_t size)
{
	assert_true(false);
}

void
vos_lru_free_track(void *arg, daos_size_t size)
{
	assert_true(false);
}

int
vos_dtx_act_reindex(struct vos_container *cont)
{
	assert_true(false);
	return GENERIC_ERROR_RC;
}

void
vos_obj_cache_evict(struct vos_container *cont)
{
	assert_true(false);
}

int
vos_dtx_table_destroy(struct umem_instance *umm, struct vos_cont_df *cont_df)
{
	assert_true(false);
	return GENERIC_ERROR_RC;
}

void
vos_evt_desc_cbs_init(struct evt_desc_cbs *cbs, struct vos_pool *pool, daos_handle_t coh,
		      struct vos_object *obj)
{
	assert_true(false);
}

void
vos_dedup_invalidate(struct vos_pool *pool)
{
	assert_true(false);
}
