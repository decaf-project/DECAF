/*
 * The Sleuth Kit
 *
 * $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
 *
 * Brian Carrier [carrier@sleuthkit.org]
 * Copyright (c) 2006 Brian Carrier, Basis Technology.  All Rights reserved
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
#include "fs_tools.h"


/* fs_read_block - read a block given the address - calls the read_random at the img layer */

SSIZE_T
fs_read_block(FS_INFO * fs, DATA_BUF * buf, OFF_T len, DADDR_T addr)
{
    OFF_T offs;
    SSIZE_T cnt;

    if (len % fs->dev_bsize) {
	tsk_errno = TSK_ERR_FS_READ;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "fs_read_block: length %" PRIuOFF " not a multiple of %d",
	    len, fs->dev_bsize);
	tsk_errstr2[0] = '\0';
	return -1;
    }


    if (len > buf->size) {
	tsk_errno = TSK_ERR_FS_READ;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "fs_read_block: Buffer too small - %"
	    PRIuOFF " > %Zd", len, buf->size);
	tsk_errstr2[0] = '\0';
	return -1;
    }

    if (addr > fs->last_block) {
	tsk_errno = TSK_ERR_FS_READ;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "fs_read_block: Address is too large: %" PRIuDADDR ")", addr);
	tsk_errstr2[0] = '\0';
	return -1;
    }

    buf->addr = addr;
    offs = (OFF_T) addr *fs->block_size;

    cnt =
	fs->img_info->read_random(fs->img_info, fs->offset, buf->data, len,
	offs);
    buf->used = cnt;
    return cnt;
}

SSIZE_T
fs_read_block_nobuf(FS_INFO * fs, char *buf, OFF_T len, DADDR_T addr)
{
    if (len % fs->dev_bsize) {
	tsk_errno = TSK_ERR_FS_READ;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "fs_read_block_nobuf: length %" PRIuOFF
	    " not a multiple of %d", len, fs->dev_bsize);
	tsk_errstr2[0] = '\0';
	return -1;
    }

    if (addr > fs->last_block) {
	tsk_errno = TSK_ERR_FS_READ;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "fs_read_block: Address is too large: %" PRIuDADDR ")", addr);
	tsk_errstr2[0] = '\0';
	return -1;
    }

    return fs->img_info->read_random(fs->img_info, fs->offset, buf, len,
	(OFF_T) addr * fs->block_size);
}
