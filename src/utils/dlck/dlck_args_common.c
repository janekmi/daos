/**
 * (C) Copyright 2025 Hewlett Packard Enterprise Development LP
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#define D_LOGFAC DD_FAC(dlck)

#include <daos_errno.h>
#include <daos/debug.h>
#include <daos_version.h>
#include <argp.h>

#include "dlck_args.h"

#define GROUP_OPTIONS        1
#define GROUP_AVAILABLE_CMDS 2

static struct argp_option common_options[] = {
    OPT_HEADER("Options:", GROUP_OPTIONS),
    {"write_mode", KEY_COMMON_WRITE_MODE, 0, 0, "Make changes persistent."},
    {"file", KEY_COMMON_FILE, "UUID,TARGET", 0,
     "Pool UUID and set of targets. Can be used more than once."},
    {"co_uuid", KEY_COMMON_CO_UUID, "UUID", 0,
     "UUID of a container to process. If not provided all containers are processed."},
    {"cmd", KEY_COMMON_CMD, "CMD", 0, "Command (Required). Please see available commands below."},
    OPT_HEADER("Available commands:", GROUP_AVAILABLE_CMDS),
    /** entries below inherits the group number of the header entry */
    LIST_ENTRY(DLCK_CMD_DTX_ACT_RECOVER_STR, "Active DTX entries' records recovery."),
    {0}};

static void
args_init(struct dlck_args_common *args)
{
	memset(args, 0, sizeof(struct dlck_args));
	/** set defaults */
	D_INIT_LIST_HEAD(&args->files);
	uuid_clear(args->co_uuid);
	args->write_mode = false; /** dry run */
	args->cmd        = DLCK_CMD_NOT_SET;
}

static int
args_check(struct argp_state *state, struct dlck_args_common *args)
{
	if (args->cmd == DLCK_CMD_NOT_SET) {
		RETURN_FAIL(state, EINVAL, "Command not set");
	}
	if (d_list_empty(&args->files)) {
		RETURN_FAIL(state, EINVAL, "No file chosen");
	}
	return 0;
}

static enum dlck_cmd
parse_command(const char *arg)
{
	if (strcmp(arg, DLCK_CMD_DTX_ACT_RECOVER_STR) == 0) {
		return DLCK_CMD_DTX_ACT_RECOVER;
	}

	return DLCK_CMD_UNKNOWN;
}

error_t
parser_common(int key, char *arg, struct argp_state *state)
{
	struct dlck_args_common *args = state->input;
	struct dlck_file        *file;
	uuid_t                   tmp_uuid;
	int                      rc = 0;

	/** state changes */
	switch (key) {
	case ARGP_KEY_INIT:
		args_init(args);
		return 0;
	case ARGP_KEY_END:
		return args_check(state, args);
	case ARGP_KEY_SUCCESS:
	case ARGP_KEY_FINI:
		return 0;
	}

	/** options */
	switch (key) {
	case KEY_COMMON_WRITE_MODE:
		args->write_mode = true;
		break;
	case KEY_COMMON_FILE:
		rc = parse_file(arg, state, &file);
		if (rc == 0) {
			d_list_add_tail(&file->link, &args->files);
		}
		break;
	case KEY_COMMON_CO_UUID:
		rc = uuid_parse(arg, tmp_uuid);
		if (rc != 0) {
			RETURN_FAIL(state, EINVAL, "Malformed uuid: %s", arg);
		}
		uuid_copy(args->co_uuid, tmp_uuid);
		break;
	case KEY_COMMON_CMD:
		args->cmd = parse_command(arg);
		if (args->cmd == DLCK_CMD_UNKNOWN) {
			RETURN_FAIL(state, EINVAL, "Unknown command: %s", arg);
		}
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}

	return rc;
}

struct argp argp_common = {common_options, parser_common, NULL, NULL, NULL};
