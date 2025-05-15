#define D_LOGFAC DD_FAC(telem) // TBD

#include <daos_errno.h>
#include <gurt/debug.h>
#include <gurt/common.h>
#include <daos_srv/dlck.h>

bool
DLCK_check_array_zero(uint64_t *array, int num)
{
	for (int i = 0; i < num; ++i) {
		if (array[i] != 0) {
			return false;
		}
	}
	return true;
}

struct DLCK_callbacks *DLCK_Callbacks = NULL;

int
dlck_dtx_rec_array_append(struct dlck_dtx_rec_array *dda, struct dlck_dtx_rec *rec)
{
	struct dlck_dtx_rec *newptr = NULL;
	uint32_t             count;

	if (dda->dda_len == dda->dda_max_len) {
		count = dda->dda_max_len + DLCK_DTX_REC_ARRAY_GROW_BY;
		D_REALLOC_ARRAY(newptr, dda->dda_rec, dda->dda_max_len, count);
		if (newptr == NULL)
			return -DER_NOMEM;
		dda->dda_max_len = count;
		dda->dda_rec     = newptr;
	}

	dda->dda_rec[dda->dda_len].lid   = rec->lid;
	dda->dda_rec[dda->dda_len].umoff = rec->umoff;
	dda->dda_len += 1;

	return DER_SUCCESS;
}

void
dlck_dtx_rec_array_move(struct dlck_dtx_rec_array *dst, struct dlck_dtx_rec_array *src)
{
	D_FREE(dst->dda_rec);

	dst->dda_rec     = src->dda_rec;
	dst->dda_len     = src->dda_len;
	dst->dda_max_len = src->dda_max_len;

	src->dda_rec     = NULL;
	src->dda_len     = 0;
	src->dda_max_len = 0;
}

void
dlck_dtx_rec_array_free(struct dlck_dtx_rec_array *dda)
{
	D_FREE(dda->dda_rec);
	dda->dda_rec     = NULL;
	dda->dda_len     = 0;
	dda->dda_max_len = 0;
}
