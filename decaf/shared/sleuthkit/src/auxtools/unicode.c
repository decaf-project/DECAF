/*
** The Sleuth Kit 
**
** $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
**
** Brian Carrier [carrier@sleuthkit.org]
** Copyright (c) 2004-2005 Brian Carrier.  All rights reserved
**
** This software is distributed under the Common Public License 1.0
**
*/
#include <stdlib.h>
#include <ctype.h>

/* 
 * Convert a UNICODE string to an ASCII string
 * 
 * both uni and asc must be defined and asc must be >= ulen / 2
 */
void
uni2ascii(char *uni, int ulen, char *asc, int alen)
{
    int i, l;


    /* find the maximum that we can go
     * we will break when we hit NULLs, but this is the
     * absolute max 
     */
    if ((ulen + 1) > alen)
	l = alen - 1;
    else
	l = ulen;

    for (i = 0; i < l; i++) {
	/* If this value is NULL, then stop */
	if (uni[i * 2] == 0 && uni[i * 2 + 1] == 0)
	    break;

	if (isprint((int) uni[i * 2]))
	    asc[i] = uni[i * 2];
	else
	    asc[i] = '?';
    }
    /* NULL Terminate */
    asc[i] = '\0';
    return;
}
