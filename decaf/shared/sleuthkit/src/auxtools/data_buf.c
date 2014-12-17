/*++
* NAME
*	fs_buf 3
* SUMMARY
*	file system buffer management routines
* SYNOPSIS
*	#include "fstools.h"
*
*	FS_BUF	*fs_buf_alloc(int size)
*
*	void	fs_buf_free(FS_BUF *buf)
* DESCRIPTION
*	This module implements file sysem buffer management.
*
*	fs_buf_alloc() allocates a buffer with the specified size.
*
*	fs_buf_free() destroys a buffer that was created by fs_buf_alloc().
* DIAGNOSTCS
*	Fatal errors: out of memory. Panic: block size is not a multiple
*	of the device block size.
* LICENSE
*	This software is distributed under the IBM Public License.
* AUTHOR(S)
*	Wietse Venema
*	IBM T.J. Watson Research
*	P.O. Box 704
*	Yorktown Heights, NY 10598, USA
*--*/

#include <stdlib.h>
#include "tsk_os.h"
#include "tsk_types.h"
#include "data_buf.h"
#include "mymalloc.h"


/* data_buf_alloc - allocate file system I/O buffer 
 *
 * Returns NULL on error
 * */

DATA_BUF *
data_buf_alloc(size_t size)
{
    DATA_BUF *buf;

    buf = (DATA_BUF *) mymalloc(sizeof(*buf));
    if (buf == NULL)
	return NULL;

    buf->data = mymalloc(size);
    if (buf->data == NULL) {
	free(buf);
	return NULL;
    }
    buf->size = size;
    buf->used = 0;
    buf->addr = 0;
    return (buf);
}

/* data_buf_free - destroy file system I/O buffer */

void
data_buf_free(DATA_BUF * buf)
{
    free(buf->data);
    free((char *) buf);
}
