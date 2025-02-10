#include <stdint.h>
#include <daos/btree.h>

#include "records.h"

extern struct umem_attr *ik_uma;
extern umem_off_t ik_root_off;
extern daos_handle_t ik_toh;
static daos_handle_t iter = DAOS_HDL_INVAL;

void run_dump(void) {

{
	int tree_class = 100;
	uint64_t tree_feats = 1;
	unsigned int tree_order = 11;
	int rc = dbtree_create(tree_class, tree_feats, tree_order, ik_uma, &ik_root_off, &ik_toh);
	assert(rc == 0);
}
{
	int rc = dbtree_close(ik_toh);
	assert(rc == 0);
	ik_toh = DAOS_HDL_INVAL;
}
{
	int rc = dbtree_open(ik_root_off, ik_uma, &ik_toh);
	assert(rc == 0);
}
{
	records = (struct record *)realloc(records, 320);
}
{
	values = (char *)realloc(values, 170);
}
{
	uint64_t key = 1502197874;
	d_iov_t key_iov = {.iov_buf = &key, .iov_buf_len = 8, .iov_len = 8};
	char val[] = {0x61, 0};
	d_iov_t val_iov = {.iov_buf = val, .iov_buf_len = 2, .iov_len = 2};
	int rc = dbtree_update(ik_toh, &key_iov, &val_iov);
	assert(rc == 0);
}
{
	values = (char *)realloc(values, 430);
}
{
	uint64_t key = 1502197874;
	d_iov_t key_iov = {.iov_buf = &key, .iov_buf_len = 8, .iov_len = 8};
	char val[] = {0x61, 0};
	d_iov_t val_iov = {.iov_buf = val, .iov_buf_len = 2, .iov_len = 2};
	int rc = dbtree_update(ik_toh, &key_iov, &val_iov);
	assert(rc == 0);
}
{
	values = (char *)realloc(values, 460);
}
{
	uint64_t key = 1502197874;
	d_iov_t key_iov = {.iov_buf = &key, .iov_buf_len = 8, .iov_len = 8};
	char val[] = {0x61, 0};
	d_iov_t val_iov = {.iov_buf = val, .iov_buf_len = 2, .iov_len = 2};
	int rc = dbtree_update(ik_toh, &key_iov, &val_iov);
	assert(rc == 0);
}
{
	values = (char *)realloc(values, 670);
}
{
	uint64_t key = 1192679685;
	d_iov_t key_iov = {.iov_buf = &key, .iov_buf_len = 8, .iov_len = 8};
	char val[] = {0x61, 0};
	d_iov_t val_iov = {.iov_buf = val, .iov_buf_len = 2, .iov_len = 2};
	int rc = dbtree_update(ik_toh, &key_iov, &val_iov);
	assert(rc == 0);
}
{
	values = (char *)realloc(values, 760);
}
{
	uint64_t key = 1192679685;
	d_iov_t key_iov = {.iov_buf = &key, .iov_buf_len = 8, .iov_len = 8};
	char val[] = {0x61, 0};
	d_iov_t val_iov = {.iov_buf = val, .iov_buf_len = 2, .iov_len = 2};
	int rc = dbtree_update(ik_toh, &key_iov, &val_iov);
	assert(rc == 0);
}
{
	values = (char *)realloc(values, 880);
}
{
	uint64_t key = 465301214;
	d_iov_t key_iov = {.iov_buf = &key, .iov_buf_len = 8, .iov_len = 8};
	char val[] = {0x61, 0};
	d_iov_t val_iov = {.iov_buf = val, .iov_buf_len = 2, .iov_len = 2};
	int rc = dbtree_update(ik_toh, &key_iov, &val_iov);
	assert(rc == 0);
}
{
	values = (char *)realloc(values, 990);
}
{
	uint64_t key = 245736342;
	d_iov_t key_iov = {.iov_buf = &key, .iov_buf_len = 8, .iov_len = 8};
	char val[] = {0x61, 0};
	d_iov_t val_iov = {.iov_buf = val, .iov_buf_len = 2, .iov_len = 2};
	int rc = dbtree_update(ik_toh, &key_iov, &val_iov);
	assert(rc == 0);
}
{
	unsigned int options = 1;
	int rc = dbtree_iter_prepare(ik_toh, options, &iter);
	assert(rc == 0);
}
{
	dbtree_probe_opc_t opc = 2;
	uint32_t intent = 0;
	d_iov_t *key_iovp = NULL;
	int rc = dbtree_iter_probe(iter, opc, intent, key_iovp, NULL);
	assert(rc == 0);
}
{
	d_iov_t key_iov = {.iov_buf = NULL, .iov_buf_len = 0, .iov_len = 0};
	d_iov_t val_iov = {.iov_buf = NULL, .iov_buf_len = 0, .iov_len = 0};
	int rc = dbtree_iter_fetch(iter, &key_iov, &val_iov, NULL);
	assert(rc == 0);
}
{
	int rc = dbtree_iter_prev(iter);
	assert(rc == 0);
}
{
	int rc = dbtree_iter_finish(iter);
	assert(rc == 0);
	iter = DAOS_HDL_INVAL;
}
{
	values = (char *)realloc(values, 1190);
}
{
	uint64_t key = 588932751;
	d_iov_t key_iov = {.iov_buf = &key, .iov_buf_len = 8, .iov_len = 8};
	char val[] = {0x61, 0};
	d_iov_t val_iov = {.iov_buf = val, .iov_buf_len = 2, .iov_len = 2};
	int rc = dbtree_update(ik_toh, &key_iov, &val_iov);
	assert(rc == 0);
}
{
	values = (char *)realloc(values, 1270);
}
{
	uint64_t key = 1809321878;
	d_iov_t key_iov = {.iov_buf = &key, .iov_buf_len = 8, .iov_len = 8};
	char val[] = {0x61, 0};
	d_iov_t val_iov = {.iov_buf = val, .iov_buf_len = 2, .iov_len = 2};
	int rc = dbtree_update(ik_toh, &key_iov, &val_iov);
	assert(rc == 0);
}
{
	values = (char *)realloc(values, 1390);
}
{
	uint64_t key = 1577973697;
	d_iov_t key_iov = {.iov_buf = &key, .iov_buf_len = 8, .iov_len = 8};
	char val[] = {0x61, 0};
	d_iov_t val_iov = {.iov_buf = val, .iov_buf_len = 2, .iov_len = 2};
	int rc = dbtree_update(ik_toh, &key_iov, &val_iov);
	assert(rc == 0);
}
{
	values = (char *)realloc(values, 1520);
}
{
	uint64_t key = 986369512;
	d_iov_t key_iov = {.iov_buf = &key, .iov_buf_len = 8, .iov_len = 8};
	char val[] = {0x61, 0};
	d_iov_t val_iov = {.iov_buf = val, .iov_buf_len = 2, .iov_len = 2};
	int rc = dbtree_update(ik_toh, &key_iov, &val_iov);
	assert(rc == 0);
}
{
	values = (char *)realloc(values, 1650);
}
{
	uint64_t key = 1207235037;
	d_iov_t key_iov = {.iov_buf = &key, .iov_buf_len = 8, .iov_len = 8};
	char val[] = {0x61, 0};
	d_iov_t val_iov = {.iov_buf = val, .iov_buf_len = 2, .iov_len = 2};
	int rc = dbtree_update(ik_toh, &key_iov, &val_iov);
	assert(rc == 0);
}
{
	values = (char *)realloc(values, 1740);
}
{
	uint64_t key = 590987125;
	d_iov_t key_iov = {.iov_buf = &key, .iov_buf_len = 8, .iov_len = 8};
	char val[] = {0x61, 0};
	d_iov_t val_iov = {.iov_buf = val, .iov_buf_len = 2, .iov_len = 2};
	int rc = dbtree_update(ik_toh, &key_iov, &val_iov);
	assert(rc == 0);
}
{
	uint64_t key = 1502197874;
	d_iov_t key_iov = {.iov_buf = &key, .iov_buf_len = 8, .iov_len = 8};
	char val[] = {0x61, 0};
	d_iov_t val_iov = {.iov_buf = val, .iov_buf_len = 2, .iov_len = 2};
	int rc = dbtree_update(ik_toh, &key_iov, &val_iov);
	assert(rc == 0);
}
{
	unsigned int options = 1;
	int rc = dbtree_iter_prepare(ik_toh, options, &iter);
	assert(rc == 0);
}
{
	dbtree_probe_opc_t opc = 2;
	uint32_t intent = 0;
	d_iov_t *key_iovp = NULL;
	int rc = dbtree_iter_probe(iter, opc, intent, key_iovp, NULL);
	assert(rc == 0);
}
{
	int rc = dbtree_iter_finish(iter);
	assert(rc == 0);
	iter = DAOS_HDL_INVAL;
}
{
	struct btr_attr attr = {0};
	struct btr_stat stat = {0};
	int rc = dbtree_query(ik_toh, &attr, &stat);
	assert(rc == 0);
}
{
	uint64_t key = 1207235037;
	d_iov_t key_iov = {.iov_buf = &key, .iov_buf_len = 8, .iov_len = 8};
	d_iov_t val_iov = {.iov_buf = NULL, .iov_buf_len = 0, .iov_len = 0};
	int rc = dbtree_lookup(ik_toh, &key_iov, &val_iov);
	assert(rc == 0 || rc == -DER_NONEXIST);
}
{
	uint64_t key = 1207235037;
	d_iov_t key_iov = {.iov_buf = &key, .iov_buf_len = 8, .iov_len = 8};
	d_iov_t val_iov = {.iov_buf = NULL, .iov_buf_len = 0, .iov_len = 0};
	int rc = dbtree_lookup(ik_toh, &key_iov, &val_iov);
	assert(rc == 0 || rc == -DER_NONEXIST);
}
{
	uint64_t key = 1577973697;
	d_iov_t key_iov = {.iov_buf = &key, .iov_buf_len = 8, .iov_len = 8};
	d_iov_t val_iov = {.iov_buf = NULL, .iov_buf_len = 0, .iov_len = 0};
	int rc = dbtree_lookup(ik_toh, &key_iov, &val_iov);
	assert(rc == 0 || rc == -DER_NONEXIST);
}
{
	uint64_t key = 982660792;
	d_iov_t key_iov = {.iov_buf = &key, .iov_buf_len = 8, .iov_len = 8};
	d_iov_t val_iov = {.iov_buf = NULL, .iov_buf_len = 0, .iov_len = 0};
	int rc = dbtree_lookup(ik_toh, &key_iov, &val_iov);
	assert(rc == 0 || rc == -DER_NONEXIST);
}
{
	uint64_t key = 1192679685;
	d_iov_t key_iov = {.iov_buf = &key, .iov_buf_len = 8, .iov_len = 8};
	d_iov_t val_iov = {.iov_buf = NULL, .iov_buf_len = 0, .iov_len = 0};
	int rc = dbtree_lookup(ik_toh, &key_iov, &val_iov);
	assert(rc == 0 || rc == -DER_NONEXIST);
}
{
	uint64_t key = 1577973697;
	d_iov_t key_iov = {.iov_buf = &key, .iov_buf_len = 8, .iov_len = 8};
	d_iov_t val_iov = {.iov_buf = NULL, .iov_buf_len = 0, .iov_len = 0};
	int rc = dbtree_lookup(ik_toh, &key_iov, &val_iov);
	assert(rc == 0 || rc == -DER_NONEXIST);
}
{
	uint64_t key = 1502197874;
	d_iov_t key_iov = {.iov_buf = &key, .iov_buf_len = 8, .iov_len = 8};
	d_iov_t val_iov = {.iov_buf = NULL, .iov_buf_len = 0, .iov_len = 0};
	int rc = dbtree_lookup(ik_toh, &key_iov, &val_iov);
	assert(rc == 0 || rc == -DER_NONEXIST);
}
{
	values = (char *)realloc(values, 1780);
}
{
	uint64_t key = 486185924;
	d_iov_t key_iov = {.iov_buf = &key, .iov_buf_len = 8, .iov_len = 8};
	char val[] = {0x61, 0};
	d_iov_t val_iov = {.iov_buf = val, .iov_buf_len = 2, .iov_len = 2};
	int rc = dbtree_update(ik_toh, &key_iov, &val_iov);
	assert(rc == 0);
}
{
	values = (char *)realloc(values, 1860);
}
{
	uint64_t key = 892357739;
	d_iov_t key_iov = {.iov_buf = &key, .iov_buf_len = 8, .iov_len = 8};
	char val[] = {0x61, 0};
	d_iov_t val_iov = {.iov_buf = val, .iov_buf_len = 2, .iov_len = 2};
	int rc = dbtree_update(ik_toh, &key_iov, &val_iov);
	assert(rc == 0);
}
{
	values = (char *)realloc(values, 1970);
}
{
	uint64_t key = 1688084903;
	d_iov_t key_iov = {.iov_buf = &key, .iov_buf_len = 8, .iov_len = 8};
	char val[] = {0x61, 0};
	d_iov_t val_iov = {.iov_buf = val, .iov_buf_len = 2, .iov_len = 2};
	int rc = dbtree_update(ik_toh, &key_iov, &val_iov);
	assert(rc == 0);
}
{
	values = (char *)realloc(values, 2220);
}
{
	uint64_t key = 986369512;
	d_iov_t key_iov = {.iov_buf = &key, .iov_buf_len = 8, .iov_len = 8};
	char val[] = {0x61, 0};
	d_iov_t val_iov = {.iov_buf = val, .iov_buf_len = 2, .iov_len = 2};
	int rc = dbtree_update(ik_toh, &key_iov, &val_iov);
	assert(rc == 0);
}
{
	values = (char *)realloc(values, 2470);
}
{
	uint64_t key = 443604083;
	d_iov_t key_iov = {.iov_buf = &key, .iov_buf_len = 8, .iov_len = 8};
	char val[] = {0x61, 0};
	d_iov_t val_iov = {.iov_buf = val, .iov_buf_len = 2, .iov_len = 2};
	int rc = dbtree_update(ik_toh, &key_iov, &val_iov);
	assert(rc == 0);
}
{
	values = (char *)realloc(values, 2730);
}
{
	uint64_t key = 1776738518;
	d_iov_t key_iov = {.iov_buf = &key, .iov_buf_len = 8, .iov_len = 8};
	char val[] = {0x61, 0};
	d_iov_t val_iov = {.iov_buf = val, .iov_buf_len = 2, .iov_len = 2};
	int rc = dbtree_update(ik_toh, &key_iov, &val_iov);
	assert(rc == 0);
}
{
	records = (struct record *)realloc(records, 640);
}
{
	uint64_t key = 859899508;
	d_iov_t key_iov = {.iov_buf = &key, .iov_buf_len = 8, .iov_len = 8};
	char val[] = {0x61, 0};
	d_iov_t val_iov = {.iov_buf = val, .iov_buf_len = 2, .iov_len = 2};
	int rc = dbtree_update(ik_toh, &key_iov, &val_iov);
	assert(rc == 0);
}
{
	int credits = 7;
	bool destroyed = false;
	int rc = dbtree_drain(ik_toh, &credits, NULL, &destroyed);
	assert(rc == 0);
	if (destroyed) ik_toh = DAOS_HDL_INVAL;
}
{
	struct btr_attr attr = {0};
	struct btr_stat stat = {0};
	int rc = dbtree_query(ik_toh, &attr, &stat);
	assert(rc == 0);
}
{
	values = (char *)realloc(values, 2970);
}
{
	uint64_t key = 729721320;
	d_iov_t key_iov = {.iov_buf = &key, .iov_buf_len = 8, .iov_len = 8};
	char val[] = {0x61, 0};
	d_iov_t val_iov = {.iov_buf = val, .iov_buf_len = 2, .iov_len = 2};
	int rc = dbtree_update(ik_toh, &key_iov, &val_iov);
	assert(rc == 0);
}
{
	values = (char *)realloc(values, 3080);
}
{
	uint64_t key = 859899508;
	d_iov_t key_iov = {.iov_buf = &key, .iov_buf_len = 8, .iov_len = 8};
	char val[] = {0x61, 0};
	d_iov_t val_iov = {.iov_buf = val, .iov_buf_len = 2, .iov_len = 2};
	int rc = dbtree_update(ik_toh, &key_iov, &val_iov);
	assert(rc == 0);
}
{
	values = (char *)realloc(values, 3260);
}
{
	uint64_t key = 859899508;
	d_iov_t key_iov = {.iov_buf = &key, .iov_buf_len = 8, .iov_len = 8};
	char val[] = {0x61, 0};
	d_iov_t val_iov = {.iov_buf = val, .iov_buf_len = 2, .iov_len = 2};
	int rc = dbtree_update(ik_toh, &key_iov, &val_iov);
	assert(rc == 0);
}
{
	values = (char *)realloc(values, 3330);
}
{
	uint64_t key = 107195367;
	d_iov_t key_iov = {.iov_buf = &key, .iov_buf_len = 8, .iov_len = 8};
	char val[] = {0x61, 0};
	d_iov_t val_iov = {.iov_buf = val, .iov_buf_len = 2, .iov_len = 2};
	int rc = dbtree_update(ik_toh, &key_iov, &val_iov);
	assert(rc == 0);
}
{
	values = (char *)realloc(values, 3440);
}
{
	uint64_t key = 771876445;
	d_iov_t key_iov = {.iov_buf = &key, .iov_buf_len = 8, .iov_len = 8};
	char val[] = {0x61, 0};
	d_iov_t val_iov = {.iov_buf = val, .iov_buf_len = 2, .iov_len = 2};
	int rc = dbtree_update(ik_toh, &key_iov, &val_iov);
	assert(rc == 0);
}
{
	values = (char *)realloc(values, 3690);
}
{
	uint64_t key = 1192679685;
	d_iov_t key_iov = {.iov_buf = &key, .iov_buf_len = 8, .iov_len = 8};
	char val[] = {0x61, 0};
	d_iov_t val_iov = {.iov_buf = val, .iov_buf_len = 2, .iov_len = 2};
	int rc = dbtree_update(ik_toh, &key_iov, &val_iov);
	assert(rc == 0);
}

}
