/**
 * (C) Copyright 2025 Hewlett Packard Enterprise Development LP
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <uuid/uuid.h>

#include <argp.h>
#include <daos/debug.h>

#include "../dlck_args.h"

void
args_files_init(struct dlck_args_files *args);
int
args_files_check(struct argp_state *state, struct dlck_args_files *args);

/** mocks */

#define MOCK_ARGP_STATE ((void *)0xDEADBEEF)
struct dlck_file File;

void
__wrap_argp_failure(struct argp_state *state, int status, int errnum, const char *fmt, ...)
{
	check_expected(status);
	check_expected(errnum);
	check_expected(fmt);
}

int
__wrap_parse_file(const char *arg, struct argp_state *state, struct dlck_file **file_ptr)
{
	memset(&File, 0, sizeof(File));
	uuid_parse("12345678-1234-1234-1234-123456789abc", File.po_uuid);
	*file_ptr = &File;
	return 0;
}

/* Setup and teardown for dlck_args_files */
static int
setup(void **state)
{
	struct dlck_args_files *args;
	D_ALLOC_PTR(args);
	assert_non_null(args);
	args_files_init(args);
	*state = args;
	return 0;
}

static int
teardown(void **state)
{
	struct dlck_args_files *args = *state;
	dlck_args_files_free(args);
	D_FREE(args);
	return 0;
}

/** tests */

static void
test_init_should_initialize_list(void **state)
{
	struct dlck_args_files *args = *state;
	assert_true(d_list_empty(&args->list));
}

static void
test_check_should_fail_if_no_files(void **state)
{
	struct dlck_args_files *args       = *state;
	struct argp_state      *argp_state = MOCK_ARGP_STATE;
	assert_ptr_equal(argp_state, MOCK_ARGP_STATE);

	expect_value(__wrap_argp_failure, status, EINVAL);
	expect_value(__wrap_argp_failure, errnum, EINVAL);
	expect_string(__wrap_argp_failure, fmt, "No file chosen");

	int rc = args_files_check(argp_state, args);
	assert_int_equal(rc, EINVAL);
}

static void
test_parser_should_add_file_to_list(void **state)
{
	struct dlck_args_files *args = *state;
	extern struct argp      argp_file;

	char *argv[] = {"program_name", "--file=12345678-1234-1234-1234-123456789abc,1,2,3", NULL};

	int   rc = argp_parse(&argp_file, 2, argv, 0, NULL, args);
	assert_int_equal(rc, 0);
	assert_false(d_list_empty(&args->list));

	struct dlck_file *file = d_list_entry(args->list.next, struct dlck_file, link);
	assert_ptr_equal(file, &File);

	/** cleanup */
	D_FREE(args);
}

static void
test_free_should_cleanup_list(void **state)
{
	struct dlck_args_files *args = *state;

	struct dlck_file       *file;
	D_ALLOC_PTR(file);

	d_list_add_tail(&file->link, &args->list);

	dlck_args_files_free(args);
	assert_true(d_list_empty(&args->list));
}

static const struct CMUnitTest tests_all[] = {
    {"DARG100: init", test_init_should_initialize_list, setup, teardown},
    {"DARG101: check", test_check_should_fail_if_no_files, setup, teardown},
    {"DARG102: parser", test_parser_should_add_file_to_list, setup, NULL},
    {"DARG103: free", test_free_should_cleanup_list, setup, teardown},
};

int
main(void)
{
	const char *test_name = "dlck_args_files.c tests";

	return cmocka_run_group_tests_name(test_name, tests_all, NULL, NULL);
}
