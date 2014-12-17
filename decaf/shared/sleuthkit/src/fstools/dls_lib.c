/*
** The Sleuth Kit
**
** $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
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
**
*/

/* TCT:
 *
 * LICENSE
 *	This software is distributed under the IBM Public License.
 * AUTHOR(S)
 *	Wietse Venema
 *	IBM T.J. Watson Research
 *	P.O. Box 704
 *	Yorktown Heights, NY 10598, USA
 */

#include "libfstools.h"



/* call backs for listing details 
 *
 * return 1 on error
 * */
static uint8_t
print_list_head(FS_INFO * fs)
{
    char hostnamebuf[BUFSIZ];
    time_t now;
    char unit[32];

    if (gethostname(hostnamebuf, sizeof(hostnamebuf) - 1) < 0) {
	if (verbose)
	    fprintf(stderr, "dls_lib: error getting hostname: %s\n",
		strerror(errno));
	strcpy(hostnamebuf, "unknown");
    }
    hostnamebuf[sizeof(hostnamebuf) - 1] = 0;
    now = time((time_t *) 0);

    switch (fs->ftype & FSMASK) {
    case EXTxFS_TYPE:
    case FFS_TYPE:
	strncpy(unit, "fragment", 32);
	break;
    case FATFS_TYPE:
    case ISO9660_TYPE:
	strncpy(unit, "sector", 32);
	break;
    case NTFS_TYPE:
	strncpy(unit, "cluster", 32);
	break;
    default:
	tsk_errno = TSK_ERR_FS_ARG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "dls_head: Unsupported File System");
	return 1;
    }

    /*
     * Identify table type and table origin.
     */
    printf("class|host|image|first_time|unit\n");
    printf("dls|%s||%lu|%s\n", hostnamebuf, (ULONG) now, unit);

    printf("addr|alloc\n");
    return 0;
}

static uint8_t
print_list(FS_INFO * fs, DADDR_T addr, char *buf, int flags, void *ptr)
{
    printf("%" PRIuDADDR "|%s\n", addr,
	(flags & FS_FLAG_DATA_ALLOC) ? "a" : "f");
    return WALK_CONT;
}



/* print_block - write data block to stdout */
static uint8_t
print_block(FS_INFO * fs, DADDR_T addr, char *buf, int flags, void *ptr)
{
    if (verbose)
	fprintf(stderr, "write block %" PRIuDADDR "\n", addr);

    if (fwrite(buf, fs->block_size, 1, stdout) != 1) {
	tsk_errno = TSK_ERR_FS_WRITE;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "dls_lib: error writing to stdout: %s", strerror(errno));
	return WALK_ERROR;
    }

    return WALK_CONT;
}



/* SLACK SPACE  call backs */
static OFF_T flen;

static uint8_t
slack_file_act(FS_INFO * fs, DADDR_T addr, char *buf,
    unsigned int size, int flags, void *ptr)
{

    if (verbose)
	fprintf(stderr,
	    "slack_file_act: Remaining File:  %" PRIuOFF
	    "  Buffer: %u\n", flen, size);

    /* This is not the last data unit */
    if (flen >= size) {
	flen -= size;
    }
    /* We have passed the end of the allocated space */
    else if (flen == 0) {
	fwrite(buf, size, 1, stdout);
    }
    /* This is the last data unit and there is unused space */
    else if (flen < size) {
	/* Clear the used space and print it */
	memset(buf, 0, flen);
	fwrite(buf, size, 1, stdout);
	flen = 0;
    }

    return WALK_CONT;
}

/* Call back for inode_walk */
static uint8_t
slack_inode_act(FS_INFO * fs, FS_INODE * fs_inode, int flags, void *ptr)
{

    if (verbose)
	fprintf(stderr,
	    "slack_inode_act: Processing meta data: %" PRIuINUM "\n",
	    fs_inode->addr);

    /* We will now do a file walk on the content and print the
     * data after the specified size of the file */
    if ((fs->ftype & FSMASK) != NTFS_TYPE) {
	flen = fs_inode->size;
	if (fs->file_walk(fs, fs_inode, 0, 0,
		FS_FLAG_FILE_SLACK |
		FS_FLAG_FILE_NOID, slack_file_act, ptr)) {
	    if (verbose)
		fprintf(stderr,
		    "slack_inode_act: error walking file: %" PRIuINUM,
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
			FS_FLAG_FILE_SLACK, slack_file_act, ptr)) {
		    if (verbose)
			fprintf(stderr,
			    "slack_inode_act: error walking file: %"
			    PRIuINUM, fs_inode->addr);
		    tsk_errno = 0;
		}
	    }

	    fs_data = fs_data->next;
	}
    }

    return WALK_CONT;
}



/* Return 1 on error and 0 on success */
uint8_t
fs_dls(FS_INFO * fs, uint8_t lclflags, DADDR_T bstart, DADDR_T blast,
    int flags)
{
    if (lclflags & DLS_SLACK) {
	/* get the info on each allocated inode */
	if (fs->inode_walk(fs, fs->first_inum, fs->last_inum,
		(FS_FLAG_META_ALLOC | FS_FLAG_META_USED |
		    FS_FLAG_META_LINK), slack_inode_act, NULL))
	    return 1;
    }
    else if (lclflags & DLS_LIST) {
	if (print_list_head(fs))
	    return 1;

	if (fs->block_walk(fs, bstart, blast, flags, print_list, NULL))
	    return 1;
    }
    else {
	if (fs->block_walk(fs, bstart, blast, flags, print_block, NULL))
	    return 1;
    }

    return 0;
}
