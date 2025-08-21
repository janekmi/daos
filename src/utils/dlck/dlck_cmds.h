/**
 * (C) Copyright 2025 Hewlett Packard Enterprise Development LP
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#ifndef __DLCK_CMDS__
#define __DLCK_CMDS__

enum dlck_cmd {
	DLCK_CMD_NOT_SET = -2,
	DLCK_CMD_UNKNOWN = -1,
	DLCK_CMD_WIP,
};

#define DLCK_CMD_WIP_STR "wip"

struct dlck_control;

typedef int (*dlck_cmd_func)(struct dlck_control *ctrl);

/**
 * WIP
 */
int
dlck_wip(struct dlck_control *ctrl);

#define DLCK_CMDS_FUNCS {dlck_wip}

#endif /** __DLCK_CMDS__ */
