/**
 * (C) Copyright 2025 Hewlett Packard Enterprise Development LP
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#ifndef __DAOS_DLCK_H__
#define __DAOS_DLCK_H__

#include <daos/btree.h>

/**
 * @struct dlck_stats
 *
 * Execution statistics.
 */
struct dlck_stats {
	unsigned touched;
};

#define DLCK_PRINT_INDENT_MAX 10
#define DLCK_PRINT_INDENT     '-'

/**
 * @struct dlck_print
 *
 * Printer for DLCK purposes.
 */
struct dlck_print {
	int (*dp_printf)(const char *fmt, ...);
	int  level;
	char prefix[DLCK_PRINT_INDENT_MAX + 2]; /** ' ' and '\0' */
};

#define DLCK_PRINT(print, msg)                 (void)print->dp_printf("%s" msg, print->prefix)

#define DLCK_PRINTF(print, fmt, ...)           (void)print->dp_printf("%s" fmt, print->prefix, __VA_ARGS__)

#define DLCK_PRINT_WO_PREFIX(print, msg)       (void)print->dp_printf(msg)

#define DLCK_PRINTF_WO_PREFIX(print, fmt, ...) (void)print->dp_printf(fmt, __VA_ARGS__)

#define DLCK_PRINT_YES_NO(print, cond)         DLCK_PRINTF_WO_PREFIX(print, "%s.\n", (cond) ? "yes" : "no")

#define DLCK_PRINT_OK(print)                   DLCK_PRINT_WO_PREFIX(print, "ok.\n")

#define DLCK_PRINT_RC(print, rc)               DLCK_PRINTF_WO_PREFIX(print, DF_RC "\n", DP_RC(rc))

inline void
dlck_print_indent_set(struct dlck_print *dp)
{
	memset(dp->prefix, DLCK_PRINT_INDENT, DLCK_PRINT_INDENT_MAX);
	if (dp->level > 0) {
		dp->prefix[dp->level]     = ' ';
		dp->prefix[dp->level + 1] = '\0';
	} else {
		dp->prefix[0] = '\0';
	}
}

inline void
dlck_print_indent_inc(struct dlck_print *dp)
{
	if (dp->level == DLCK_PRINT_INDENT_MAX) {
		DLCK_PRINT(dp, "Max indent reached.\n");
		return;
	}

	dp->level++;
	dlck_print_indent_set(dp);
}

inline void
dlck_print_indent_dec(struct dlck_print *dp)
{
	if (dp->level == 0) {
		DLCK_PRINT(dp, "Min indent reached.\n");
		return;
	}

	dp->level--;
	dlck_print_indent_set(dp);
}

/**
 * WIP
 */
int
dlck_pool_check(const char *path, uuid_t po_uuid, unsigned int flags, struct dlck_print *dp);

/**
 * WIP
 */
int
dlck_dbtree_check_inplace(struct btr_root *root, struct dlck_print *dp);

#endif /* __DAOS_DLCK_H__ */
