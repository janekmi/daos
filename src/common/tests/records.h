#include <stdint.h>
#include <stddef.h>

#define RECORD_IDX_UNKNOWN (-1)

/*
 * Records is just a different name for copies of entries kept by test for
 * validation.
 */
struct record {
	uint64_t key;
	size_t value_pos;
	char *value;
	size_t value_size;
};

extern size_t records_used;
extern struct record *records;
extern char *values;

void value_rand(struct record *rec);
void record_delete(uint64_t key, int idx);
void record_check(uint64_t key, char *value, size_t value_size);
size_t record_get_empty();
struct record *record_get(uint64_t key);
