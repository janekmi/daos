#include <stddef.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <daos/mem.h>
#include <daos/btree.h>

#include "interceptor.h"

char *interceptor_output;

void
dump(const char *format, ...)
{
  	if (interceptor_output) {
		FILE *file = fopen(interceptor_output, "a");
        va_list arg;
	    va_start(arg, format);
        vfprintf(file, format, arg);
        va_end(arg);
		fclose(file);
	}  
}

FUNC_MOCK(dbtree_create, int, unsigned int tree_class, uint64_t tree_feats,
	      unsigned int tree_order, struct umem_attr *uma,
	      umem_off_t *root_offp, daos_handle_t *toh)
{
    dump(
"{\n"
"	int tree_class = %d;\n"
"	uint64_t tree_feats = %lu;\n"
"	unsigned int tree_order = %u;\n"
"	int rc = dbtree_create(tree_class, tree_feats, tree_order, ik_uma, &ik_root_off, &ik_toh);\n"
"	assert(rc == 0);\n"
"}\n",
		tree_class, tree_feats, tree_order);
	return FUNC_REAL(dbtree_create)(tree_class, tree_feats, tree_order, uma, root_offp, toh);
}

FUNC_MOCK(dbtree_close, int, daos_handle_t toh)
{
    dump(
"{\n"
"	int rc = dbtree_close(ik_toh);\n"
"	assert(rc == 0);\n"
"	ik_toh = DAOS_HDL_INVAL;\n"
"}\n");
	return FUNC_REAL(dbtree_close)(toh);
}

FUNC_MOCK(dbtree_open, int, umem_off_t root_off, struct umem_attr *uma, daos_handle_t *toh)
{
    dump(
"{\n"
"	int rc = dbtree_open(ik_root_off, ik_uma, &ik_toh);\n"
"	assert(rc == 0);\n"
"}\n");
	return FUNC_REAL(dbtree_open)(root_off, uma, toh);
}

FUNC_MOCK(dbtree_update, int, daos_handle_t toh, d_iov_t *key_iov, d_iov_t *val_iov)
{
    assert_int_equal(key_iov->iov_buf_len, sizeof(uint64_t));
    assert_int_equal(key_iov->iov_len, sizeof(uint64_t));
    assert_int_equal(val_iov->iov_buf_len, 2 * sizeof(char));
    assert_int_equal(val_iov->iov_len, 2 * sizeof(char));
	dump(
"{\n"
"	uint64_t key = %lu;\n"
"	d_iov_t key_iov = {.iov_buf = &key, .iov_buf_len = %lu, .iov_len = %lu};\n"
"	char val[] = {%#x, %#x};\n"
"	d_iov_t val_iov = {.iov_buf = val, .iov_buf_len = 2, .iov_len = 2};\n"
"	int rc = dbtree_update(ik_toh, &key_iov, &val_iov);\n"
"	assert(rc == 0);\n"
"}\n",
        *(uint64_t *)key_iov->iov_buf, key_iov->iov_buf_len, key_iov->iov_len,
        ((char *)val_iov->iov_buf)[0], ((char *)val_iov->iov_buf)[1]);
	return FUNC_REAL(dbtree_update)(toh, key_iov, val_iov);
}

FUNC_MOCK(dbtree_iter_prepare, int, daos_handle_t toh, unsigned int options, daos_handle_t *ih)
{
    dump(
"{\n"
"	unsigned int options = %u;\n"
"	int rc = dbtree_iter_prepare(ik_toh, options, &iter);\n"
"	assert(rc == 0);\n"
"}\n",
        options);
    return FUNC_REAL(dbtree_iter_prepare)(toh, options, ih);
}

FUNC_MOCK(dbtree_iter_probe, int, daos_handle_t ih, dbtree_probe_opc_t opc, uint32_t intent,
		  d_iov_t *key_iov, daos_anchor_t *anchor)
{
    assert_null(anchor);
    dump(
"{\n"
"	dbtree_probe_opc_t opc = %u;\n"
"	uint32_t intent = %u;\n",
    opc, intent);
    if (key_iov == NULL) {
        dump(
"	d_iov_t *key_iovp = NULL;\n");
    } else {
        assert_int_equal(key_iov->iov_buf_len, sizeof(uint64_t));
        assert_int_equal(key_iov->iov_len, sizeof(uint64_t));
        dump(
"	uint64_t key = %lu;\n"
"	d_iov_t key_iov = {.iov_buf = &key, .iov_buf_len = %lu, .iov_len = %lu};\n"
"	d_iov_t *key_iovp = &key_iov;\n",
            *(uint64_t *)key_iov->iov_buf, key_iov->iov_buf_len, key_iov->iov_len
        );
    }
    dump(
"	int rc = dbtree_iter_probe(iter, opc, intent, key_iovp, NULL);\n"
"	assert(rc == 0);\n"
"}\n",
        opc, intent
    );
    return FUNC_REAL(dbtree_iter_probe)(ih, opc, intent, key_iov, NULL);
}

FUNC_MOCK(dbtree_iter_fetch, int, daos_handle_t ih, d_iov_t *key_iov, d_iov_t *val_iov, daos_anchor_t *anchor)
{
    assert_null(key_iov->iov_buf);
    assert_int_equal(key_iov->iov_buf_len, 0);
    assert_int_equal(key_iov->iov_len, 0);
    assert_null(val_iov->iov_buf);
    assert_int_equal(val_iov->iov_buf_len, 0);
    assert_int_equal(val_iov->iov_len, 0);
    assert_null(anchor);
	dump(
"{\n"
"	d_iov_t key_iov = {.iov_buf = NULL, .iov_buf_len = 0, .iov_len = 0};\n"
"	d_iov_t val_iov = {.iov_buf = NULL, .iov_buf_len = 0, .iov_len = 0};\n"
"	int rc = dbtree_iter_fetch(iter, &key_iov, &val_iov, NULL);\n"
"	assert(rc == 0);\n"
"}\n");
    return FUNC_REAL(dbtree_iter_fetch)(ih, key_iov, val_iov, anchor);
}

FUNC_MOCK(dbtree_iter_delete, int, daos_handle_t ih, void *args)
{
    assert_null(args);
    dump(
"{\n"
"	int rc = dbtree_iter_delete(iter, NULL);\n"
"	assert(rc == 0);\n"
"}\n");
    return FUNC_REAL(dbtree_iter_delete)(ih, NULL);
}

FUNC_MOCK(dbtree_iter_next, int, daos_handle_t ih)
{
    dump(
"{\n"
"	int rc = dbtree_iter_next(iter);\n"
"	assert(rc == 0);\n"
"}\n");
    return FUNC_REAL(dbtree_iter_next)(ih);
}

FUNC_MOCK(dbtree_iter_prev, int, daos_handle_t ih)
{
    dump(
"{\n"
"	int rc = dbtree_iter_prev(iter);\n"
"	assert(rc == 0);\n"
"}\n");
    return FUNC_REAL(dbtree_iter_prev)(ih);
}

FUNC_MOCK(dbtree_iter_finish, int, daos_handle_t ih)
{
    dump(
"{\n"
"	int rc = dbtree_iter_finish(iter);\n"
"	assert(rc == 0);\n"
"	iter = DAOS_HDL_INVAL;\n"
"}\n");
    return FUNC_REAL(dbtree_iter_finish)(ih);
}

FUNC_MOCK(dbtree_query, int, daos_handle_t toh, struct btr_attr *attr, struct btr_stat *stat)
{
    dump(
"{\n"
"	struct btr_attr attr = {0};\n"
"	struct btr_stat stat = {0};\n"
"	int rc = dbtree_query(ik_toh, &attr, &stat);\n"
"	assert(rc == 0);\n"
"}\n");
    return FUNC_REAL(dbtree_query)(toh, attr, stat);
}

FUNC_MOCK(dbtree_lookup, int, daos_handle_t toh, d_iov_t *key_iov, d_iov_t *val_iov)
{
    assert_int_equal(key_iov->iov_buf_len, sizeof(uint64_t));
    assert_int_equal(key_iov->iov_len, sizeof(uint64_t));
    assert_null(val_iov->iov_buf);
    assert_int_equal(val_iov->iov_buf_len, 0);
    assert_int_equal(val_iov->iov_len, 0);
    dump(
"{\n"
"	uint64_t key = %lu;\n"
"	d_iov_t key_iov = {.iov_buf = &key, .iov_buf_len = %lu, .iov_len = %lu};\n"
"	d_iov_t val_iov = {.iov_buf = NULL, .iov_buf_len = 0, .iov_len = 0};\n"
"	int rc = dbtree_lookup(ik_toh, &key_iov, &val_iov);\n"
"	assert(rc == 0 || rc == -DER_NONEXIST);\n"
"}\n",
        *(uint64_t *)key_iov->iov_buf, key_iov->iov_buf_len, key_iov->iov_len);
    return FUNC_REAL(dbtree_lookup)(toh, key_iov, val_iov);
}

FUNC_MOCK(dbtree_drain, int, daos_handle_t toh, int *credits, void *args, bool *destroyed)
{
    assert_null(args);
    dump(
"{\n"
"	int credits = %d;\n"
"	bool destroyed = false;\n"
"	int rc = dbtree_drain(ik_toh, &credits, NULL, &destroyed);\n"
"	assert(rc == 0);\n"
"	if (destroyed) ik_toh = DAOS_HDL_INVAL;\n"
"}\n",
        *credits);
    return FUNC_REAL(dbtree_drain)(toh, credits, args, destroyed);
}
