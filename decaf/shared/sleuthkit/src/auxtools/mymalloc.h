/*++
* NAME
*	mymalloc 3h
* SUMMARY
*	memory management wrappers
* SYNOPSIS
*	#include "mymalloc.h"
* DESCRIPTION
* .nf
*/

#ifndef _MYMALLOC_H
#define _MYMALLOC_H

#ifdef __cplusplus
extern "C" {
#endif

    /*
     * External interface.
     */
    extern char *mymalloc(int);
    extern char *myrealloc(char *, int);
    extern char *mystrdup(const char *);

#ifdef __cplusplus
}
#endif
#endif
/* LICENSE
* .ad
* .fi
*	The IBM Public License must be distributed with this software.
* AUTHOR(S)
*	Wietse Venema
*	IBM T.J. Watson Research
*	P.O. Box 704
*	Yorktown Heights, NY 10598, USA
--*/
