
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "interceptor.h"
#include "records.h"

#define RECORDS_INCREASE 1024

size_t records_total = 0;
size_t records_used = 0;
struct record *records = NULL;

#define VALUES_INCREASE 4096
#define VALUE_MAX 256

size_t values_size = 0;
size_t values_pos = 0;
char *values = NULL;

size_t
record_get_empty() {
	if (records_used == records_total) {
        size_t size = sizeof(struct record) * (records_total + RECORDS_INCREASE);
        dump(
"{\n"
"	records = (struct record *)realloc(records, %llu);\n"
"}\n",
            size);
		records = (struct record *)realloc(records, size);
		records_total += RECORDS_INCREASE;
	}
	records[records_used].value = NULL;
	return records_used++;
}

struct record *
record_get(uint64_t key)
{
	for (int i = 0; i < records_used; ++i) {
		if (records[i].key == key) {
			return &records[i];
		}
	}
	return NULL;
}

void
record_check(uint64_t key, char *value, size_t value_size)
{
	struct record *record = record_get(key);
	if (record == NULL) {
		fail_msg("Can not find a record of key: %lu", key);
	}
	assert_int_equal(record->value_size, value_size);
	assert_int_equal(memcmp(record->value, value, sizeof(char) * value_size), 0);
}

void
record_delete(uint64_t key, int idx) {
	if (idx == RECORD_IDX_UNKNOWN) {
		for (int i = 0; i < records_used; ++i) {
			if (records[i].key == key) {
				idx = i;
				break;
			}
		}
		assert_int_not_equal(idx, RECORD_IDX_UNKNOWN);
	}
	printf("Deleted [%d]: %lu\n", idx, records[idx].key);
	if (idx != records_used - 1) {
		memcpy(&records[idx], &records[idx + 1],
			sizeof(struct record) * (records_used - idx - 1));
	}
	records_used -= 1;
}

void
value_rand(struct record *rec)
{
	rec->value_size = rand() % (VALUE_MAX - 2) + 2;
	if (values_pos + rec->value_size > values_size) {
		unsigned increase = (rec->value_size / VALUES_INCREASE + 1) * VALUES_INCREASE;
        size_t size = sizeof(char) * (values_size + increase);
        dump(
"{\n"
"	values = (char *)realloc(values, %llu);\n"
"}\n",
            size);
		values = (char *)realloc(values, size);
		values_size += increase;
		// fix pointers in the records
		for (int i = 0; i < records_used; ++i) {
			if (records[i].value == NULL) {
				continue;
			}
			records[i].value = &values[records[i].value_pos];
		}
	}
	rec->value_pos = values_pos;
	rec->value = &values[values_pos];
	values_pos += rec->value_size;
	for (int i = 0; i < rec->value_size; ++i) {
		rec->value[i] = rand() % 256;
		rec->value[i] = 'a'; // XXX
	}
	// XXX
	assert_true(rec->value_size >= 2);
	rec->value[1]   = '\0';
	rec->value_size = 2;
}
