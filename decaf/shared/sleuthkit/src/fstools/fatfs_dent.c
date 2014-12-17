/*
** fatfs_dent
** The Sleuth Kit 
**
** $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
**
** Human interface Layer support for the FAT file system
**
** Brian Carrier [carrier@sleuthkit.org]
** Copyright (c) 2006 Brian Carrier, Basis Technology.  All Rights reserved
** Copyright (c) 2003-2005 Brian Carrier.  All rights reserved 
**
** TASK
** Copyright (c) 2002 Brian Carrier, @stake Inc.  All rights reserved
**
**
** This software is distributed under the Common Public License 1.0
**
** Unicode added with support from I.D.E.A.L. Technology Corp (Aug '05)
**
*/

#include "fs_tools.h"
#include "fatfs.h"
#include "fs_unicode.h"

/*
 * DESIGN NOTES
 *
 * the basic goal of this code is to parse directory entry structures for
 * file names.  The main function is fatfs_dent_walk, which takes an
 * inode value, reads in the contents of the directory into a buffer, 
 * and the processes the buffer.  
 *
 * The buffer is processed in directory entry size chunks and if the
 * entry meets tne flag requirements, an action function is called.
 *
 * One of the odd aspects of this code is that the 'inode' values are
 * the 'slot-address'.  Refer to the document on how FAT was implemented
 * for more details. This means that we need to search for the actual
 * 'inode' address for the '.' and '..' entries though!  The search
 * for '..' is quite painful if this code is called from a random 
 * location.  It does save what the parent is though, so the search
 * only has to be done once per session.
 */



/* Special data structure allocated for each directory to hold the long
 * file name entries until all entries have been found */
typedef struct {
    uint8_t name[FATFS_MAXNAMLEN_UTF8];	/* buffer for lfn - in reverse order */
    uint16_t start;		/* current start of name */
    uint8_t chk;		/* current checksum */
    uint8_t seq;		/* seq of first entry in lfn */
} FATFS_LFN;

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

    /* as FAT does not use inode numbers, we are making them up.  This causes
     * minor problems with the . and .. entries.  These variables help
     * us out with that
     */
    INUM_T curdir_inode;	/* the . inode */
    INUM_T pardir_inode;	/* the .. inode */


    /* We need to search for an inode addr based on starting cluster, 
     * these do it */
    DADDR_T find_clust;
    DADDR_T find_inode;

    /* Set to 1 when we are recursing down a deleted directory.  This will
     * supress the errors that may occur from invalid data
     */
    uint8_t recdel;

} FATFS_DINFO;


static uint8_t fatfs_dent_walk_lcl(FS_INFO *, FATFS_DINFO *, INUM_T, int,
    FS_DENT_WALK_FN, void *);


/**************************************************************************
 *
 * find_parent
 *
 *************************************************************************/

/*
 * this is the call back for the dent walk when we need to find the 
 * parent directory
 */
static uint8_t
find_parent_act(FS_INFO * fs, FS_DENT * fsd, int flags, void *ptr)
{
    FATFS_DINFO *dinfo = (FATFS_DINFO *) ptr;

    /* we found the directory entry that has allocated the cluster 
     * we are looking for */
    if (fsd->fsi->direct_addr[0] == dinfo->find_clust) {
	dinfo->find_inode = fsd->inode;
	return WALK_STOP;
    }
    return WALK_CONT;
}

/*
 * this function will find the parent inode of the directory
 * specified in fs_dent. It works by walking the directory tree
 * starting at the root.  
 *
 * return the inode number or 0 on error 
 */
static INUM_T
find_parent(FATFS_INFO * fatfs, FS_DENT * fs_dent)
{
    FS_INFO *fs = (FS_INFO *) & fatfs->fs_info;
    FATFS_DINFO dinfo;

    memset(&dinfo, 0, sizeof(FATFS_DINFO));

    /* set the value that the action function will use */
    dinfo.find_clust = fs_dent->fsi->direct_addr[0];

    if (verbose)
	fprintf(stderr,
	    "fatfs_find_parent: Looking for directory in cluster %"
	    PRIuDADDR "\n", dinfo.find_clust);

    /* Are we searching for the root directory? */
    if (fs->ftype == FAT32) {
	OFF_T clust = FATFS_SECT_2_CLUST(fatfs, fatfs->rootsect);

	if ((clust == dinfo.find_clust) || (dinfo.find_clust == 0)) {
	    return fs->root_inum;
	}
    }
    else {
	if ((dinfo.find_clust == 1) || (dinfo.find_clust == 0)) {
	    return fs->root_inum;
	}
    }

    if ((fs_dent->fsi->mode & FS_INODE_FMT) != FS_INODE_DIR) {
	tsk_errno = TSK_ERR_FS_ARG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "fatfs_find_parent called on a non-directory");
	return 0;
    }


    /* walk the inodes - looking for an inode that has allocated the
     * same first sector 
     */

    if (fatfs_dent_walk_lcl(fs, &dinfo, fs->root_inum,
	    FS_FLAG_NAME_ALLOC | FS_FLAG_NAME_RECURSE,
	    find_parent_act, (void *) &dinfo)) {
	return 0;
    }


    if (verbose)
	fprintf(stderr,
	    "fatfs_find_parent: Directory %" PRIuINUM
	    " found for cluster %" PRIuDADDR "\n", dinfo.find_inode,
	    dinfo.find_clust);

    /* if we didn't find anything then 0 will be returned */
    return dinfo.find_inode;
}


/* 
**
** Read contents of directory sector (in buf) with length len and
** original address of addr.  len should be a multiple of 512.
**
** fs_dent should be empty and is used to hold the data, but we allocate
** it only once... 
**
** flags: FS_FLAG_NAME_ALLOC, FS_FLAG_NAME_UNALLOC, FS_FLAG_NAME_RECURSE
**
** Return -1 on error, 0 on success, and 1 to stop
*/
static int8_t
fatfs_dent_parse_buf(FATFS_INFO * fatfs, FATFS_DINFO * dinfo,
    char *buf, int len,
    DADDR_T * addrs, int flags, FS_DENT_WALK_FN action, void *ptr)
{
    unsigned int idx, sidx;
    int a, b;
    INUM_T inode, ibase;
    fatfs_dentry *dep;
    FS_INFO *fs = (FS_INFO *) & fatfs->fs_info;
    int sectalloc;
    FS_DENT *fs_dent;
    FATFS_LFN lfninfo;

    if (buf == NULL) {
	tsk_errno = TSK_ERR_FS_ARG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "fatfs_dent_parse_buf: buffer is NULL");
	tsk_errstr2[0] = '\0';
	return -1;
    }

    dep = (fatfs_dentry *) buf;

    if ((fs_dent = fs_dent_alloc(FATFS_MAXNAMLEN_UTF8, 32)) == NULL) {
	return -1;
    }
    else if ((fs_dent->fsi =
	    fs_inode_alloc(FATFS_NDADDR, FATFS_NIADDR)) == NULL) {
	fs_dent_free(fs_dent);
	return -1;
    }

    memset(&lfninfo, 0, sizeof(FATFS_LFN));
    lfninfo.start = FATFS_MAXNAMLEN_UTF8 - 1;

    for (sidx = 0; sidx < (unsigned int) (len / 512); sidx++) {

	/* Get the base inode for this sector */
	ibase = FATFS_SECT_2_INODE(fatfs, addrs[sidx]);
	if (ibase > fs->last_inum) {
	    tsk_errno = TSK_ERR_FS_ARG;
	    snprintf(tsk_errstr, TSK_ERRSTR_L,
		"fatfs_parse: inode address is too large");
	    fs_dent_free(fs_dent);
	    return -1;
	}

	if (verbose)
	    fprintf(stderr,
		"fatfs_dent_parse_buf: Parsing sector %" PRIuDADDR
		"\n", addrs[sidx]);

	if ((sectalloc = is_sectalloc(fatfs, addrs[sidx])) == -1) {
	    fs_dent_free(fs_dent);
	    return -1;
	}

	/* cycle through the directory entries */
	for (idx = 0; idx < fatfs->dentry_cnt_se; idx++, dep++) {
	    fatfs_dentry *dir;
	    int myflags = 0;

	    /* is it a valid dentry? */
	    if (0 == fatfs_isdentry(fatfs, dep))
		continue;

	    /* Copy the directory entry into the FS_DENT structure */
	    dir = (fatfs_dentry *) dep;

	    inode = ibase + idx;

	    /* Take care of the name 
	     * Copy a long name to a buffer and take action if it
	     * is a small name */
	    if ((dir->attrib & FATFS_ATTR_LFN) == FATFS_ATTR_LFN) {
		fatfs_dentry_lfn *dirl = (fatfs_dentry_lfn *) dir;

		/* Store the name in dinfo until we get the 8.3 name 
		 * Use the checksum to identify a new sequence 
		 * */
		if (((dirl->seq & FATFS_LFN_SEQ_FIRST)
			&& (dirl->seq != FATFS_SLOT_DELETED))
		    || (dirl->chksum != lfninfo.chk)) {
		    // @@@ Do a partial output here


		    /* Reset the values */
		    lfninfo.seq = dirl->seq & FATFS_LFN_SEQ_MASK;
		    lfninfo.chk = dirl->chksum;
		    lfninfo.start = FATFS_MAXNAMLEN_UTF8 - 1;

		}
		else if (dirl->seq != lfninfo.seq - 1) {
		    // @@@ Check the sequence number - the checksum is correct though...

		}

		/* Copy the UTF16 values starting at end of buffer */
		for (a = 3; a >= 0; a--) {
		    if ((lfninfo.start > 0))
			lfninfo.name[lfninfo.start--] = dirl->part3[a];
		}
		for (a = 11; a >= 0; a--) {
		    if ((lfninfo.start > 0))
			lfninfo.name[lfninfo.start--] = dirl->part2[a];
		}
		for (a = 9; a >= 0; a--) {
		    if ((lfninfo.start > 0))
			lfninfo.name[lfninfo.start--] = dirl->part1[a];
		}

		// Skip ahead until we get a new sequence num or the 8.3 name
		continue;
	    }
	    /* Special case for volume label: name does not have an
	     * extension and we add a note at the end that it is a label */
	    else if ((dir->attrib & FATFS_ATTR_VOLUME) ==
		FATFS_ATTR_VOLUME) {
		a = 0;

		for (b = 0; b < 8; b++) {
		    if ((dir->name[b] != 0) && (dir->name[b] != 0xff)) {
			fs_dent->name[a++] = dir->name[b];
		    }
		}
		for (b = 0; b < 3; b++) {
		    if ((dir->ext[b] != 0) && (dir->ext[b] != 0xff)) {
			fs_dent->name[a++] = dir->ext[b];
		    }
		}

		fs_dent->name[a] = '\0';
		/* Append a string to show it is a label */
		if (a + 22 < FATFS_MAXNAMLEN_UTF8) {
		    char *volstr = " (Volume Label Entry)";
		    strncat(fs_dent->name, volstr,
			FATFS_MAXNAMLEN_UTF8 - a);
		}
	    }

	    /* A short (8.3) entry */
	    else {
		char *name_ptr;	// The dest location for the short name

		/* if we have a lfn, copy it in */
		if (lfninfo.start != FATFS_MAXNAMLEN_UTF8 - 1) {
		    int retVal;

		    /* @@@ Check the checksum */



		    /* Convert the UTF16 to UTF8 */
		    UTF16 *name16 =
			(UTF16 *) ((uintptr_t) & lfninfo.
			name[lfninfo.start + 1]);
		    UTF8 *name8 = (UTF8 *) fs_dent->name;

		    retVal = fs_UTF16toUTF8(fs, (const UTF16 **) &name16,
			(UTF16 *) & lfninfo.
			name[FATFS_MAXNAMLEN_UTF8],
			&name8,
			(UTF8 *) ((uintptr_t) name8 +
			    FATFS_MAXNAMLEN_UTF8), lenientConversion);

		    if (retVal != conversionOK) {
			tsk_errno = TSK_ERR_FS_UNICODE;
			snprintf(tsk_errstr, TSK_ERRSTR_L,
			    "fatfs_parse: Error converting FAT LFN to UTF8: %d",
			    retVal);
			continue;
		    }

		    /* Make sure it is NULL Terminated */
		    if ((uintptr_t) name8 >
			(uintptr_t) fs_dent->name + FATFS_MAXNAMLEN_UTF8)
			fs_dent->name[FATFS_MAXNAMLEN_UTF8 - 1] = '\0';
		    else
			*name8 = '\0';

		    lfninfo.start = FATFS_MAXNAMLEN_UTF8 - 1;
		    name_ptr = fs_dent->shrt_name;	// put 8.3 into shrt_name
		}
		else {
		    fs_dent->shrt_name[0] = '\0';
		    name_ptr = fs_dent->name;	// put 8.3 into normal location
		}


		/* copy in the short name, skipping spaces and putting in the . */
		a = 0;
		for (b = 0; b < 8; b++) {
		    if ((dir->name[b] != 0) && (dir->name[b] != 0xff) &&
			(dir->name[b] != 0x20)) {

			if ((b == 0)
			    && (dir->name[0] == FATFS_SLOT_DELETED)) {
			    name_ptr[a++] = '_';
			}
			else if ((dir->lowercase & FATFS_CASE_LOWER_BASE)
			    && (dir->name[b] >= 'A')
			    && (dir->name[b] <= 'Z')) {
			    name_ptr[a++] = dir->name[b] + 32;
			}
			else {
			    name_ptr[a++] = dir->name[b];
			}
		    }
		}

		for (b = 0; b < 3; b++) {
		    if ((dir->ext[b] != 0) && (dir->ext[b] != 0xff) &&
			(dir->ext[b] != 0x20)) {
			if (b == 0)
			    name_ptr[a++] = '.';
			if ((dir->lowercase & FATFS_CASE_LOWER_EXT) &&
			    (dir->ext[b] >= 'A') && (dir->ext[b] <= 'Z'))
			    name_ptr[a++] = dir->ext[b] + 32;
			else
			    name_ptr[a++] = dir->ext[b];
		    }
		}
		name_ptr[a] = '\0';
	    }

	    /* add the path data */
	    fs_dent->path = dinfo->dirs;
	    fs_dent->pathdepth = dinfo->depth;


	    /* file type: FAT only knows DIR and FILE */
	    if ((dir->attrib & FATFS_ATTR_DIRECTORY) ==
		FATFS_ATTR_DIRECTORY)
		fs_dent->ent_type = FS_DENT_DIR;
	    else
		fs_dent->ent_type = FS_DENT_REG;

	    /* Get inode */
	    fs_dent->inode = inode;
	    if (fatfs_dinode_copy(fatfs, fs_dent->fsi, dir, addrs[sidx],
		    inode)) {
		if (verbose)
		    fprintf(stderr,
			"fatfs_dent_parse: could not copy inode -- ignoring\n");
	    }


	    /* Handle the . and .. entries specially
	     * The current inode 'address' they have is for the current
	     * slot in the cluster, but it needs to refer to the original
	     * slot 
	     */
	    if (dep->name[0] == '.') {

		/* dinfo->curdir_inode is always set and we can copy it in */
		if (dep->name[1] == ' ')
		    inode = fs_dent->inode = dinfo->curdir_inode;

		/* dinfo->pardir_inode is not always set, so we may have to search */
		else if (dep->name[1] == '.') {

		    if ((!dinfo->pardir_inode) && (!dinfo->find_clust))
			dinfo->pardir_inode = find_parent(fatfs, fs_dent);

		    inode = fs_dent->inode = dinfo->pardir_inode;

		    /* If the .. entry is for the root directory, then make
		     * up the data
		     */
		    if (inode == fs->root_inum) {
			if (fatfs_make_root(fatfs, fs_dent->fsi)) {
			    if (verbose)
				fprintf(stderr,
				    "fatfs_dent_parse: could not make root directory -- ignoring\n");
			}
		    }
		}
	    }


	    /* The allocation status of an entry is based on the allocation
	     * status of the sector it is in and the flag.  Deleted directories
	     * do not always clear the flags of each entry
	     */
	    if (sectalloc == 1) {
		myflags = (dep->name[0] == FATFS_SLOT_DELETED) ?
		    FS_FLAG_NAME_UNALLOC : FS_FLAG_NAME_ALLOC;
	    }
	    else {
		myflags = FS_FLAG_NAME_UNALLOC;
	    }

	    if ((flags & myflags) == myflags) {
		int retval = action(fs, fs_dent, myflags, ptr);

		if (retval == WALK_STOP) {
		    fs_dent_free(fs_dent);
		    return 1;
		}
		else if (retval == WALK_ERROR) {
		    fs_dent_free(fs_dent);
		    return -1;
		}
	    }


	    /* if we have a directory and need to recurse then do it */
	    if (((fs_dent->fsi->mode & FS_INODE_FMT) == FS_INODE_DIR) &&
		(flags & FS_FLAG_NAME_RECURSE)
		&& (!ISDOT(fs_dent->name))) {

		INUM_T back_p = 0;
		uint8_t back_recdel = 0;


		/* append our name */
		if (dinfo->depth < MAX_DEPTH) {
		    dinfo->didx[dinfo->depth] =
			&dinfo->dirs[strlen(dinfo->dirs)];
		    strncpy(dinfo->didx[dinfo->depth], fs_dent->name,
			DIR_STRSZ - strlen(dinfo->dirs));
		    strncat(dinfo->dirs, "/", DIR_STRSZ);
		}

		/* save the .. inode value */
		back_p = dinfo->pardir_inode;
		dinfo->pardir_inode = dinfo->curdir_inode;
		dinfo->depth++;


		/* This will prevent errors from being generated from the invalid
		 * deleted files.  save the current setting and set it to del */
		if (myflags & FS_FLAG_NAME_ALLOC) {
		    back_recdel = dinfo->recdel;
		    dinfo->recdel = 1;
		}

		if (fatfs_dent_walk_lcl(&(fatfs->fs_info), dinfo,
			fs_dent->inode, flags, action, ptr)) {
		    /* If the directory could not be loaded,
		     *                  * then move on */
		    if (verbose) {
			fprintf(stderr,
			    "fatfs_dent_parse: error reading directory: %"
			    PRIuINUM "\n", fs_dent->inode);
			tsk_error_print(stderr);
		    }
		    tsk_errno = 0;
		    tsk_errstr[0] = '\0';

		}

		dinfo->depth--;
		dinfo->curdir_inode = dinfo->pardir_inode;
		dinfo->pardir_inode = back_p;

		if (dinfo->depth < MAX_DEPTH)
		    *dinfo->didx[dinfo->depth] = '\0';

		/* Restore the recursion setting */
		if (myflags & FS_FLAG_NAME_ALLOC) {
		    dinfo->recdel = back_recdel;
		}
	    }
	}
    }

    return 0;
}



/**************************************************************************
 *
 * dent_walk
 *
 *************************************************************************/

/* values used to copy the directory contents into a buffer */


typedef struct {
    /* ptr to the current location in a local buffer */
    char *curdirptr;

    /* number of bytes left in curdirptr */
    size_t dirleft;

    /* ptr to a local buffer for the stack of sector addresses */
    DADDR_T *curaddrbuf;

    /* num of entries allocated to curaddrbuf */
    size_t addrsize;

    /* The current index in the curaddrbuf stack */
    size_t addridx;

} FATFS_LOAD_DIR;



/* 
 * file_walk callback action to load directory contents 
 */
static uint8_t
fatfs_dent_action(FS_INFO * fs, DADDR_T addr, char *buf,
    unsigned int size, int flags, void *ptr)
{
    FATFS_LOAD_DIR *load = (FATFS_LOAD_DIR *) ptr;

    /* how much of the buffer are we copying */
    int len = (load->dirleft < size) ? load->dirleft : size;

    /* Copy the sector into a buffer and increment the pointers */
    memcpy(load->curdirptr, buf, len);
    load->curdirptr = (char *) ((uintptr_t) load->curdirptr + len);
    load->dirleft -= len;

    /* fill in the stack of addresses of sectors 
     *
     * if we are at the last entry, then realloc more */
    if (load->addridx == load->addrsize) {
	if (verbose)
	    fprintf(stderr, "fatfs_dent_action: realloc curaddrbuf");

	load->addrsize += 512;
	load->curaddrbuf = (DADDR_T *) myrealloc((char *) load->curaddrbuf,
	    load->addrsize * sizeof(DADDR_T));
	if (load->curaddrbuf == NULL) {
	    return WALK_ERROR;
	}
    }

    /* Add this sector to the stack */
    load->curaddrbuf[load->addridx++] = addr;

    if (load->dirleft)
	return WALK_CONT;
    else
	return WALK_STOP;
}


/* 
** The main function to do directory entry walking
**
** action is called for each entry with flags set to FS_FLAG_NAME_ALLOC for
** active entries
**
** Use the following flags: FS_FLAG_NAME_ALLOC, FS_FLAG_NAME_UNALLOC, 
** FS_FLAG_NAME_RECURSE
*/
uint8_t
fatfs_dent_walk(FS_INFO * fs, INUM_T inode, int flags,
    FS_DENT_WALK_FN action, void *ptr)
{
    FATFS_DINFO dinfo;

    memset(&dinfo, 0, sizeof(FATFS_DINFO));
    return fatfs_dent_walk_lcl(fs, &dinfo, inode, flags, action, ptr);
}


static uint8_t
fatfs_dent_walk_lcl(FS_INFO * fs, FATFS_DINFO * dinfo, INUM_T inode,
    int flags, FS_DENT_WALK_FN action, void *ptr)
{
    OFF_T size, len;
    FS_INODE *fs_inode;
    FATFS_INFO *fatfs = (FATFS_INFO *) fs;
    char *dirbuf;
    DADDR_T *addrbuf;
    FATFS_LOAD_DIR load;
    int retval;

    if ((inode < fs->first_inum) || (inode > fs->last_inum)) {
	tsk_errno = TSK_ERR_FS_WALK_RNG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "fatfs_dent_walk: invalid inode value: %" PRIuINUM "\n",
	    inode);
	return 1;
    }

    fs_inode = fs->inode_lookup(fs, inode);
    if (!fs_inode) {
	tsk_errno = TSK_ERR_FS_INODE_NUM;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "fatfs_dent_walk: %" PRIuINUM " is not a valid inode", inode);
	return 1;
    }

    size = fs_inode->size;
    len = roundup(size, 512);

    if (verbose)
	fprintf(stderr,
	    "fatfs_dent_walk: Processing directory %" PRIuINUM "\n",
	    inode);

    /* Save the current inode value ('.') */
    dinfo->curdir_inode = inode;

    /* Make a copy of the directory contents using file_walk */
    if ((dirbuf = mymalloc(len)) == NULL) {
	fs_inode_free(fs_inode);
	return 1;
    }
    memset(dirbuf, 0, len);
    load.curdirptr = dirbuf;
    load.dirleft = size;

    /* We are going to save the address of each sector in the directory
     * in a stack - they are needed to determine the inode address */
    load.addrsize = 512;
    addrbuf = (DADDR_T *) mymalloc(load.addrsize * sizeof(DADDR_T));
    if (addrbuf == NULL) {
	fs_inode_free(fs_inode);
	free(dirbuf);
	return 1;
    }

    /* Set the variables that are used during the copy */
    load.addridx = 0;
    load.curaddrbuf = addrbuf;

    /* save the directory contents into dirbuf */
    if (fs->file_walk(fs, fs_inode, 0, 0,
	    FS_FLAG_FILE_SLACK | FS_FLAG_FILE_RECOVER |
	    FS_FLAG_FILE_NOID, fatfs_dent_action, (void *) &load)) {
	fs_inode_free(fs_inode);
	free(dirbuf);
	strncat(tsk_errstr2, " - fatfs_dent_walk",
	    TSK_ERRSTR_L - strlen(tsk_errstr2));
	return 1;
    }

    /* We did not copy the entire directory, which occurs if an error occured */
    if (load.dirleft > 0) {

	tsk_errno = TSK_ERR_FS_FWALK;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "fatfs_dent_walk: Error reading directory %" PRIuINUM, inode);
	tsk_errstr2[0] = '\0';

	/* Free the local buffers */
	free(load.curaddrbuf);
	free(load.curdirptr);
	fs_inode_free(fs_inode);

	return 1;
    }

    /* Reset the local pointer because we could have done a realloc */
    addrbuf = load.curaddrbuf;
    retval =
	fatfs_dent_parse_buf(fatfs, dinfo, dirbuf, len, addrbuf, flags,
	action, ptr);


    fs_inode_free(fs_inode);
    free(dirbuf);
    free(addrbuf);

    return (retval == -1) ? 1 : 0;
}
