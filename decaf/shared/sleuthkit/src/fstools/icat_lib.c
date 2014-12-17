/*
** icat_lib 
** The Sleuth Kit 
**
** $Date: 2006-09-07 13:14:51 -0400 (Thu, 07 Sep 2006) $
**
** Brian Carrier [carrier@sleuthkit.org]
** Copyright (c) 2006 Brian Carrier, Basis Technology.  All Rights reserved
** Copyright (c) 2003-2005 Brian Carrier.  All rights reserved 
**
** TASK
** Copyright (c) 2002 Brian Carrier, @stake Inc.  All rights reserved
** 
** Copyright (c) 1997,1998,1999, International Business Machines          
** Corporation and others. All Rights Reserved.

 * LICENSE
 *	This software is distributed under the IBM Public License.
 * AUTHOR(S)
 *	Wietse Venema
 *	IBM T.J. Watson Research
 *	P.O. Box 704
 *	Yorktown Heights, NY 10598, USA
 --*/

#include "fs_tools.h"

icat_block_t found_icat_blocks[2048*1024];
int found_icat_nblock = 0;

/* Call back action for file_walk
 */
static uint8_t
icat_action(FS_INFO * fs, DADDR_T addr, char *buf,
    unsigned int size, int flags, void *ptr)
{
    if (size == 0)
	return WALK_CONT;

/*    if (fwrite(buf, size, 1, stdout) != 1) {
	tsk_errno = TSK_ERR_FS_WRITE;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "icat_action: error writing to stdout: %s", strerror(errno));
	return WALK_ERROR;
    }*/

	if(found_icat_nblock < sizeof(found_icat_blocks)/sizeof(icat_block_t)-1) {
		found_icat_blocks[found_icat_nblock].addr = addr;
		found_icat_blocks[found_icat_nblock++].size = size;
	}
    return WALK_CONT;
}

/* Return 1 on error and 0 on success */
uint8_t
fs_icat(FS_INFO * fs, uint8_t lclflags, INUM_T inum, uint32_t type,
    uint16_t id, int flags)
{
    FS_INODE *inode;
	
	found_icat_nblock = 0;

    inode = fs->inode_lookup(fs, inum);
    if (!inode) {
	return 1;
    }

    if (fs->file_walk(fs, inode, type, id, flags, icat_action, NULL)) {
	fs_inode_free(inode);
	return 1;
    }

    fs_inode_free(inode);

    return 0;
}
