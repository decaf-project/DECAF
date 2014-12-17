/*++
* NAME
*	split_at 3h
* SUMMARY
*	trivial token splitter
* SYNOPSIS
*	#include <split_at.h>
* DESCRIPTION
* .nf
*/

#ifndef _SPLIT_AT_H
#define _SPLIT_AT_H

#ifdef __cplusplus
extern "C" {
#endif

    /* External interface. */

    extern char *split_at(char *, int);
    extern char *split_at_right(char *, int);

#ifdef __cplusplus
}
#endif
#endif
/* HISTORY
* .ad
* .fi
*	A split_at() routine appears in the TCP Wrapper software
*	by Wietse Venema.
* LICENSE
* .ad
* .fi
*	The IBM Public Licence must be distributed with this software.
* AUTHOR(S)
*	Wietse Venema
*	IBM T.J. Watson Research
*	P.O. Box 704
*	Yorktown Heights, NY 10598, USA
--*/
