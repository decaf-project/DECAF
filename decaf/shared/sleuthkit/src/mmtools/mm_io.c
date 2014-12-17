/*
 * The Sleuth Kit
 *
 * $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
 *
 * Brian Carrier [carrier@sleuthkit.org]
 * Copyright (c) 2006 Brian Carrier, Basis Technology.  All rights reserved
 * Copyright (c) 2003-2005 Brian Carrier.  All rights reserved
 * 
 * Copyright (c) 1997,1998,1999, International Business Machines          
 * Corporation and others. All Rights Reserved.
 *
 *
 * LICENSE
 *	This software is distributed under the IBM Public License.
 * AUTHOR(S)
 *	Wietse Venema
 *	IBM T.J. Watson Research
 *	P.O. Box 704
 *	Yorktown Heights, NY 10598, USA
--*/

#include <errno.h>
#include "mm_tools.h"


/* mm_read_block - read a block given the address - 
 * calls the read_random at the img layer 
 * Returns the size read or -1 on error */

SSIZE_T
mm_read_block(MM_INFO * mm, DATA_BUF * buf, OFF_T len, DADDR_T addr)
{
    OFF_T ofmm;
    SSIZE_T cnt;

    if (len % mm->dev_bsize) {
	tsk_errno = TSK_ERR_MM_READ;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "mm_read_block: length %" PRIuOFF " not a multiple of %d",
	    len, mm->dev_bsize);
	tsk_errstr2[0] = '\0';
	return -1;
    }


    if (len > buf->size) {
	tsk_errno = TSK_ERR_MM_READ;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "mm_read_block: buffer too small - %"
	    PRIuOFF " > %Zd", len, buf->size);
	tsk_errstr2[0] = '\0';
	return -1;
    }

    buf->addr = addr;
    ofmm = (OFF_T) addr *mm->block_size;

    cnt =
	mm->img_info->read_random(mm->img_info, mm->offset, buf->data, len,
	ofmm);
    buf->used = cnt;
    return cnt;
}

/* Return number of bytes read or -1 on error */
SSIZE_T
mm_read_block_nobuf(MM_INFO * mm, char *buf, OFF_T len, DADDR_T addr)
{
    if (len % mm->dev_bsize) {
	tsk_errno = TSK_ERR_MM_READ;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "mm_read_block_nobuf: length %" PRIuOFF
	    " not a multiple of %d", len, mm->dev_bsize);
	tsk_errstr2[0] = '\0';
	return -1;
    }

    return mm->img_info->read_random(mm->img_info, mm->offset, buf, len,
	(OFF_T) addr * mm->block_size);
}
