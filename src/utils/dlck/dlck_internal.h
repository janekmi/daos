/**
 * (C) Copyright 2025 Hewlett Packard Enterprise Development LP
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#ifndef __DLCK_INTERNAL__
#define __DLCK_INTERNAL__

#include <daos_srv/dlck.h>

#include "dlck_args.h"
#include "dlck_engine.h"

/**
 * @struct xstream_arg
 *
 * Arguments passed to the main ULT on each of the execution streams.
 */
struct xstream_arg {
	struct dlck_control *ctrl;   /** Control state. */
	struct dlck_engine  *engine; /** Engine itself. */
	struct dlck_xstream *xs;     /** The execution stream the ULT is run in. */
	struct dlck_stats    stats;  /** Cumulative stats for all the DLCK calls. */
	int                  rc;     /** [out] return code */
};

#endif /** __DLCK_INTERNAL__ */
