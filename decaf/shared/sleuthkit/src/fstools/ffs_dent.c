/*
** ffs_dent
** The  Sleuth Kit 
**
** $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
**
** File name layer for a FFS/UFS image 
**
** Brian Carrier [carrier@sleuthkit.org]
** Copyright (c) 2006 Brian Carrier, Basis Technology.  All Rights reserved
** Copyright (c) 2003-2006 Brian Carrier.  All rights reserved 
**
** TASK
** Copyright (c) 2002 Brian Carrier, @stake Inc.  All rights reserved
**
** TCTUTILs
** Copyright (c) 2001 Brian Carrier.  All rights reserved
**
**
** This software is distributed under the Common Public License 1.0
**
*/

#include <ctype.h>
#include "fs_tools.h"
#include "ffs.h"


#define MAX_DEPTH   64
#define DIR_STRSZ  2048

typedef struct {
    /* Recursive path stuff */

    /* how deep in the directory tree are we */
    unsigned int depth;

    /* pointer in dirs string to where '/' is for given depth */
    char *didx[MAX_DEPTH];

    /* The current directory name string */
    char dirs[DIR_STRSZ];

} FFS_DINFO;

static uint8_t ffs_dent_walk_lcl(FS_INFO *, FFS_DINFO *, INUM_T, int,
    FS_DENT_WALK_FN, void *);


/* 
** copy OS specific directory inode to generic FS_DENT
 * 
 * Return 1 on error and 0 on success
*/
static uint8_t
ffs_dent_copy(FFS_INFO * ffs, FFS_DINFO * dinfo, char *ffs_dent,
    FS_DENT * fs_dent)
{
    FS_INFO *fs = &(ffs->fs_info);

    /* this one has the type field */
    if ((fs->ftype == FFS_1) || (fs->ftype == FFS_2)) {
	ffs_dentry1 *dir = (ffs_dentry1 *) ffs_dent;

	fs_dent->inode = getu32(fs, dir->d_ino);

	if (fs_dent->name_max != FFS_MAXNAMLEN) {
	    if ((fs_dent =
		    fs_dent_realloc(fs_dent, FFS_MAXNAMLEN)) == NULL)
		return 1;
	}

	/* ffs null terminates so we can strncpy */
	strncpy(fs_dent->name, dir->d_name, fs_dent->name_max);

	/* generic types are same as FFS */
	fs_dent->ent_type = dir->d_type;

    }
    else if (fs->ftype == FFS_1B) {
	ffs_dentry2 *dir = (ffs_dentry2 *) ffs_dent;

	fs_dent->inode = getu32(fs, dir->d_ino);

	if (fs_dent->name_max != FFS_MAXNAMLEN) {
	    if ((fs_dent =
		    fs_dent_realloc(fs_dent, FFS_MAXNAMLEN)) == NULL)
		return 1;
	}

	/* ffs null terminates so we can strncpy */
	strncpy(fs_dent->name, dir->d_name, fs_dent->name_max);

	fs_dent->ent_type = FS_DENT_UNDEF;
    }
    else {
	tsk_errno = TSK_ERR_FS_ARG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "ffs_dent_copy: Unknown FS type");
	tsk_errstr2[0] = '\0';
	return 1;
    }

    /* copy the path data */
    fs_dent->path = dinfo->dirs;
    fs_dent->pathdepth = dinfo->depth;

    if ((fs != NULL) && (fs_dent->inode)
	&& (fs_dent->inode <= fs->last_inum)) {
	/* Get inode */
	if (fs_dent->fsi)
	    fs_inode_free(fs_dent->fsi);

	if ((fs_dent->fsi = fs->inode_lookup(fs, fs_dent->inode)) == NULL) {
	    strncat(tsk_errstr2, " - ffs_dent_copy",
		TSK_ERRSTR_L - strlen(tsk_errstr2));
	    return 1;
	}
    }
    else {
	if (fs_dent->fsi)
	    fs_inode_free(fs_dent->fsi);
	fs_dent->fsi = NULL;
    }

    return 0;
}


/* Scan the buffer for directory entries and call action on each.
** Flags will be
** set to FS_FLAG_NAME_ALLOC for acive entires and FS_FLAG_NAME_UNALLOC for
** deleted ones
**
** len is size of buf
**
** return 0 on success, -1 on error, and 1 to stop 
*/
static int
ffs_dent_parse_block(FFS_INFO * ffs, FFS_DINFO * dinfo, char *buf,
    unsigned int len, int flags, FS_DENT_WALK_FN action, void *ptr)
{
    unsigned int idx;
    unsigned int inode = 0, dellen = 0, reclen = 0;
    unsigned int minreclen = 4;
    FS_INFO *fs = &(ffs->fs_info);

    char *dirPtr;
    FS_DENT *fs_dent;

    if ((fs_dent = fs_dent_alloc(FFS_MAXNAMLEN + 1, 0)) == NULL)
	return -1;

    /* update each time by the actual length instead of the
     ** recorded length so we can view the deleted entries 
     */
    for (idx = 0; idx <= len - FFS_DIRSIZ_lcl(1); idx += minreclen) {
	unsigned int namelen = 0;
	int myflags = 0;

	dirPtr = (char *) &buf[idx];

	/* copy to local variables */
	if ((fs->ftype == FFS_1) || (fs->ftype == FFS_2)) {
	    ffs_dentry1 *dir = (ffs_dentry1 *) dirPtr;
	    inode = getu32(fs, dir->d_ino);
	    namelen = dir->d_namlen;
	    reclen = getu16(fs, dir->d_reclen);
	}
	/* FFS_1B */
	else if (fs->ftype == FFS_1B) {
	    ffs_dentry2 *dir = (ffs_dentry2 *) dirPtr;
	    inode = getu32(fs, dir->d_ino);
	    namelen = getu16(fs, dir->d_namlen);
	    reclen = getu16(fs, dir->d_reclen);
	}

	/* what is the minimum size needed for this entry */
	minreclen = FFS_DIRSIZ_lcl(namelen);

	/* Perform a couple sanity checks 
	 ** OpenBSD never zeros the inode number, but solaris
	 ** does.  These checks will hopefully catch all non
	 ** entries 
	 */
	if ((inode > fs->last_inum) ||
	    (inode < 0) ||
	    (namelen > FFS_MAXNAMLEN) ||
	    (namelen <= 0) ||
	    (reclen < minreclen) || (reclen % 4) || (idx + reclen > len)) {

	    /* we don't have a valid entry, so skip ahead 4 */
	    minreclen = 4;
	    if (dellen > 0)
		dellen -= 4;
	    continue;
	}

	/* Before we process an entry in unallocated space, make
	 * sure that it also ends in the unalloc space */
	if ((dellen) && (dellen < minreclen)) {
	    minreclen = 4;
	    if (dellen)
		dellen -= 4;

	    continue;
	}

	/* the entry is valid */
	if (ffs_dent_copy(ffs, dinfo, dirPtr, fs_dent)) {
	    fs_dent_free(fs_dent);
	    return -1;
	}

	myflags = 0;
	/* Do we have a deleted entry? (are we in a deleted space) */
	if ((dellen > 0) || (inode == 0)) {
	    myflags |= FS_FLAG_NAME_UNALLOC;
	    if (dellen)
		dellen -= minreclen;

	    if (flags & FS_FLAG_NAME_UNALLOC) {
		int retval;
		retval = action(fs, fs_dent, myflags, ptr);
		if (retval == WALK_STOP) {
		    fs_dent_free(fs_dent);
		    return 1;
		}
		else if (retval == WALK_ERROR) {
		    fs_dent_free(fs_dent);
		    return -1;
		}
	    }
	}
	else {
	    myflags |= FS_FLAG_NAME_ALLOC;
	    if (flags & FS_FLAG_NAME_ALLOC) {
		int retval;
		retval = action(fs, fs_dent, myflags, ptr);
		if (retval == WALK_STOP) {
		    fs_dent_free(fs_dent);
		    return 1;
		}
		else if (retval == WALK_ERROR) {
		    fs_dent_free(fs_dent);
		    return -1;
		}
	    }
	}

	/* If we have some slack, the set dellen */
	if ((reclen != minreclen) && (dellen <= 0))
	    dellen = reclen - minreclen;


	/* if we have a directory and the RECURSE flag is set, then
	 * lets do it
	 */
	if ((myflags & FS_FLAG_NAME_ALLOC) &&
	    (flags & FS_FLAG_NAME_RECURSE) &&
	    (!ISDOT(fs_dent->name)) &&
	    ((fs_dent->fsi->mode & FS_INODE_FMT) == FS_INODE_DIR)) {

	    /* save the path */
	    if (dinfo->depth < MAX_DEPTH) {
		dinfo->didx[dinfo->depth] =
		    &dinfo->dirs[strlen(dinfo->dirs)];
		strncpy(dinfo->didx[dinfo->depth], fs_dent->name,
		    DIR_STRSZ - strlen(dinfo->dirs));
		strncat(dinfo->dirs, "/", DIR_STRSZ);
	    }
	    dinfo->depth++;

	    /* Call ourselves again */
	    if (ffs_dent_walk_lcl(&(ffs->fs_info), dinfo, fs_dent->inode,
		    flags, action, ptr)) {
		/* If the directory could not be loaded, 
		 * then move on */
		if (verbose) {
		    fprintf(stderr,
			"ffs_dent_parse_block: error reading directory: %"
			PRIuINUM "\n", fs_dent->inode);
		    tsk_error_print(stderr);
		}
		tsk_errno = 0;
		tsk_errstr[0] = '\0';
		tsk_errstr2[0] = '\0';
	    }

	    dinfo->depth--;
	    if (dinfo->depth < MAX_DEPTH)
		*dinfo->didx[dinfo->depth] = '\0';
	}

    }				/* end for size */

    fs_dent_free(fs_dent);
    return 0;
}				/* end ffs_dent_parse_block */



/* Process _inode_ as a directory inode and process the data blocks
** as file entries.  Call action on all entries with the flags set to
** FS_FLAG_NAME_ALLOC for active entries
**
**
** Use the following flags: FS_FLAG_NAME_ALLOC, FS_FLAG_NAME_UNALLOC, 
** FS_FLAG_NAME_RECURSE
 *
 * returns 1 on error and 0 on success
 */
uint8_t
ffs_dent_walk(FS_INFO * fs, INUM_T inode, int flags,
    FS_DENT_WALK_FN action, void *ptr)
{
    FFS_DINFO dinfo;
    memset(&dinfo, 0, sizeof(FFS_DINFO));

    return ffs_dent_walk_lcl(fs, &dinfo, inode, flags, action, ptr);
}

/* Return 0 on success and 1 on error */
static uint8_t
ffs_dent_walk_lcl(FS_INFO * fs, FFS_DINFO * dinfo, INUM_T inode, int flags,
    FS_DENT_WALK_FN action, void *ptr)
{
    OFF_T size;
    FS_INODE *fs_inode;
    FFS_INFO *ffs = (FFS_INFO *) fs;
    char *dirbuf;
    int nchnk, cidx;
    FS_LOAD_FILE load_file;
    int retval = 0;

    if (inode < fs->first_inum || inode > fs->last_inum) {
	tsk_errno = TSK_ERR_FS_WALK_RNG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "ffs_dent_walk_lcl: Invalid inode value: %" PRIuINUM, inode);
	tsk_errstr2[0] = '\0';
	return 1;
    }

    if (verbose)
	fprintf(stderr,
	    "ffs_dent_walk: Processing directory %" PRIuINUM "\n", inode);

    if ((fs_inode = fs->inode_lookup(fs, inode)) == NULL) {
	strncat(tsk_errstr2, " - ffs_dent_walk_lcl",
	    TSK_ERRSTR_L - strlen(tsk_errstr2));
	return 1;
    }

    /* make a copy of the directory contents that we can process */
    /* round up cause we want the slack space too */
    size = roundup(fs_inode->size, FFS_DIRBLKSIZ);
    if ((dirbuf = mymalloc(size)) == NULL) {
	fs_inode_free(fs_inode);
	return 1;
    }

    load_file.total = load_file.left = size;
    load_file.base = load_file.cur = dirbuf;

    if (fs->file_walk(fs, fs_inode, 0, 0,
	    FS_FLAG_FILE_SLACK | FS_FLAG_FILE_NOID,
	    load_file_action, (void *) &load_file)) {
	free(dirbuf);
	fs_inode_free(fs_inode);
	strncat(tsk_errstr2, " - ffs_dent_walk_lcl",
	    TSK_ERRSTR_L - strlen(tsk_errstr2));
	return 1;
    }

    /* Not all of the directory was copied, so we return */
    if (load_file.left > 0) {
	tsk_errno = TSK_ERR_FS_FWALK;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "ffs_dent_walk: Error reading directory %" PRIuINUM, inode);
	tsk_errstr2[0] = '\0';
	free(dirbuf);
	fs_inode_free(fs_inode);
	return 1;
    }

    /* Directory entries are written in chunks of DIRBLKSIZ
     ** determine how many chunks of this size we have to read to
     ** get a full block
     **
     ** Entries do not cross over the DIRBLKSIZ boundary
     */
    nchnk = (size) / (FFS_DIRBLKSIZ) + 1;

    for (cidx = 0; cidx < nchnk && size > 0; cidx++) {
	int len = (FFS_DIRBLKSIZ < size) ? FFS_DIRBLKSIZ : size;

	retval =
	    ffs_dent_parse_block(ffs, dinfo, dirbuf + cidx * FFS_DIRBLKSIZ,
	    len, flags, action, ptr);

	/* one is returned when the action wants to stop */
	if ((retval == 1) || (retval == -1))
	    break;

	size -= len;
    }

    fs_inode_free(fs_inode);
    free(dirbuf);
    if (retval == -1)
	return 1;
    else
	return 0;
}
