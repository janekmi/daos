/**
 * (C) Copyright 2025 Hewlett Packard Enterprise Development LP
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#ifndef __DLCK_COMMON__
#define __DLCK_COMMON__

#include <daos_types.h>

/**
 * XXX
 */
int
dlck_pool_mkdir(const char *storage_path, struct dlck_file *file);

/**
 * XXX
 */
int
dlck_pool_open(const char *storage_path, struct dlck_file *file, int tgt_id, daos_handle_t *poh);

#endif /** __DLCK_COMMON__ */