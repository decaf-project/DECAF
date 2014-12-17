/*
 * Brian Carrier [carrier@sleuthkit.org]
 * Copyright (c) 2006 Brian Carrier, Basis Technology.  All rights reserved
 *
 */
/*++
* NAME
*	mymalloc 3
* SUMMARY
*	memory management wrappers
* SYNOPSIS
*	#include <mymalloc.h>
*
*	char	*mymalloc(len)
*	int	len;
*
*	char	*myrealloc(ptr, len)
*	char	*ptr;
*	int	len;
*
*	char	*mystrdup(str)
*const char *str;
*DESCRIPTION
*	This module performs low-level memory management with error
*	handling. A call of these functions either succeeds or it does
*	not return at all.
*
*	mymalloc() allocates the requested amount of memory. The memory
*	is not set to zero.
*
*	myrealloc() resizes memory obtained from mymalloc() or myrealloc()
*	to the requested size. The result pointer value may differ from
*	that given via the \fBptr\fR argument.
*
*	mystrdup() returns a dynamic-memory copy of its null-terminated
*	argument. This routine uses mymalloc().
* SEE ALSO
*	error(3) error reporting module.
* DIAGNOSTICS
*	Fatal errors: the requested amount of memory is not available.
* LICENSE
* .ad
* .fi
*	The IBM Public Licence must be distributed with this software.
* AUTHOR(S)
*	Wietse Venema
*	IBM T.J. Watson Research
*	P.O. Box 704
*	Yorktown Heights, NY 10598, USA
*--*/

/* System library. */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "tsk_os.h"
#include "tsk_types.h"
#include "tsk_error.h"


/* mymalloc - allocate memory and set error values on error
 */
char *
mymalloc(int len)
{
    char *ptr;

    if ((ptr = (char *) malloc(len)) == 0) {
	tsk_errno = TSK_ERR_AUX_MALLOC;
	snprintf(tsk_errstr, TSK_ERRSTR_L, "mymalloc: %s",
	    strerror(errno));
    }
    return (ptr);
}

/* myrealloc - reallocate memory and set error values if needed */
char *
myrealloc(char *ptr, int len)
{
    if ((ptr = (char *) realloc(ptr, len)) == 0) {
	tsk_errno = TSK_ERR_AUX_MALLOC;
	snprintf(tsk_errstr, TSK_ERRSTR_L, "myrealloc: %s",
	    strerror(errno));
    }
    return (ptr);
}
