/**
 * (C) Copyright 2023 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

struct umem_intercept_cbs {
	void (*tx_begin)(void);
	void (*tx_commit)(void);
};

/**
 * Initialize UMEM intercept.
 *
 * \param poh			[IN]	The pool handle
 * \param umem_intercept_cbs	[IN]	Interception call-backs
 */
void
umem_intercept_init(daos_handle_t poh, struct umem_intercept_cbs *cbs);

/**
 * Finalize UMEM intercept.
 */
void
umem_intercept_fini();
