/*++
* NAME
*	strerror 3
* SUMMARY
*	convert error number to string
* SYNOPSIS
*	#include <error.h>
*
*	char	*strerror(err)
*	int	err;
* DESCRIPTION
*	strerror() maps the specified error number to the corresponding
*	text. The result is in static memory and must not be changed
*	by the caller.
* SEE ALSO
*	errno(2), system error numbers
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

#include <errno.h>
#include <stdio.h>

#ifdef MISSING_STRERROR

extern char *sys_errlist[];
extern int sys_nerr;

char *
strerror(int err)
{
    static char buf[20];

    if (err < sys_nerr && err > 0) {
	return (sys_errlist[err]);
    }
    else {
	sprintf(buf, "Unknown error %d", err);
	return (buf);
    }
}

#endif
