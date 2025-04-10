#define D_LOGFAC DD_FAC(telem) // TBD

#include <daos_errno.h>
#include <gurt/debug.h>
#include <gurt/common.h>

#include "dlck_internal.h"

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
