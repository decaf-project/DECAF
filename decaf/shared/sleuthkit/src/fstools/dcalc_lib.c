/*
** dcalc
** The Sleuth Kit 
**
** $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
**
** Calculates the corresponding block number between 'dls' and 'dd' images
** when given an 'dls' block number, it determines the block number it
** had in a 'dd' image.  When given a 'dd' image, it determines the
** value it would have in a 'dls' image (if the block is unallocated)
**
** Brian Carrier [carrier@sleuthkit.org]
** Copyright (c) 2006 Brian Carrier, Basis Technology.  All Rights reserved
** Copyright (c) 2003-2005 Brian Carrier. All Rights reserved
**
** TASK
** Copyright (c) 2002 Brian Carrier, @stake Inc. All Rights reserved
**
** TCTUTILs
** Copyright (c) 2001 Brian Carrier.  All rights reserved
**
**
** This software is distributed under the Common Public License 1.0
**
*/
#include "libfstools.h"


static DADDR_T count;
static DADDR_T uncnt = 0;

static uint8_t found = 0;



/* function used when -d is given
**
** keeps a count of unallocated blocks seen thus far
**
** If the specified block is allocated, an error is given, else the
** count of unalloc blocks is given 
**
** This is called for all blocks (alloc and unalloc)
*/
static uint8_t
count_dd_act(FS_INFO * fs, DADDR_T addr, char *buf, int flags, void *ptr)
{
    if (flags & FS_FLAG_DATA_UNALLOC)
	uncnt++;

    if (count-- == 0) {
	if (flags & FS_FLAG_DATA_UNALLOC)
	    printf("%" PRIuDADDR "\n", uncnt);
	else
	    printf
		("ERROR: unit is allocated, it will not be in an dls image\n");

	found = 1;
	return WALK_STOP;
    }
    return WALK_CONT;
}

/*
** count how many unalloc blocks there are.
**
** This is called for unalloc blocks only
*/
static uint8_t
count_dls_act(FS_INFO * fs, DADDR_T addr, char *buf, int flags, void *ptr)
{
    if (count-- == 0) {
	printf("%" PRIuDADDR "\n", addr);
	found = 1;
	return WALK_STOP;
    }
    return WALK_CONT;
}


/* SLACK SPACE  call backs */
static OFF_T flen;

static uint8_t
count_slack_file_act(FS_INFO * fs, DADDR_T addr, char *buf,
    unsigned int size, int flags, void *ptr)
{

    if (verbose)
	fprintf(stderr,
	    "count_slack_file_act: Remaining File:  %lu  Buffer: %lu\n",
	    (ULONG) flen, (ULONG) size);

    /* This is not the last data unit */
    if (flen >= size) {
	flen -= size;
    }
    /* We have passed the end of the allocated space */
    else if (flen == 0) {
	if (count-- == 0) {
	    printf("%" PRIuDADDR "\n", addr);
	    found = 1;
	    return WALK_STOP;

	}
    }
    /* This is the last data unit and there is unused space */
    else if (flen < size) {
	if (count-- == 0) {
	    printf("%" PRIuDADDR "\n", addr);
	    found = 1;
	    return WALK_STOP;

	}
	flen = 0;
    }

    return WALK_CONT;
}

static uint8_t
count_slack_inode_act(FS_INFO * fs, FS_INODE * fs_inode,
    int flags, void *ptr)
{

    if (verbose)
	fprintf(stderr,
	    "count_slack_inode_act: Processing meta data: %" PRIuINUM
	    "\n", fs_inode->addr);

    /* We will now do a file walk on the content */
    if ((fs->ftype & FSMASK) != NTFS_TYPE) {
	flen = fs_inode->size;
	if (fs->file_walk(fs, fs_inode, 0, 0,
		FS_FLAG_FILE_SLACK |
		FS_FLAG_FILE_NOID, count_slack_file_act, ptr)) {

	    /* ignore any errors */
	    if (verbose)
		fprintf(stderr, "Error walking file %" PRIuINUM,
		    fs_inode->addr);
	    tsk_errno = 0;
	}
    }

    /* For NTFS we go through each non-resident attribute */
    else {
	FS_DATA *fs_data = fs_inode->attr;

	while ((fs_data) && (fs_data->flags & FS_DATA_INUSE)) {

	    if (fs_data->flags & FS_DATA_NONRES) {
		flen = fs_data->size;
		if (fs->file_walk(fs, fs_inode, fs_data->type, fs_data->id,
			FS_FLAG_FILE_SLACK, count_slack_file_act, ptr)) {
		    /* ignore any errors */
		    if (verbose)
			fprintf(stderr, "Error walking file %" PRIuINUM,
			    fs_inode->addr);
		    tsk_errno = 0;
		}
	    }

	    fs_data = fs_data->next;
	}
    }
    return WALK_CONT;
}




/* Return 1 if block is not found, 0 if it was found, and -1 on error */
int8_t
fs_dcalc(FS_INFO * fs, uint8_t lclflags, DADDR_T cnt)
{
    count = cnt;

    found = 0;

    if (lclflags == DCALC_DLS) {
	if (fs->block_walk(fs, fs->first_block, fs->last_block,
		(FS_FLAG_DATA_UNALLOC | FS_FLAG_DATA_ALIGN |
		    FS_FLAG_DATA_META | FS_FLAG_DATA_CONT),
		count_dls_act, NULL))
	    return -1;
    }
    else if (lclflags == DCALC_DD) {
	if (fs->block_walk(fs, fs->first_block, fs->last_block,
		(FS_FLAG_DATA_ALLOC | FS_FLAG_DATA_UNALLOC |
		    FS_FLAG_DATA_ALIGN | FS_FLAG_DATA_META |
		    FS_FLAG_DATA_CONT), count_dd_act, NULL))
	    return -1;
    }
    else if (lclflags == DCALC_SLACK) {
	if (fs->inode_walk(fs, fs->first_inum, fs->last_inum,
		(FS_FLAG_META_ALLOC | FS_FLAG_META_USED |
		    FS_FLAG_META_LINK), count_slack_inode_act, NULL))
	    return -1;
    }

    if (found == 0) {
	printf("Block too large\n");
	return 1;
    }
    else {
	return 0;
    }
}
