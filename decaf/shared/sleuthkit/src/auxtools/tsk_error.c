#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "tsk_os.h"
#include "tsk_types.h"
#include "tsk_error.h"

/* Global variables that fit here as well as anywhere */
char *progname = "unknown";
int verbose = 0;

uint32_t tsk_errno = 0;		/* Set when an error occurs */
char tsk_errstr[TSK_ERRSTR_L];	/* Contains an error-specific string
				 * and is valid only when tsk_errno is set 
				 *
				 * This should be set when errno is set,
				 * if it is not needed, then set 
				 * tsk_errstr[0] to '\0'.
				 * */

char tsk_errstr2[TSK_ERRSTR_L];	/* Contains a caller-specific string 
				 * and is valid only when tsk_errno is set 
				 *
				 * This is typically set to start with a NULL
				 * char when errno is set and then set with
				 * a string by the code that called the 
				 * function that had the error.  For
				 * example, the X_read() function may set why
				 * the read failed in tsk_errstr and the
				 * function that called X_read() can provide
				 * more context about why X_read() was 
				 * called in the first place
				 */

const char *tsk_err_aux_str[TSK_ERR_IMG_MAX] = {
    "Insufficient memory",
    ""
};

/* imagetools specific error strings */
const char *tsk_err_img_str[TSK_ERR_IMG_MAX] = {
    "Missing image file names",
    "Invalid image offset",
    "Cannot determine image type",
    "Unsupported image type",
    "Error opening image file",
    "Error stat(ing) image file",
    "Error seeking in image file",
    "Error reading image file",
    "Read offset too large for image file",
    "Invalid image format layer sequence",
    "Invalid magic value",
    "Error writing data",
};


const char *tsk_err_mm_str[TSK_ERR_MM_MAX] = {
    "Cannot determine partiton type",
    "Unsupported partition type",
    "Error reading image file",
    "Invalid magic value",
    "Invalid walk range",
    "Invalid buffer size",
    "Invalid sector address"
};

const char *tsk_err_fs_str[TSK_ERR_FS_MAX] = {
    "Cannot determine file system type",
    "Unsupported file system type",
    "Function not supported",
    "Invalid walk range",
    "Error reading image file",
    "Invalid argument",
    "Invalid block address",
    "Invalid metadata address",
    "Error in metadata structure",
    "Invalid magic value",
    "Error extracting file from image",
    "Error writing data",
    "Error converting Unicode" "Error recovering deleted file",
    "General file system error"
};



/* Print the error message to hFile */
void
tsk_error_print(FILE * hFile)
{
    if (tsk_errno == 0)
	return;

    if (tsk_errno & TSK_ERR_AUX) {
	if ((TSK_ERR_MASK & tsk_errno) < TSK_ERR_AUX_MAX)
	    fprintf(hFile, "%s",
		tsk_err_aux_str[tsk_errno & TSK_ERR_MASK]);
	else
	    fprintf(hFile, "auxtools error: %d", TSK_ERR_MASK & tsk_errno);
    }
    else if (tsk_errno & TSK_ERR_IMG) {
	if ((TSK_ERR_MASK & tsk_errno) < TSK_ERR_IMG_MAX)
	    fprintf(hFile, "%s",
		tsk_err_img_str[tsk_errno & TSK_ERR_MASK]);
	else
	    fprintf(hFile, "imgtools error: %d", TSK_ERR_MASK & tsk_errno);
    }
    else if (tsk_errno & TSK_ERR_MM) {
	if ((TSK_ERR_MASK & tsk_errno) < TSK_ERR_MM_MAX)
	    fprintf(hFile, "%s", tsk_err_mm_str[tsk_errno & TSK_ERR_MASK]);
	else
	    fprintf(hFile, "mmtools error: %d", TSK_ERR_MASK & tsk_errno);
    }
    else if (tsk_errno & TSK_ERR_FS) {
	if ((TSK_ERR_MASK & tsk_errno) < TSK_ERR_FS_MAX)
	    fprintf(hFile, "%s", tsk_err_fs_str[tsk_errno & TSK_ERR_MASK]);
	else
	    fprintf(hFile, "fstools error: %d", TSK_ERR_MASK & tsk_errno);
    }
    else {
	fprintf(hFile, "Error: %d", tsk_errno);
    }

    /* Print the unique string, if it exists */
    if (tsk_errstr[0] != '\0')
	fprintf(hFile, " (%s)", tsk_errstr);

    if (tsk_errstr2[0] != '\0')
	fprintf(hFile, " (%s)", tsk_errstr2);

    fprintf(hFile, "\n");

}
