/**
 * (C) Copyright 2023 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>
#include <daos/mem.h>
#include <vos_internal.h>

#include "umem_intercept.h"

/**
 * Copy of pool handle provided on init.
 */
static daos_handle_t              poh_copy;

/**
 * Copy of the UMEM ops extracted from the provided pool on init.
 */
static umem_ops_t                *ops_copy;

/**
 * Call-backs to be called on intercepting UMEM calls.
 */
static struct umem_intercept_cbs *cbs;

static int
intercept_tx_begin(struct umem_instance *umm, struct umem_tx_stage_data *txd)
{
	print_message("tx_begin\n");
	cbs->tx_begin();
	return ops_copy->mo_tx_begin(umm, txd);
}

static umem_off_t
intercept_reserve(struct umem_instance *umm, void *act, size_t size, unsigned int type_num)
{
	print_message("reserve\n");
	return ops_copy->mo_reserve(umm, act, size, type_num);
}

static void *
intercept_atomic_copy(struct umem_instance *umm, void *dest, const void *src, size_t len,
		      enum acopy_hint hint)
{
	print_message("atomic_copy\n");
	return ops_copy->mo_atomic_copy(umm, dest, src, len, hint);
}

static int
intercept_tx_add_ptr(struct umem_instance *umm, void *ptr, size_t size)
{
	print_message("tx_add_ptr\n");
	return ops_copy->mo_tx_add_ptr(umm, ptr, size);
}

static int
intercept_tx_commit(struct umem_instance *umm, void *data)
{
	print_message("tx_commit\n");
	cbs->tx_commit();
	return ops_copy->mo_tx_commit(umm, data);
}

static int
intercept_tx_publish(struct umem_instance *umm, void *actv, int actv_cnt)
{
	print_message("tx_publish\n");
	return ops_copy->mo_tx_publish(umm, actv, actv_cnt);
}

static int
intercept_tx_xadd(struct umem_instance *umm, umem_off_t umoff, uint64_t offset, size_t size,
		  uint64_t flags)
{
	print_message("tx_xadd\n");
	return ops_copy->mo_tx_xadd(umm, umoff, offset, size, flags);
}

static int
intercept_tx_free(struct umem_instance *umm, umem_off_t umoff)
{
	print_message("tx_free\n");
	return ops_copy->mo_tx_free(umm, umoff);
}

/**
 * Setting not implemented UMEM calls to NULL will just force them to be silently skipped. It is
 * more desirable to fail badly with SIGSEGV indicating a missing implementation.
 */
#define BAD_PTR (void *)0xbaadf00d

static umem_ops_t ops_intercept = {
    .mo_tx_free         = intercept_tx_free,
    .mo_tx_alloc        = BAD_PTR,
    .mo_tx_add          = BAD_PTR,
    .mo_tx_xadd         = intercept_tx_xadd,
    .mo_tx_add_ptr      = intercept_tx_add_ptr,
    .mo_tx_abort        = BAD_PTR,
    .mo_tx_begin        = intercept_tx_begin,
    .mo_tx_commit       = intercept_tx_commit,
    .mo_tx_stage        = BAD_PTR,
    .mo_reserve         = intercept_reserve,
    .mo_defer_free      = BAD_PTR,
    .mo_cancel          = BAD_PTR,
    .mo_tx_publish      = intercept_tx_publish,
    .mo_atomic_copy     = intercept_atomic_copy,
    .mo_atomic_alloc    = BAD_PTR,
    .mo_atomic_free     = BAD_PTR,
    .mo_atomic_flush    = BAD_PTR,
    .mo_tx_add_callback = BAD_PTR,
};

void
umem_intercept_init(daos_handle_t poh, struct umem_intercept_cbs *cbs_in)
{
	struct vos_pool *vp = (struct vos_pool *)poh.cookie;
	poh_copy            = poh;
	ops_copy            = vp->vp_umm.umm_ops;
	cbs                 = cbs_in;
	vp->vp_umm.umm_ops  = &ops_intercept;
}

void
umem_intercept_fini()
{
	struct vos_pool *vp = (struct vos_pool *)poh_copy.cookie;
	vp->vp_umm.umm_ops  = ops_copy;
}
