#ifndef __DAOS_DLCK_H__
#define __DAOS_DLCK_H__

#include <daos/mem.h>

/**
 * Trace a single DTX record.
 */
struct dlck_dtx_rec {
	uint32_t   lid;
	umem_off_t umoff;
};

/**
 * Array of DTX records.
 */
struct dlck_dtx_rec_array {
	uint32_t             dda_len;     /** Current length of array */
	uint32_t             dda_max_len; /** Allocated length of array */
	struct dlck_dtx_rec *dda_rec;     /** Entries in array */
};

/**
 * Append a record to the array.
 *
 * \param[in,out]	dda	Array to which the record will be appended.
 * \param[in]		rec	Record to append.
 *
 * \retval 0		Success.
 * \retval -DER_NOMEM	Cannot resize the array to accommodate the new record.
 */
int
dlck_dtx_rec_array_append(struct dlck_dtx_rec_array *dda, const struct dlck_dtx_rec *rec);

/**
 * Move records from \p src to \p dst. \p src is left empty.
 *
 * \param[in,out]	dst	Destination array.
 * \param[in,out]	src	Source array.
 */
void
dlck_dtx_rec_array_move(struct dlck_dtx_rec_array *dst, struct dlck_dtx_rec_array *src);

/**
 * Release the attached resources. \p dda is left empty.
 *
 * \param[in,out]	dda	Array to process.
 */
void
dlck_dtx_rec_array_free(struct dlck_dtx_rec_array *dda);

/**
 * \brief Recreate the records for active DTX entries.
 *
 * Scan the entire provided \p coh container for records referencing active DTX entries.
 *
 * \param[in]	coh	The container to process.
 * \param[out]	dda	Array of active DAE records.
 *
 * \retval 0		Success.
 * \retval -DER_*	Error.
 */
int
dlck_vos_cont_rec_get_active(daos_handle_t coh, struct dlck_dtx_rec_array *dda);

/**
 * \brief Remove records from all active DTX entries.
 *
 * This process is intended for catastrophic recovery in case the records of active DTX entries have
 * been corrupted.
 *
 * \param[in]	coh	Parent container.
 *
 * \retval 0		Success.
 * \retval -DER_*	The transaction has failed.
 */
int
dlck_dtx_act_recs_remove(daos_handle_t coh);

/**
 * \brief Set active DTX entries' records as provided by \p dda.
 *
 * This process assumes the exising DAE records have been first removed. Please find a relevant API
 * call to do it for you. This process is intended for catastrophic recovery in case the records of
 * DAEs have been corrupted.
 *
 *
 * \param[in]	coh	Parent container.
 * \param[in]	dda	Array of active DAE records.
 *
 * \retval 0			Success.
 * \retval -DER_NOTSUPPORTED	DAE records are not removed.
 * \retval -DER_NOMEM		Run out of memory.
 * \retval -DER_*		The transaction has failed.
 */
int
dlck_dtx_act_recs_set(daos_handle_t coh, struct dlck_dtx_rec_array *dda);

/** DLCK callbacks */

typedef bool (*DLCK_ask_yes_no)(const char *);

struct DLCK_callbacks {
	DLCK_ask_yes_no dc_ask_yes_no;
};

extern struct DLCK_callbacks *DLCK_Callbacks;

#endif /* __DAOS_DLCK_H__ */
