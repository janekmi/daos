/**
 * (C) Copyright 2026 Hewlett Packard Enterprise Development LP
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
#define D_LOGFAC DD_FAC(tests)
#include <stdint.h>

#include <daos/mem.h>
#include <daos/btree.h>

/** integer key record */
struct ik_rec {
	uint64_t   ir_key;
	uint32_t   ir_val_size;
	uint32_t   ir_val_msize;
	umem_off_t ir_val_off;
};

static int
ik_hkey_size(void)
{
	struct ik_rec irec;
	return sizeof(irec.ir_key);
}

static void
ik_hkey_gen(struct btr_instance *tins, d_iov_t *key_iov, void *hkey)
{
	uint64_t *ikey;

	ikey = (uint64_t *)key_iov->iov_buf;
	/* ikey = dummy_hash(ikey); */
	memcpy(hkey, ikey, sizeof(*ikey));
}

static int
ik_key_cmp(struct btr_instance *tins, struct btr_record *rec, d_iov_t *key_iov)
{
	int            key1 = *(uint64_t *)key_iov->iov_buf;
	struct ik_rec *irec = (struct ik_rec *)umem_off2ptr(&tins->ti_umm, rec->rec_off);

	if (irec->ir_key < key1)
		return BTR_CMP_LT;
	return irec->ir_key > key1 ? BTR_CMP_GT : BTR_CMP_EQ;
}

static int
ik_rec_alloc(struct btr_instance *tins, d_iov_t *key_iov, d_iov_t *val_iov, struct btr_record *rec,
	     d_iov_t *val_out)
{
	umem_off_t     irec_off;
	struct ik_rec *irec;
	char          *vbuf;

	irec_off = (umem_off_t)umem_zalloc(&tins->ti_umm, sizeof(struct ik_rec));
	D_ASSERT(!UMOFF_IS_NULL(irec_off)); /* lazy bone... */

	irec = (struct ik_rec *)umem_off2ptr(&tins->ti_umm, irec_off);

	irec->ir_key      = *(uint64_t *)key_iov->iov_buf;
	irec->ir_val_size = irec->ir_val_msize = val_iov->iov_len;

	irec->ir_val_off = umem_alloc(&tins->ti_umm, val_iov->iov_len);
	D_ASSERT(!UMOFF_IS_NULL(irec->ir_val_off));

	vbuf = (char *)umem_off2ptr(&tins->ti_umm, irec->ir_val_off);
	memcpy(vbuf, (char *)val_iov->iov_buf, val_iov->iov_len);

	rec->rec_off = irec_off;
	return 0;
}

static int
ik_rec_free(struct btr_instance *tins, struct btr_record *rec, void *args)
{
	struct umem_instance *umm = &tins->ti_umm;
	struct ik_rec        *irec;

	irec = (struct ik_rec *)umem_off2ptr(umm, rec->rec_off);

	if (args != NULL) {
		umem_off_t *rec_ret = (umem_off_t *)args;
		/** Provide the buffer to user */
		*rec_ret = rec->rec_off;
		return 0;
	}
	umem_free(&tins->ti_umm, irec->ir_val_off);
	umem_free(&tins->ti_umm, rec->rec_off);

	return 0;
}

static int
ik_rec_fetch(struct btr_instance *tins, struct btr_record *rec, d_iov_t *key_iov, d_iov_t *val_iov)
{
	struct ik_rec *irec;
	char          *val;
	int            val_size;
	int            key_size;

	if (key_iov == NULL && val_iov == NULL)
		return -EINVAL;

	irec     = (struct ik_rec *)umem_off2ptr(&tins->ti_umm, rec->rec_off);
	val_size = irec->ir_val_size;
	key_size = sizeof(irec->ir_key);

	val = (char *)umem_off2ptr(&tins->ti_umm, irec->ir_val_off);
	if (key_iov != NULL) {
		key_iov->iov_len = key_size;
		if (key_iov->iov_buf == NULL)
			key_iov->iov_buf = &irec->ir_key;
		else if (key_iov->iov_buf_len >= key_size)
			memcpy(key_iov->iov_buf, &irec->ir_key, key_size);
	}

	if (val_iov != NULL) {
		val_iov->iov_len = val_size;
		if (val_iov->iov_buf == NULL)
			val_iov->iov_buf = val;
		else if (val_iov->iov_buf_len >= val_size)
			memcpy(val_iov->iov_buf, val, val_size);
	}
	return 0;
}

static char *
ik_rec_string(struct btr_instance *tins, struct btr_record *rec, bool leaf, char *buf, int buf_len)
{
	struct ik_rec *irec = NULL;
	char          *val;
	int            nob;
	uint64_t       ikey;

	if (!leaf) { /* NB: no record body on intermediate node */
		memcpy(&ikey, &rec->rec_hkey[0], sizeof(ikey));
		snprintf(buf, buf_len, DF_U64, ikey);
		return buf;
	}

	irec = (struct ik_rec *)umem_off2ptr(&tins->ti_umm, rec->rec_off);
	ikey = irec->ir_key;
	nob  = snprintf(buf, buf_len, DF_U64, ikey);

	buf[nob++] = ':';
	buf_len -= nob;

	val = (char *)umem_off2ptr(&tins->ti_umm, irec->ir_val_off);
	strncpy(buf + nob, val, min(irec->ir_val_size, buf_len));

	return buf;
}

static int
ik_rec_update(struct btr_instance *tins, struct btr_record *rec, d_iov_t *key, d_iov_t *val_iov,
	      d_iov_t *val_out)
{
	struct umem_instance *umm = &tins->ti_umm;
	struct ik_rec        *irec;
	char                 *val;

	irec = (struct ik_rec *)umem_off2ptr(umm, rec->rec_off);

	if (irec->ir_val_msize >= val_iov->iov_len) {
		umem_tx_add(umm, irec->ir_val_off, irec->ir_val_msize);

	} else {
		umem_tx_add(umm, rec->rec_off, sizeof(*irec));
		umem_free(umm, irec->ir_val_off);

		irec->ir_val_msize = val_iov->iov_len;
		irec->ir_val_off   = umem_alloc(umm, val_iov->iov_len);
		D_ASSERT(!UMOFF_IS_NULL(irec->ir_val_off));
	}
	val = (char *)umem_off2ptr(umm, irec->ir_val_off);

	memcpy(val, val_iov->iov_buf, val_iov->iov_len);
	irec->ir_val_size = val_iov->iov_len;
	return 0;
}

static int
ik_rec_stat(struct btr_instance *tins, struct btr_record *rec, struct btr_rec_stat *stat)
{
	struct umem_instance *umm = &tins->ti_umm;
	struct ik_rec        *irec;

	irec = (struct ik_rec *)umem_off2ptr(umm, rec->rec_off);

	stat->rs_ksize = sizeof(irec->ir_key);
	stat->rs_vsize = irec->ir_val_size;
	return 0;
}

btr_ops_t ik_ops = {
    .to_hkey_gen   = ik_hkey_gen,
    .to_hkey_size  = ik_hkey_size,
    .to_key_cmp    = ik_key_cmp,
    .to_rec_alloc  = ik_rec_alloc,
    .to_rec_free   = ik_rec_free,
    .to_rec_fetch  = ik_rec_fetch,
    .to_rec_update = ik_rec_update,

    .to_rec_stat   = ik_rec_stat,
    .to_rec_string = ik_rec_string,
};
