#define D_LOGFAC DD_FAC(dlck)

#include <daos_errno.h>
#include <daos/debug.h>
#include <daos/common.h>
#include <daos_srv/dlck.h>

#define DLCK_DTX_REC_ARRAY_GROW_BY 10

struct DLCK_callbacks *DLCK_Callbacks = NULL;

int
dlck_dtx_rec_array_append(struct dlck_dtx_rec_array *dda, const struct dlck_dtx_rec *rec)
{
	struct dlck_dtx_rec *newptr = NULL;
	uint32_t             count;

	/** Array has to grow. */
	if (dda->dda_len == dda->dda_max_len) {
		count = dda->dda_max_len + DLCK_DTX_REC_ARRAY_GROW_BY;
		D_REALLOC_ARRAY(newptr, dda->dda_rec, dda->dda_max_len, count);
		if (newptr == NULL)
			return -DER_NOMEM;
		dda->dda_max_len = count;
		dda->dda_rec     = newptr;
	}

	/** Append the new record. */
	dda->dda_rec[dda->dda_len].lid   = rec->lid;
	dda->dda_rec[dda->dda_len].umoff = rec->umoff;
	dda->dda_len += 1;

	return DER_SUCCESS;
}

void
dlck_dtx_rec_array_move(struct dlck_dtx_rec_array *dst, struct dlck_dtx_rec_array *src)
{
	D_FREE(dst->dda_rec);

	/** Copy/move everything from the source. */
	dst->dda_rec     = src->dda_rec;
	dst->dda_len     = src->dda_len;
	dst->dda_max_len = src->dda_max_len;

	/** Reset the source. */
	src->dda_rec     = NULL;
	src->dda_len     = 0;
	src->dda_max_len = 0;
}

void
dlck_dtx_rec_array_free(struct dlck_dtx_rec_array *dda)
{
	/** Free the resources. */
	D_FREE(dda->dda_rec);

	/** Reset. */
	dda->dda_rec     = NULL;
	dda->dda_len     = 0;
	dda->dda_max_len = 0;
}
