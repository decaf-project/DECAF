#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "tsk_os.h"
#include "tsk_types.h"
#include "tsk_error.h"


/* Parse a string to a byte offset 
 * Return -1 on error 
 */

SSIZE_T
parse_offset(char *offset)
{
    char offset_lcl[32], *offset_lcl_p;
    DADDR_T num_blk;
    char *cp, *at;
    int bsize;
    OFF_T offset_b;

    /* Parse the offset value */
    if (offset == NULL) {
	return 0;
    }


    strncpy(offset_lcl, offset, 32);
    offset_lcl_p = offset_lcl;

    /* Check for the x@y setup  and set
     * bsize if it exists
     */
    if ((at = strchr(offset_lcl_p, '@')) != NULL) {
	*at = '\0';
	at++;

	bsize = strtoul(at, &cp, 0);
	if (*cp || cp == at) {
	    tsk_errno = TSK_ERR_IMG_OFFSET;
	    snprintf(tsk_errstr, TSK_ERRSTR_L,
		"tsk_parse: block size: %s", at);
	    tsk_errstr2[0] = '\0';
	    return -1;
	}
	else if (bsize % 512) {
	    tsk_errno = TSK_ERR_IMG_OFFSET;
	    snprintf(tsk_errstr, TSK_ERRSTR_L,
		"tsk_parse: block size not multiple of 512");
	    tsk_errstr2[0] = '\0';
	    return -1;
	}
    }
    else {
	bsize = 512;
    }


    /* Now we address the sector offset */
    offset_lcl_p = offset_lcl;

    /* remove leading 0s */
    while ((offset_lcl_p[0] != '\0') && (offset_lcl_p[0] == '0'))
	offset_lcl_p++;

    if (offset_lcl_p[0] != '\0') {
	num_blk = strtoull(offset_lcl_p, &cp, 0);
	if (*cp || cp == offset_lcl_p) {
	    tsk_errno = TSK_ERR_IMG_OFFSET;
	    snprintf(tsk_errstr, TSK_ERRSTR_L,
		"tsk_parse: invalid image offset: %s", offset_lcl_p);
	    tsk_errstr2[0] = '\0';
	    return -1;
	}
	offset_b = num_blk * bsize;
    }
    else {
	offset_b = 0;
    }

    if (verbose)
	fprintf(stderr, "parse_offset: Offset set to %" PRIuOFF "\n",
	    offset_b);

    return offset_b;
}
