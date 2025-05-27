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

/** set --version */

#define _STRINGIFY(x) #x
#define STRINGIFY(x)  _STRINGIFY(x)

#define DAOS_VERSION_STR                                                                           \
	STRINGIFY(DAOS_VERSION_MAJOR)                                                              \
	"." STRINGIFY(DAOS_VERSION_MINOR) "." STRINGIFY(DAOS_VERSION_FIX)

const char *argp_program_version = "dlck " DAOS_VERSION_STR;

/** all short options and all groups to avoid conflicts */

#define KEY_COMMON_WRITE_MODE     'w'
#define KEY_COMMON_CO_UUID        'q'
#define KEY_COMMON_JOBS           'j'
#define KEY_COMMON_CMD            'c'

#define GROUP_FILE                1
#define GROUP_COMMON              2
#define GROUP_AVAILABLE_CMDS      3
#define GROUP_AUTOMAGIC           -1 /** yes, -1 is the last group */

/** argument parsers and helper functions */

static void
args_init(struct dlck_args *args)
{
	memset(args, 0, sizeof(struct dlck_args));
	/** set defaults */
	uuid_clear(args->common.co_uuid);
	args->common.write_mode = false; /** dry run */
	args->common.jobs       = 1;
	args->common.cmd        = DLCK_CMD_NOT_SET;
}

static int
args_files_append(struct dlck_args *args, char *arg, int files_num_max)
{
	char **newptr;

	if (args->files_num_max == 0) {
		D_ALLOC_ARRAY_NZ(newptr, files_num_max);
		if (newptr == NULL) {
			return ENOMEM;
		}
		args->files         = newptr;
		args->files_num_max = files_num_max;
	}

	if (args->files_num == args->files_num_max) {
		return ARGP_ERR_UNKNOWN;
	}

	args->files[args->files_num] = arg;
	args->files_num += 1;

	return 0;
}

#define RETURN_FAIL(STATE, ERRNUM, ...)                                                            \
	argp_failure(STATE, ERRNUM, ERRNUM, __VA_ARGS__);                                          \
	return ERRNUM;

static int
args_check(struct argp_state *state, struct dlck_args *args)
{
	if (args->common.cmd == DLCK_CMD_NOT_SET) {
		RETURN_FAIL(state, EINVAL, "Command not set");
	}
	if (args->files_num == 0) {
		RETURN_FAIL(state, EINVAL, "No file chosen");
	}
	return 0;
}

static enum dlck_cmd
command_parse(const char *arg)
{
	if (strcmp(arg, DLCK_CMD_DTX_ACT_RECOVER_STR) == 0) {
		return DLCK_CMD_DTX_ACT_RECOVER;
	}

	return DLCK_CMD_UNKNOWN;
}

static error_t
parser_common(int key, char *arg, struct argp_state *state)
{
	struct dlck_args *args = state->input;
	int               tmp;
	uuid_t            tmp_uuid;
	int               ret;

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

	/** after -- only the list of files is expected */
	if (state->quoted != 0) {
		/** This number decreases with each consumed file but at first it is the actual max.
		 */
		int files_num_max = state->argc - state->next + 1;
		ret               = args_files_append(args, arg, files_num_max);
		if (ret != 0) {
			RETURN_FAIL(state, ret, "%d", ret);
		}
		return 0;
	}

	/** options */
	switch (key) {
	case KEY_COMMON_WRITE_MODE:
		args->common.write_mode = true;
		break;
	case KEY_COMMON_CO_UUID:
		ret = uuid_parse(arg, tmp_uuid);
		if (ret != 0) {
			RETURN_FAIL(state, EINVAL, "Malformed uuid: %s", arg);
		}
		uuid_copy(args->common.co_uuid, tmp_uuid);
		break;
	case KEY_COMMON_JOBS:
		tmp = atoi(arg);
		if (tmp < 1 || tmp > UINT_MAX) {
			RETURN_FAIL(state, EINVAL, "N < 1 or N > UINT_MAX: %s", arg);
		}
		args->common.jobs = tmp;
		break;
	case KEY_COMMON_CMD:
		args->common.cmd = command_parse(arg);
		if (args->common.cmd == DLCK_CMD_UNKNOWN) {
			RETURN_FAIL(state, EINVAL, "Unknown command: %s", arg);
		}
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

/** helper definitions */

#define OPT_HEADER(HEADER, GROUP) {0, 0, 0, 0, HEADER, GROUP}

#define POSITIONAL(ARG, DESC, GROUP) {ARG, 0, 0, OPTION_DOC, DESC, GROUP}

#define LIST_ENTRY(CMD, DESC)        {CMD, 0, 0, OPTION_DOC, DESC}

/** complete help list (in order) */

static char               usage[] = "-- [FILE...]";
/** XXX provide more details here. */
static char               doc[]   = "\nDAOS Local Consistency Checker (dlck)";

static struct argp_option common_options[] = {
    POSITIONAL("FILE", "VOS file(s) to run the command against.", GROUP_FILE),
    OPT_HEADER("Common options:", GROUP_COMMON),
    /** entries below inherits the group number of the header entry */
    {"write-mode", KEY_COMMON_WRITE_MODE, 0, 0, "Open VOS files in write mode."},
    {"co-uuid", KEY_COMMON_CO_UUID, "UUID", 0,
     "UUID of a container to process. If not provided all containers are processed."},
    {"jobs", KEY_COMMON_JOBS, "N", 0, "Allow N jobs at once. One job by default."},
    {"cmd", KEY_COMMON_CMD, "CMD", 0, "Command (Required). Please see available commands below."},
    {0}};

static struct argp_option _cmds_list[] = {
    OPT_HEADER("Available commands:", GROUP_AVAILABLE_CMDS),
    /** entries below inherits the group number of the header entry */
    LIST_ENTRY(DLCK_CMD_DTX_ACT_RECOVER_STR, "Active DTX entries' records recovery."),
    {0}};

static struct argp_option _automagic[] = {OPT_HEADER("Other options:", GROUP_AUTOMAGIC), {0}};

/** glue everything together */

static struct argp        cmds_list = {_cmds_list, NULL};

static struct argp        automagic = {_automagic, NULL};

static struct argp_child  children[] = {{&cmds_list}, {&automagic}, {0}};

static struct argp        argp = {common_options, parser_common, usage, doc, children};

/** entry point */

void
dlck_args_parse(int argc, char *argv[], struct dlck_args *args)
{
	error_t ret = argp_parse(&argp, argc, argv, ARGP_IN_ORDER, 0, args);

	if (ret != 0) {
		D_ERROR("Parsing arguments failed: %d", ret);
		exit(ret);
	}
}
