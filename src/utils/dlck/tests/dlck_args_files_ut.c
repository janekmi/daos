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
extern struct argp argp_file;

/** mocks */

struct argp_state  Argp_state;
#define MOCK_ARGP_STATE (&Argp_state)
struct dlck_file File;
static char      mock_arg_str[] = "mock_arg_value";
#define MOCK_ARG mock_arg_str

void
__wrap_argp_failure(struct argp_state *state, int status, int errnum, const char *fmt, ...)
{
	assert_ptr_equal(state, MOCK_ARGP_STATE);
	check_expected(status);
	check_expected(errnum);
	assert_non_null(fmt);
}

int
parse_file(const char *arg, struct argp_state *state, struct dlck_file **file_ptr)
{
	assert_ptr_equal(arg, MOCK_ARG);
	assert_ptr_equal(state, MOCK_ARGP_STATE);
	assert_non_null(file_ptr);
	int rc = mock_type(int);
	if (rc == DER_SUCCESS) {
		memset(&File, 0, sizeof(File));
		*file_ptr = &File;
	}
	return rc;
}

void *
__wrap_d_calloc(size_t count, size_t eltsize)
{
	return test_calloc(count, eltsize);
}

void
__wrap_d_free(void *ptr)
{
	test_free(ptr);
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
	if (args) {
		D_FREE(args);
	}
	return 0;
}

/** tests */

static void
test_files_free_empty_list(void **unused)
{
	/** the work is done by setup and teardown functions */;
}

static void
test_init_should_initialize_list(void **state)
{
	struct dlck_args_files *args = *state;

	assert_true(d_list_empty(&args->list));
}

static void
test_check_should_fail_if_no_files(void **state)
{
	struct dlck_args_files *args = *state;

	expect_value(__wrap_argp_failure, status, EINVAL);
	expect_value(__wrap_argp_failure, errnum, EINVAL);

	int rc = args_files_check(MOCK_ARGP_STATE, args);
	assert_int_equal(rc, EINVAL);
}

static void
test_check_should_succeed_with_file(void **state)
{
	struct dlck_args_files *args = *state;

	d_list_add_tail(&File.link, &args->list);

	int rc = args_files_check(MOCK_ARGP_STATE, args);
	assert_int_equal(rc, 0);
}

static void
test_parser_should_add_file_to_list(void **state)
{
	struct dlck_args_files *args = *state;

	memset(&Argp_state, 0, sizeof(Argp_state));
	Argp_state.input = args;

	will_return(parse_file, 0);

	int rc = argp_file.parser(KEY_FILES, MOCK_ARG, &Argp_state);
	assert_int_equal(rc, 0);

	assert_false(d_list_empty(&args->list));

	struct dlck_file *file = d_list_entry(args->list.next, struct dlck_file, link);
	assert_ptr_equal(file, &File);
}

static void
test_free_should_cleanup_list(void **state)
{
	struct dlck_args_files *args = *state;

	struct dlck_file       *file;
	D_ALLOC_PTR(file);

	d_list_add_tail(&file->link, &args->list);

	dlck_args_files_free(args);
}

static void
test_free_should_cleanup_multiple_files(void **state)
{
	struct dlck_args_files *args = *state;

	for (int i = 0; i < 3; i++) {
		struct dlck_file *file;
		D_ALLOC_PTR(file);
		d_list_add_tail(&file->link, &args->list);
	}

	dlck_args_files_free(args);
	assert_true(d_list_empty(&args->list));
}

static void
test_parser_end_fails_without_file(void **state)
{
	struct dlck_args_files args;
	args_files_init(&args);

	will_return(parse_file, EINVAL);

	int rc = argp_file.parser(KEY_FILES, MOCK_ARG, &Argp_state);
	assert_int_equal(rc, EINVAL);
	assert_true(d_list_empty(&args.list));
}

static void
test_end_succeeds_with_file(void **state)
{
	struct dlck_args_files args;
	args_files_init(&args);
	Argp_state.input = &args;

	will_return(parse_file, DER_SUCCESS);

	int rc = argp_file.parser(KEY_FILES, MOCK_ARG, &Argp_state);
	assert_int_equal(rc, 0);
	assert_false(d_list_empty(&args.list));
}

static void
test_key_files_parse_file_fails(void **state)
{
	struct dlck_args_files args;
	args_files_init(&args);

	will_return(parse_file, EINVAL);

	int rc = argp_file.parser(KEY_FILES, MOCK_ARG, &Argp_state);
	assert_int_equal(rc, EINVAL);
	assert_true(d_list_empty(&args.list));
}

static void
test_parser_unknown_key_should_return_zero(void **state)
{
	struct dlck_args_files *args = *state;
	Argp_state.input             = args;

	int rc = argp_file.parser(9999, NULL, &Argp_state);
	assert_int_equal(rc, ARGP_ERR_UNKNOWN);
}

static const struct CMUnitTest tests_all[] = {
    {"DARG100: free empty", test_files_free_empty_list, setup, teardown},
    {"DARG101: init", test_init_should_initialize_list, setup, teardown},
    {"DARG102: check fail", test_check_should_fail_if_no_files, setup, teardown},
    {"DARG103: check success", test_check_should_succeed_with_file, setup, NULL},
    {"DARG104: parser add", test_parser_should_add_file_to_list, setup, NULL},
    {"DARG105: free list", test_free_should_cleanup_list, setup, teardown},
    {"DARG106: free list multiple", test_free_should_cleanup_multiple_files, setup, teardown},
    {"DARG107: key end fail", test_parser_end_fails_without_file, setup, teardown},
    {"DARG108: key end ok", test_end_succeeds_with_file, setup, teardown},
    {"DARG109: parser fail", test_key_files_parse_file_fails, setup, teardown},
    {"DARG110: unknown key", test_parser_unknown_key_should_return_zero, setup, teardown},
};

int
main(void)
{
	const char *test_name = "dlck_args_files.c tests";

	return cmocka_run_group_tests_name(test_name, tests_all, NULL, NULL);
}
