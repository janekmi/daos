/**
 * (C) Copyright 2025 Hewlett Packard Enterprise Development LP
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#define D_LOGFAC DD_FAC(tests)

#include <stdio.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>

#include <daos_errno.h>
#include <daos/debug.h>
#include <daos_srv/d_vector.h>

#define SRAND_SEED 0x1234
#define ARRAY_MAX  10

struct element {
	/**
	 * - about 3-4 elements per page
	 * - odd size
	 */
	char content[D_VECTOR_SEGMENT_SIZE / 4 - 1];
};

struct state {
	struct element *array;
	d_vector_t      vec;
};

static int
setup(void **state_ptr)
{
	static struct state state;

	srand(SRAND_SEED);

	D_ALLOC_ARRAY(state.array, ARRAY_MAX);

	for (int i = 0; i < ARRAY_MAX; ++i) {
		char pattern = rand() % CHAR_MAX;
		memset(&state.array[i], pattern, sizeof(struct element));
	}

	d_vector_init(sizeof(struct element), &state.vec);

	*state_ptr = &state;

	return 0;
}

static int
teardown(void **state_ptr)
{
	struct state *state = *state_ptr;

	D_FREE(state->array);

	return 0;
}

static void
empty_vector(void **state_ptr)
{
	struct state       *state = *state_ptr;
	struct element     *entry;
	d_vector_segment_t *segment;
	uint32_t            idx;

	d_vector_for_each_entry(entry, segment, idx, &state->vec.dv_list) {
		(void)entry;
		assert_false(true);
	}
}

static void
append_null_vector_test(void **state_ptr)
{
	struct state *state = *state_ptr;
	int           rc    = d_vector_append(NULL, &state->array[0]);
	assert_int_equal(rc, -DER_INVAL);
}

static void
append_null_entry_test(void **state_ptr)
{
	struct state *state = *state_ptr;
	int           rc    = d_vector_append(&state->vec, NULL);
	assert_int_equal(rc, -DER_INVAL);
}

static void
init_too_big_entry_size_test(void **state_ptr)
{
	/** entry_size larger than segment raw capacity */
	d_vector_t vec_large;
	size_t     large_size = D_VECTOR_SEGMENT_RAW_CAPACITY + 1;
	expect_assert_failure(d_vector_init(large_size, &vec_large))
}

static void
move_empty_vector_test(void **state_ptr)
{
	struct state *state = *state_ptr;
	d_vector_t    empty;

	d_vector_init(sizeof(struct element), &empty);
	d_vector_move(&state->vec, &empty);

	assert_int_equal(d_vector_size(&empty), 0);
	assert_int_equal(d_vector_size(&state->vec), 0);
}

static void
move_populated_vector_test(void **state_ptr)
{
	struct state *state = *state_ptr;
	d_vector_t    target;

	d_vector_init(sizeof(struct element), &target);

	/** Overload two segment. Three segments in total. */
	int total_entries = (int)(state->vec.dv_segment_capacity * 2 + 1);
	assert_true(total_entries < ARRAY_MAX);

	for (int i = 0; i < total_entries; i++) {
		int rc = d_vector_append(&state->vec, &state->array[i]);
		assert_int_equal(rc, DER_SUCCESS);
	}

	/** Move data to terget */
	d_vector_move(&target, &state->vec);

	/** The original vector should be empty */
	assert_int_equal(d_vector_size(&state->vec), 0);
	assert_true(d_list_empty(&state->vec.dv_list));

	/** Target should contain data */
	struct element     *entry;
	d_vector_segment_t *seg;
	uint32_t            idx;
	int                 count = 0;

	d_vector_for_each_entry(entry, seg, idx, &target.dv_list) {
		assert_memory_equal(entry, &state->array[count], target.dv_entry_size);
		count += 1;
	}

	assert_int_equal(count, total_entries);

	d_vector_free(&target);
}

static void
double_free_test(void **state_ptr)
{
	struct state *state = *state_ptr;

	assert_true(ARRAY_MAX > state->vec.dv_segment_capacity);

	for (int i = 0; i < ARRAY_MAX; ++i) {
		d_vector_append(&state->vec, &state->array[i]);
	}

	d_vector_free(&state->vec);
	assert_int_equal(d_vector_size(&state->vec), 0);

	d_vector_free(&state->vec);
	assert_int_equal(d_vector_size(&state->vec), 0);
}

static void
append_segment_overflow_test(void **state_ptr)
{
	struct state       *state    = *state_ptr;
	d_vector_t         *vec      = &state->vec;
	int                 capacity = (int)vec->dv_segment_capacity;
	struct element     *entry;
	d_vector_segment_t *seg;
	uint32_t            idx;

	assert_true(capacity + 1 < ARRAY_MAX);

	/** Fill the segment completely + one more item - exceeding capacity */
	for (int i = 0; i <= capacity; i++) {
		int rc = d_vector_append(vec, &state->array[i]);
		assert_int_equal(rc, DER_SUCCESS);
	}

	/** Check that we have 2 segments */
	int segment_count = 0;
	d_list_for_each_entry(seg, &vec->dv_list, dvs_link) {
		segment_count += 1;
	}
	assert_int_equal(segment_count, 2);

	/** Verify the contents */
	int entry_count = 0;
	d_vector_for_each_entry(entry, seg, idx, &vec->dv_list) {
		assert_memory_equal(entry, &state->array[entry_count], vec->dv_entry_size);
		entry_count += 1;
	}
	assert_int_equal(entry_count, capacity + 1);

	d_vector_free(vec);

	/** Check if d_vector_free works properly */
	assert_true(d_list_empty(&state->vec.dv_list));
}

static void
append_and_iterate_success(void **state_ptr)
{
	struct state       *state    = *state_ptr;
	d_vector_t         *vec      = &state->vec;
	int                 capacity = (int)vec->dv_segment_capacity;
	struct element     *entry;
	d_vector_segment_t *seg;
	uint32_t            idx;

	assert_true(capacity + 1 < ARRAY_MAX);

	for (int i = 0; i < ARRAY_MAX; ++i) {
		int rc = d_vector_append(vec, &state->array[i]);
		assert_int_equal(rc, DER_SUCCESS);
	}

	int entry_count = 0;
	d_vector_for_each_entry(entry, seg, idx, &vec->dv_list) {
		assert_memory_equal(entry, &state->array[entry_count], vec->dv_entry_size);
		entry_count += 1;
	}
	assert_int_equal(entry_count, ARRAY_MAX);

	d_vector_free(&state->vec);

	/** Check if d_vector_free works properly */
	assert_true(d_list_empty(&state->vec.dv_list));
}

static const struct CMUnitTest tests_all[] = {
    {"DVEC100: empty", empty_vector, setup, teardown},
    {"DVEC101: null_vector", append_null_vector_test, setup, teardown},
    {"DVEC102: null_entry", append_null_entry_test, setup, teardown},
    {"DVEC103: too_big_entry_size", init_too_big_entry_size_test, NULL, NULL},
    {"DVEC104: empty_vector", move_empty_vector_test, setup, teardown},
    {"DVEC105: move_populated_vector", move_populated_vector_test, setup, teardown},
    {"DVEC106: double_free", double_free_test, setup, teardown},
    {"DVEC107: segment_overflow", append_segment_overflow_test, setup, teardown},
    {"DVEC108: happy_day_scenario", append_and_iterate_success, setup, teardown},
};

int
main(int argc, char **argv)
{
	const char *test_name = "d_vector_t tests";

	d_register_alt_assert(mock_assert);

	return cmocka_run_group_tests_name(test_name, tests_all, NULL, NULL);
}
