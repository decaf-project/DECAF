/*
** ntfs_dent
** The Sleuth Kit
**
** $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
**
** name layer support for the NTFS file system
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
#include "fs_data.h"
#include "ntfs.h"
#include "fs_unicode.h"


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

} NTFS_DINFO;


static uint8_t
ntfs_dent_walk_lcl(FS_INFO *, NTFS_DINFO *, INUM_T, int,
    FS_DENT_WALK_FN, void *);
/* 
 * copy the index (directory) entry into the generic structure
 *
 * uses the global variables 'dirs' and 'depth'
 *
 * Returns 1 on eror and 0 on success
 */
static uint8_t
ntfs_dent_copy(NTFS_INFO * ntfs, NTFS_DINFO * dinfo, ntfs_idxentry * idxe,
    FS_DENT * fs_dent)
{
    ntfs_attr_fname *fname = (ntfs_attr_fname *) & idxe->stream;
    FS_INFO *fs = (FS_INFO *) & ntfs->fs_info;
    UTF16 *name16;
    UTF8 *name8;
    int retVal;

    fs_dent->inode = getu48(fs, idxe->file_ref);

    name16 = (UTF16 *) & fname->name;
    name8 = (UTF8 *) fs_dent->name;

    retVal = fs_UTF16toUTF8(fs, (const UTF16 **) &name16,
	(UTF16 *) ((uintptr_t) name16 +
	    fname->nlen * 2), &name8,
	(UTF8 *) ((uintptr_t) name8 +
	    fs_dent->name_max), lenientConversion);

    if (retVal != conversionOK) {
	*name8 = '\0';
	if (verbose)
	    fprintf(stderr,
		"Error converting NTFS name to UTF8: %d %" PRIuINUM,
		retVal, fs_dent->inode);
    }

    /* Make sure it is NULL Terminated */
    if ((uintptr_t) name8 > (uintptr_t) fs_dent->name + fs_dent->name_max)
	fs_dent->name[fs_dent->name_max] = '\0';
    else
	*name8 = '\0';

    /* copy the path data */
    fs_dent->path = dinfo->dirs;
    fs_dent->pathdepth = dinfo->depth;

    /* Get the actual inode */
    if (fs_dent->fsi != NULL)
	fs_inode_free(fs_dent->fsi);

    fs_dent->fsi = fs->inode_lookup(fs, fs_dent->inode);
    if (verbose)
	fprintf(stderr,
	    "ntfs_dent_copy: error looking up inode: %" PRIuINUM "\n",
	    fs_dent->inode);

    if (getu64(fs, fname->flags) & NTFS_FNAME_FLAGS_DIR)
	fs_dent->ent_type = FS_DENT_DIR;
    else
	fs_dent->ent_type = FS_DENT_REG;

    return 0;
}




/* This is a sanity check to see if the time is valid
 * it is divided by 100 to keep it in a 32-bit integer 
 */

static uint8_t
is_time(uint64_t t)
{
#define SEC_BTWN_1601_1970_DIV100 ((369*365 + 89) * 24 * 36)
#define SEC_BTWN_1601_2010_DIV100 (SEC_BTWN_1601_1970_DIV100 + (40*365 + 6) * 24 * 36)

    t /= 1000000000;		/* put the time in seconds div by additional 100 */

    if (!t)
	return 0;

    if (t < SEC_BTWN_1601_1970_DIV100)
	return 0;

    if (t > SEC_BTWN_1601_2010_DIV100)
	return 0;

    return 1;
}

/* 
 * Process a list of directory entries, starting at idxe with a length
 * of _size_ bytes.  _len_ is the length that is reported in the 
 * idxelist header.  in other words, everything after _len_ bytes is
 * considered unallocated area and therefore deleted content 
 *
 * The only flag that we care about is ALLOC and UNALLOC
 *
 * Return 1 to stop, 0 on success, and -1 on error
 */
static int
ntfs_dent_idxentry(NTFS_INFO * ntfs, NTFS_DINFO * dinfo,
    ntfs_idxentry * idxe, uint32_t size, uint32_t len,
    int flags, FS_DENT_WALK_FN action, void *ptr)
{
    uintptr_t endaddr, endaddr_alloc;
    int myflags = 0;
    FS_DENT *fs_dent;
    FS_INFO *fs = (FS_INFO *) & ntfs->fs_info;

    if ((fs_dent = fs_dent_alloc(NTFS_MAXNAMLEN_UTF8, 0)) == NULL) {
	return -1;
    }

    if (verbose)
	fprintf(stderr,
	    "ntfs_dent_idxentry: Processing entry: %" PRIu64
	    "  Size: %" PRIu32 "  Len: %" PRIu32 "\n",
	    (uint64_t) ((uintptr_t) idxe), size, len);

    /* where is the end of the buffer */
    endaddr = ((uintptr_t) idxe + size);

    /* where is the end of the allocated data */
    endaddr_alloc = ((uintptr_t) idxe + len);

    /* cycle through the index entries, based on provided size */
    while (((uintptr_t) & (idxe->stream) + sizeof(ntfs_attr_fname)) <
	endaddr) {

	ntfs_attr_fname *fname = (ntfs_attr_fname *) & idxe->stream;


	if (verbose)
	    fprintf(stderr,
		"ntfs_dent_idxentry: New IdxEnt: %" PRIu64
		" $FILE_NAME Entry: %" PRIu64 "  File Ref: %" PRIu64
		"  IdxEnt Len: %" PRIu16 "  StrLen: %" PRIu16 "\n",
		(uint64_t) ((uintptr_t) idxe),
		(uint64_t) ((uintptr_t) fname), (uint64_t) getu48(fs,
		    idxe->
		    file_ref),
		getu16(fs, idxe->idxlen), getu16(fs, idxe->strlen));

	/* perform some sanity checks on index buffer head
	 * and advance by 4-bytes if invalid
	 */
	if ((getu48(fs, idxe->file_ref) > fs->last_inum) ||
	    (getu48(fs, idxe->file_ref) < fs->first_inum) ||
	    (getu16(fs, idxe->idxlen) <= getu16(fs, idxe->strlen)) ||
	    (getu16(fs, idxe->idxlen) % 4) ||
	    (getu16(fs, idxe->idxlen) > size)) {
	    idxe = (ntfs_idxentry *) ((uintptr_t) idxe + 4);
	    continue;
	}

	/* do some sanity checks on the deleted entries
	 */
	if ((getu16(fs, idxe->strlen) == 0) ||
	    (((uintptr_t) idxe + getu16(fs, idxe->idxlen)) >
		endaddr_alloc)) {

	    /* name space checks */
	    if ((fname->nspace != NTFS_FNAME_POSIX) &&
		(fname->nspace != NTFS_FNAME_WIN32) &&
		(fname->nspace != NTFS_FNAME_DOS) &&
		(fname->nspace != NTFS_FNAME_WINDOS)) {
		idxe = (ntfs_idxentry *) ((uintptr_t) idxe + 4);
		continue;
	    }

	    if ((getu64(fs, fname->alloc_fsize) <
		    getu64(fs, fname->real_fsize)) || (fname->nlen == 0)
		|| (*(uint8_t *) & fname->name == 0)) {

		idxe = (ntfs_idxentry *) ((uintptr_t) idxe + 4);
		continue;
	    }

	    if ((is_time(getu64(fs, fname->crtime)) == 0) ||
		(is_time(getu64(fs, fname->atime)) == 0) ||
		(is_time(getu64(fs, fname->mtime)) == 0)) {

		idxe = (ntfs_idxentry *) ((uintptr_t) idxe + 4);
		continue;
	    }
	}

	/* For all fname entries, there will exist a DOS style 8.3 
	 * entry.  We don't process those because we already processed
	 * them before in their full version.  If the type is 
	 * full POSIX or WIN32 that does not satisfy DOS, then a 
	 * type NTFS_FNAME_DOS will exist.  If the name is WIN32,
	 * but already satisfies DOS, then a type NTFS_FNAME_WINDOS
	 * will exist 
	 *
	 * Note that we could be missing some info from deleted files
	 * if the windows version was deleted and the DOS wasn't...
	 *
	 * @@@ This should be added to the shrt_name entry of FS_DENT.  The short
	 * name entry typically comes after the long name
	 */

	if (fname->nspace == NTFS_FNAME_DOS) {
	    if (verbose)
		fprintf(stderr,
		    "ntfs_dent_idxentry: Skipping because of name space: %d\n",
		    fname->nspace);

	    goto incr_entry;
	}

	/* Copy it into the generic form */
	ntfs_dent_copy(ntfs, dinfo, idxe, fs_dent);

	if (verbose)
	    fprintf(stderr,
		"ntfs_dent_idxentry: Deletion Check Details of %s: Str Len: %"
		PRIu16 "  Len to end after current: %" PRIu64 "\n",
		fs_dent->name, getu16(fs, idxe->strlen),
		(uint64_t) (endaddr_alloc - (uintptr_t) idxe -
		    getu16(fs, idxe->idxlen)));

	/* 
	 * Check if this entry is deleted
	 *
	 * The final check is to see if the end of this entry is 
	 * within the space that the idxallocbuf claimed was valid
	 */
	if ((getu16(fs, idxe->strlen) == 0) ||
	    (((uintptr_t) idxe + getu16(fs, idxe->idxlen)) >
		endaddr_alloc)) {

	    /* we know deleted entries with an inode of 0 are not legit because
	     * that is the MFT value.  Free it so it does not confuse
	     * people with invalid data
	     */
	    if (fs_dent->inode == 0) {
		fs_inode_free(fs_dent->fsi);
		fs_dent->fsi = NULL;
	    }
	    myflags = FS_FLAG_NAME_UNALLOC;
	}
	else {
	    myflags = FS_FLAG_NAME_ALLOC;
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

	/* Recurse if we need to */
	if ((myflags & FS_FLAG_NAME_ALLOC) &&
	    (flags & FS_FLAG_NAME_RECURSE) &&
	    (!ISDOT(fs_dent->name)) &&
	    ((fs_dent->fsi->mode & FS_INODE_FMT) == FS_INODE_DIR) &&
	    (fs_dent->inode)) {

	    if (dinfo->depth < MAX_DEPTH) {
		dinfo->didx[dinfo->depth] =
		    &dinfo->dirs[strlen(dinfo->dirs)];
		strncpy(dinfo->didx[dinfo->depth], fs_dent->name,
		    DIR_STRSZ - strlen(dinfo->dirs));
		strncat(dinfo->dirs, "/", DIR_STRSZ);
	    }
	    dinfo->depth++;

	    if (ntfs_dent_walk_lcl(&(ntfs->fs_info), dinfo, fs_dent->inode,
		    flags, action, ptr)) {
		if (verbose)
		    fprintf(stderr, "Error recursing into directory\n");
		tsk_errno = 0;

	    }

	    dinfo->depth--;
	    if (dinfo->depth < MAX_DEPTH)
		*dinfo->didx[dinfo->depth] = '\0';

	}			/* end of recurse */

      incr_entry:

	/* the theory here is that deleted entries have strlen == 0 and
	 * have been found to have idxlen == 16
	 *
	 * if the strlen is 0, then guess how much the indexlen was
	 * before it was deleted
	 */

	/* 16: size of idxentry before stream
	 * 66: size of fname before name
	 * 2*nlen: size of name (in unicode)
	 */
	if (getu16(fs, idxe->strlen) == 0) {
	    idxe =
		(ntfs_idxentry
		*) ((((uintptr_t) idxe + 16 + 66 + 2 * fname->nlen +
			3) / 4) * 4);
	}
	else {
	    idxe =
		(ntfs_idxentry *) ((uintptr_t) idxe +
		getu16(fs, idxe->idxlen));
	}

    }				/* end of loop of index entries */

    fs_dent_free(fs_dent);
    return 0;
}




/*
 * remove the update sequence values that are changed in the last two 
 * bytes of each sector 
 *
 * return 1 on error and 0 on success
 */
static uint8_t
ntfs_fix_idxrec(NTFS_INFO * ntfs, ntfs_idxrec * idxrec, uint32_t len)
{
    int i;
    uint16_t orig_seq;
    FS_INFO *fs = (FS_INFO *) & ntfs->fs_info;
    ntfs_upd *upd;

    if (verbose)
	fprintf(stderr,
	    "ntfs_fix_idxrec: Fixing idxrec: %" PRIu64 "  Len: %"
	    PRIu32 "\n", (uint64_t) ((uintptr_t) idxrec), len);

    /* sanity check so we don't run over in the next loop */
    if ((unsigned int) ((getu16(fs, idxrec->upd_cnt) - 1) *
	    ntfs->ssize_b) > len) {
	tsk_errno = TSK_ERR_FS_INODE_INT;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "fix_idxrec: More Update Sequence Entries than idx record size");
	return 1;
    }

    /* Apply the update sequence structure template */
    upd = (ntfs_upd *) ((uintptr_t) idxrec + getu16(fs, idxrec->upd_off));

    /* Get the sequence value that each 16-bit value should be */
    orig_seq = getu16(fs, upd->upd_val);

    /* cycle through each sector */
    for (i = 1; i < getu16(fs, idxrec->upd_cnt); i++) {

	/* The offset into the buffer of the value to analyze */
	int offset = i * ntfs->ssize_b - 2;
	uint8_t *new_val, *old_val;

	/* get the current sequence value */
	uint16_t cur_seq = getu16(fs, (uintptr_t) idxrec + offset);

	if (cur_seq != orig_seq) {
	    /* get the replacement value */
	    uint16_t cur_repl = getu16(fs, &upd->upd_seq + (i - 1) * 2);

	    tsk_errno = TSK_ERR_FS_INODE_INT;
	    snprintf(tsk_errstr, TSK_ERRSTR_L,
		"fix_idxrec: Incorrect update sequence value in index buffer\nUpdate Value: 0x%"
		PRIx16 " Actual Value: 0x%" PRIx16
		" Replacement Value: 0x%" PRIx16
		"\nThis is typically because of a corrupted entry",
		orig_seq, cur_seq, cur_repl);
	    return 1;
	}

	new_val = &upd->upd_seq + (i - 1) * 2;
	old_val = (uint8_t *) ((uintptr_t) idxrec + offset);

	if (verbose)
	    fprintf(stderr,
		"ntfs_fix_idxrec: upd_seq %i   Replacing: %.4" PRIx16
		"   With: %.4" PRIx16 "\n", i, getu16(fs, old_val),
		getu16(fs, new_val));

	*old_val++ = *new_val++;
	*old_val = *new_val;
    }

    return 0;
}





/* 
 * This function looks up the inode and processes its tree
 *
 */
uint8_t
ntfs_dent_walk(FS_INFO * fs, INUM_T inum, int flags,
    FS_DENT_WALK_FN action, void *ptr)
{
    NTFS_DINFO dinfo;

    memset(&dinfo, 0, sizeof(NTFS_DINFO));

    return ntfs_dent_walk_lcl(fs, &dinfo, inum, flags, action, ptr);
}


static uint8_t
ntfs_dent_walk_lcl(FS_INFO * fs, NTFS_DINFO * dinfo, INUM_T inum,
    int flags, FS_DENT_WALK_FN action, void *ptr)
{

    NTFS_INFO *ntfs = (NTFS_INFO *) fs;
    FS_INODE *fs_inode;
    FS_DATA *fs_data_root, *fs_data_alloc;
    char *idxalloc;
    ntfs_idxentry *idxe;
    ntfs_idxroot *idxroot;
    ntfs_idxelist *idxelist;
    ntfs_idxrec *idxrec_p, *idxrec;
    int off, idxalloc_len;
    FS_LOAD_FILE load_file;
    int retval;

    /* sanity check */
    if (inum < fs->first_inum || inum > fs->last_inum) {
	tsk_errno = TSK_ERR_FS_WALK_RNG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "ntfs_dent_walk: inode value: %" PRIuINUM "\n", inum);
	return 1;
    }

    if (verbose)
	fprintf(stderr,
	    "ntfs_dent_walk: Processing directory %" PRIuINUM "\n", inum);

    /* Get the inode and verify it has attributes */
    fs_inode = fs->inode_lookup(fs, inum);
    if (!fs_inode) {
	strncat(tsk_errstr2, " - ntfs_dent_walk",
	    TSK_ERRSTR_L - strlen(tsk_errstr2));
	return 1;
    }
    if (!fs_inode->attr) {
	tsk_errno = TSK_ERR_FS_INODE_INT;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "dent_walk: Error: Directory address %" PRIuINUM
	    " has no attributes", inum);
	return 1;
    }


    /* 
     * Read the Index Root Attribute  -- we do some sanity checking here
     * to report errors before we start to make up data for the "." and ".."
     * entries
     */
    fs_data_root = fs_data_lookup_noid(fs_inode->attr, NTFS_ATYPE_IDXROOT);
    if (!fs_data_root) {
	strncat(tsk_errstr2, " - dent_walk: $IDX_ROOT not found",
	    TSK_ERRSTR_L - strlen(tsk_errstr2));
	fs_inode_free(fs_inode);
	return 1;
    }

    if (fs_data_root->flags & FS_DATA_NONRES) {
	tsk_errno = TSK_ERR_FS_INODE_INT;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "dent_walk: $IDX_ROOT is not resident - it should be");
	fs_inode_free(fs_inode);
	return 1;
    }
    idxroot = (ntfs_idxroot *) fs_data_root->buf;

    /* Verify that the attribute type is $FILE_NAME */
    if (getu32(fs, idxroot->type) == 0) {
	tsk_errno = TSK_ERR_FS_INODE_INT;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "dent_walk: Attribute type in index root is 0");
	fs_inode_free(fs_inode);
	return 1;
    }
    else if (getu32(fs, idxroot->type) != NTFS_ATYPE_FNAME) {
	tsk_errno = TSK_ERR_FS_INODE_INT;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "ERROR: Directory index is sorted by type: %" PRIu32
	    ".\nOnly $FNAME is currently supported", getu32(fs,
		idxroot->type));
	return 1;
    }

    /* Get the header of the index entry list */
    idxelist = &idxroot->list;

    /* Get the offset to the start of the index entry list */
    idxe = (ntfs_idxentry *) ((uintptr_t) idxelist +
	getu32(fs, idxelist->begin_off));

    /* 
     * NTFS does not have "." and ".." entries in the index trees
     * (except for a "." entry in the root directory)
     * 
     * So, we'll make 'em up by making a FS_DENT structure for
     * a '.' and '..' entry and call the action
     */
    if ((inum != fs->root_inum) && (flags & FS_FLAG_NAME_ALLOC)) {
	FS_DENT *fs_dent;
	FS_NAME *fs_name;
	int myflags = FS_FLAG_NAME_ALLOC;

	if (verbose)
	    fprintf(stderr, "ntfs_dent_walk: Creating . and .. entries\n");

	if ((fs_dent = fs_dent_alloc(16, 0)) == NULL) {
	    fs_inode_free(fs_inode);
	    return 1;
	}
	/* 
	 * "." 
	 */
	fs_dent->inode = inum;
	strcpy(fs_dent->name, ".");

	/* copy the path data */
	fs_dent->path = dinfo->dirs;
	fs_dent->pathdepth = dinfo->depth;

	/* this is probably a waste, but just in case the action mucks
	 * with it ...
	 */
	fs_dent->fsi = fs->inode_lookup(fs, fs_dent->inode);
	if (fs_dent->fsi != NULL) {
	    fs_dent->ent_type = FS_DENT_DIR;

	    retval = action(fs, fs_dent, myflags, ptr);
	    if (retval == WALK_STOP) {
		fs_dent_free(fs_dent);
		fs_inode_free(fs_inode);
		return 0;
	    }
	    else if (retval == WALK_ERROR) {
		fs_dent_free(fs_dent);
		fs_inode_free(fs_inode);
		return 1;
	    }
	}
	else {
	    if (verbose)
		fprintf(stderr, "Error reading . entry: %" PRIuINUM,
		    fs_dent->inode);
	}



	/*
	 * ".."
	 */
	strcpy(fs_dent->name, "..");
	fs_dent->ent_type = FS_DENT_DIR;

	/* The fs_name structure holds the parent inode value, so we 
	 * just cycle using those
	 */
	for (fs_name = fs_inode->name; fs_name != NULL;
	    fs_name = fs_name->next) {
	    if (fs_dent->fsi) {
		fs_inode_free(fs_dent->fsi);
		fs_dent->fsi = NULL;
	    }

	    fs_dent->inode = fs_name->par_inode;
	    fs_dent->fsi = fs->inode_lookup(fs, fs_dent->inode);
	    if (fs_dent->fsi) {
		retval = action(fs, fs_dent, myflags, ptr);
		if (retval == WALK_STOP) {
		    fs_dent_free(fs_dent);
		    fs_inode_free(fs_inode);
		    return 0;
		}
		else if (retval == WALK_ERROR) {
		    fs_dent_free(fs_dent);
		    fs_inode_free(fs_inode);
		    return 1;
		}
	    }
	    else {
		if (verbose)
		    fprintf(stderr,
			"dent_walk: Error reading .. inode: %"
			PRIuINUM, fs_dent->inode);
	    }

	}

	fs_dent_free(fs_dent);
	fs_dent = NULL;
    }



    /* Now we return to processing the Index Root Attribute */
    retval = ntfs_dent_idxentry(ntfs, dinfo, idxe,
	getu32(fs,
	    idxelist->buf_off) -
	getu32(fs, idxelist->begin_off),
	getu32(fs,
	    idxelist->end_off) -
	getu32(fs, idxelist->begin_off), flags, action, ptr);

    if (retval != 0) {
	fs_inode_free(fs_inode);
	return (retval == -1) ? 1 : 0;
    }

    /* 
     * get the index allocation attribute if it exists (it doesn't for 
     * small directories 
     */
    fs_data_alloc =
	fs_data_lookup_noid(fs_inode->attr, NTFS_ATYPE_IDXALLOC);


    /* if we don't have an index alloc then return, we have processed
     * all of the entries 
     */
    if (!fs_data_alloc) {
	fs_inode_free(fs_inode);
	if (getu32(fs, idxelist->flags) & NTFS_IDXELIST_CHILD) {
	    tsk_errno = TSK_ERR_FS_INODE_INT;
	    snprintf(tsk_errstr, TSK_ERRSTR_L,
		"Error: $IDX_ROOT says there should be children, but there isn't");

	    return 1;
	}
	else {
	    return 0;
	}
    }


    if (fs_data_alloc->flags & FS_DATA_RES) {
	tsk_errno = TSK_ERR_FS_INODE_INT;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "$IDX_ALLOC is Resident - it shouldn't be");
	fs_inode_free(fs_inode);
	return 1;
    }

    /* 
     * Copy the index allocation run into a big buffer
     */

    idxalloc_len = fs_data_alloc->runlen;
    if ((idxalloc = mymalloc(idxalloc_len)) == NULL) {
	fs_inode_free(fs_inode);
	return 1;
    }

    /* Fill in the loading data structure */
    load_file.total = load_file.left = idxalloc_len;
    load_file.cur = load_file.base = idxalloc;

    if (verbose)
	fprintf(stderr,
	    "ntfs_dent_walk: Copying $IDX_ALLOC into buffer\n");

    if (ntfs_data_walk(ntfs, fs_inode->addr, fs_data_alloc,
	    FS_FLAG_FILE_SLACK, load_file_action, (void *) &load_file)) {
	free(idxalloc);
	fs_inode_free(fs_inode);
	strncat(tsk_errstr2, " - ntfs_dent_walk",
	    TSK_ERRSTR_L - strlen(tsk_errstr2));
	return 1;
    }

    /* Not all of the directory was copied, so we exit */
    if (load_file.left > 0) {
	free(idxalloc);
	fs_inode_free(fs_inode);

	tsk_errno = TSK_ERR_FS_FWALK;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "Error reading directory contents: %" PRIuINUM "\n", inum);
	return 1;
    }

    /*
     * The idxalloc is a big buffer that contains one or more
     * idx buffer structures.  Each idxrec is a node in the B-Tree.  
     * We do not process the tree as a tree because then we could
     * not find the deleted file names.
     *
     * Therefore, we scan the big buffer looking for the index record
     * structures.  We save a pointer to the known beginning (idxrec_p).
     * Then we scan for the beginning of the next one (idxrec) and process
     * everything in the middle as an ntfs_idxrec.  We can't use the
     * size given because then we wouldn't see the deleted names
     */

    /* Set the previous pointer to NULL */
    idxrec_p = idxrec = NULL;

    /* Loop by cluster size */
    for (off = 0; off < idxalloc_len; off += ntfs->csize_b) {
	uint32_t list_len, rec_len;

	idxrec = (ntfs_idxrec *) & idxalloc[off];

	if (verbose)
	    fprintf(stderr,
		"ntfs_dent_walk: Index Buffer Offset: %d  Magic: %"
		PRIx32 "\n", off, getu32(fs, idxrec->magic));

	/* Is this the begining of an index record? */
	if (getu32(fs, idxrec->magic) != NTFS_IDXREC_MAGIC)
	    continue;


	/* idxrec_p is only NULL for the first time 
	 * Set it and start again to find the next one */
	if (idxrec_p == NULL) {
	    idxrec_p = idxrec;
	    continue;
	}

	/* Process the previous structure */

	/* idxrec points to the next idxrec structure, idxrec_p
	 * points to the one we are going to process
	 */
	rec_len = ((uintptr_t) idxrec - (uintptr_t) idxrec_p);

	if (verbose)
	    fprintf(stderr,
		"ntfs_dent_walk: Processing previous index record (len: %"
		PRIu32 ")\n", rec_len);

	/* remove the update sequence in the index record */
	if (ntfs_fix_idxrec(ntfs, idxrec_p, rec_len)) {
	    free(idxalloc);
	    fs_inode_free(fs_inode);
	    return 1;
	}

	/* Locate the start of the index entry list */
	idxelist = &idxrec_p->list;
	idxe = (ntfs_idxentry *) ((uintptr_t) idxelist +
	    getu32(fs, idxelist->begin_off));

	/* the length from the start of the next record to where our
	 * list starts.
	 * This should be the same as buf_off in idxelist, but we don't
	 * trust it.
	 */
	list_len = (uintptr_t) idxrec - (uintptr_t) idxe;

	/* process the list of index entries */
	retval = ntfs_dent_idxentry(ntfs, dinfo, idxe, list_len,
	    getu32(fs,
		idxelist->end_off) -
	    getu32(fs, idxelist->begin_off), flags, action, ptr);
	if (retval != 0) {
	    fs_inode_free(fs_inode);
	    free(idxalloc);
	    return (retval == -1) ? 1 : 0;
	}

	/* reset the pointer to the next record */
	idxrec_p = idxrec;

    }				/* end of cluster loop */


    /* Process the final record */
    if (idxrec_p) {
	uint32_t list_len, rec_len;

	/* Length from end of attribute to start of this */
	rec_len =
	    idxalloc_len - ((uintptr_t) idxrec_p - (uintptr_t) idxalloc);

	if (verbose)
	    fprintf(stderr,
		"ntfs_dent_walk: Processing final index record (len: %"
		PRIu32 ")\n", rec_len);

	/* remove the update sequence */
	if (ntfs_fix_idxrec(ntfs, idxrec_p, rec_len)) {
	    fs_inode_free(fs_inode);
	    free(idxalloc);
	    return 1;
	}

	idxelist = &idxrec_p->list;
	idxe = (ntfs_idxentry *) ((uintptr_t) idxelist +
	    getu32(fs, idxelist->begin_off));

	/* This is the length of the idx entries */
	list_len =
	    ((uintptr_t) idxalloc + idxalloc_len) - (uintptr_t) idxe;

	/* process the list of index entries */
	retval = ntfs_dent_idxentry(ntfs, dinfo, idxe, list_len,
	    getu32(fs,
		idxelist->end_off) -
	    getu32(fs, idxelist->begin_off), flags, action, ptr);
	if (retval != 0) {
	    fs_inode_free(fs_inode);
	    free(idxalloc);
	    return (retval == -1) ? 1 : 0;
	}
    }

    fs_inode_free(fs_inode);
    free(idxalloc);

    return 0;
}				/* end of dent_walk */




/****************************************************************************
 * FIND_FILE ROUTINES
 *
 */


/* 
 * Looks up the parent inode described in fs_name.  
 * myflags are the flags from the original inode lookup
 *
 * fs_dent was filled in by ntfs_find_file and will get the final path
 * added to it before action is called
 *
 * return 1 on error and 0 on success
 */
static uint8_t
ntfs_find_file_rec(FS_INFO * fs, NTFS_DINFO * dinfo, FS_DENT * fs_dent,
    FS_NAME * fs_name, int myflags, int flags,
    FS_DENT_WALK_FN action, void *ptr)
{
    FS_INODE *fs_inode_par;
    FS_NAME *fs_name_par;
    uint8_t decrem = 0;
    int len = 0, i;
    char *begin = NULL;
    int retval;


    if (fs_name->par_inode < fs->first_inum ||
	fs_name->par_inode > fs->last_inum) {
	tsk_errno = TSK_ERR_FS_ARG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "invalid inode value: %" PRIuINUM "\n", fs_name->par_inode);
	return 1;
    }

    fs_inode_par = fs->inode_lookup(fs, fs_name->par_inode);
    if (fs_inode_par == NULL) {
	strncat(tsk_errstr2, " - ntfs_find_file_rec",
	    TSK_ERRSTR_L - strlen(tsk_errstr2));
	return 1;
    }

    /* 
     * Orphan File
     * This occurs when the file is deleted and either:
     * - The parent is no longer a directory 
     * - The sequence number of the parent is no longer correct
     */
    if (((fs_inode_par->mode & FS_INODE_FMT) != FS_INODE_DIR) ||
	(fs_inode_par->seq != fs_name->par_seq)) {
	char *str = ORPHAN_STR;
	len = strlen(str);

	/* @@@ There should be a sanity check here to verify that the 
	 * previous name was unallocated ... but how do I get it again?
	 */
	if ((((uintptr_t) dinfo->didx[dinfo->depth - 1] - len) >=
		(uintptr_t) & dinfo->dirs[0])
	    && (dinfo->depth < MAX_DEPTH)) {
	    begin = dinfo->didx[dinfo->depth] =
		(char *) ((uintptr_t) dinfo->didx[dinfo->depth - 1] - len);

	    dinfo->depth++;
	    decrem = 1;

	    for (i = 0; i < len; i++)
		begin[i] = str[i];
	}

	fs_dent->path = begin;
	fs_dent->pathdepth = dinfo->depth;
	retval = action(fs, fs_dent, myflags, ptr);

	if (decrem)
	    dinfo->depth--;

	fs_inode_free(fs_inode_par);
	return (retval == WALK_ERROR) ? 1 : 0;
    }

    for (fs_name_par = fs_inode_par->name; fs_name_par != NULL;
	fs_name_par = fs_name_par->next) {

	len = strlen(fs_name_par->name);

	/* do some length checks on the dir structure 
	 * if we can't fit it then forget about it */
	if ((((uintptr_t) dinfo->didx[dinfo->depth - 1] - len - 1) >=
		(uintptr_t) & dinfo->dirs[0])
	    && (dinfo->depth < MAX_DEPTH)) {
	    begin = dinfo->didx[dinfo->depth] =
		(char *) ((uintptr_t) dinfo->didx[dinfo->depth - 1] - len -
		1);

	    dinfo->depth++;
	    decrem = 1;

	    *begin = '/';
	    for (i = 0; i < len; i++)
		begin[i + 1] = fs_name_par->name[i];
	}
	else {
	    begin = dinfo->didx[dinfo->depth];
	    decrem = 0;
	}


	/* if we are at the root, then fill out the rest of fs_dent with
	 * the full path and call the action 
	 */
	if (fs_name_par->par_inode == NTFS_ROOTINO) {
	    /* increase the path by one so that we do not pass the '/'
	     * if we do then the printed result will have '//' at 
	     * the beginning
	     */
	    fs_dent->path = (char *) ((uintptr_t) begin + 1);
	    fs_dent->pathdepth = dinfo->depth;
	    if (WALK_ERROR == action(fs, fs_dent, myflags, ptr)) {
		fs_inode_free(fs_inode_par);
		return 1;
	    }
	}

	/* otherwise, recurse some more */
	else {
	    if (ntfs_find_file_rec(fs, dinfo, fs_dent, fs_name_par,
		    myflags, flags, action, ptr)) {
		fs_inode_free(fs_inode_par);
		return 1;
	    }
	}

	/* if we incremented before, then decrement the depth now */
	if (decrem)
	    dinfo->depth--;
    }

    fs_inode_free(fs_inode_par);
    return 0;
}

/* 
 * this is a much faster way of doing it in NTFS 
 *
 * the inode that is passed in this case is the one to find the name
 * for
 *
 * This can not be called with dent_walk because the path
 * structure will get messed up!
 */

uint8_t
ntfs_find_file(FS_INFO * fs, INUM_T inode_toid, uint32_t type_toid,
    uint16_t id_toid, int flags, FS_DENT_WALK_FN action, void *ptr)
{
    FS_NAME *fs_name;
    FS_DENT *fs_dent;
    int myflags;
    NTFS_INFO *ntfs = (NTFS_INFO *) fs;
    char *attr = NULL;
    NTFS_DINFO dinfo;


    /* sanity check */
    if (inode_toid < fs->first_inum || inode_toid > fs->last_inum) {
	tsk_errno = TSK_ERR_FS_ARG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "ntfs_find_file: invalid inode value: %" PRIuINUM "\n",
	    inode_toid);
	return 1;
    }

    if ((fs_dent = fs_dent_alloc(NTFS_MAXNAMLEN_UTF8, 0)) == NULL) {
	return 1;
    }

    memset(&dinfo, 0, sizeof(NTFS_DINFO));

    /* in this function, we use the dinfo->dirs array in the opposite order.
     * we set the end of it to NULL and then prepend the
     * directories to it
     *
     * dinfo->didx[dinfo->depth] will point to where the current level started their
     * dir name
     */
    dinfo.dirs[DIR_STRSZ - 2] = '/';
    dinfo.dirs[DIR_STRSZ - 1] = '\0';
    dinfo.didx[0] = &dinfo.dirs[DIR_STRSZ - 2];
    dinfo.depth = 1;


    /* lookup the inode and get its allocation status */
    fs_dent->inode = inode_toid;
    fs_dent->fsi = fs->inode_lookup(fs, inode_toid);
    if (fs_dent->fsi == NULL) {
	strncat(tsk_errstr2, " - ntfs_find_file",
	    TSK_ERRSTR_L - strlen(tsk_errstr2));
	fs_dent_free(fs_dent);
	return 1;
    }
    myflags = ((getu16(fs, ntfs->mft->flags) & NTFS_MFT_INUSE) ?
	FS_FLAG_NAME_ALLOC : FS_FLAG_NAME_UNALLOC);

    /* Get the name for the attribute - if specified */
    if (type_toid != 0) {
	FS_DATA *fs_data =
	    fs_data_lookup(fs_dent->fsi->attr, type_toid, id_toid);
	if (!fs_data) {
	    tsk_errno = TSK_ERR_FS_INODE_INT;
	    snprintf(tsk_errstr, TSK_ERRSTR_L,
		"find_file: Type %" PRIu32 " Id %" PRIu16
		" not found in MFT %" PRIuINUM "", type_toid, id_toid,
		inode_toid);
	    fs_dent_free(fs_dent);
	    return 1;
	}

	/* only add the attribute name if it is the non-default data stream */
	if (strcmp(fs_data->name, "$Data") != 0)
	    attr = fs_data->name;
    }

    /* loop through all the names it may have */
    for (fs_name = fs_dent->fsi->name; fs_name != NULL;
	fs_name = fs_name->next) {
	int retval;

	/* Append on the attribute name, if it exists */
	if (attr != NULL) {
	    snprintf(fs_dent->name, fs_dent->name_max, "%s:%s",
		fs_name->name, attr);
	}
	else {
	    strncpy(fs_dent->name, fs_name->name, fs_dent->name_max);
	}

	/* if this is in the root directory, then call back */
	if (fs_name->par_inode == NTFS_ROOTINO) {
	    fs_dent->path = dinfo.didx[0];
	    fs_dent->pathdepth = dinfo.depth;
	    retval = action(fs, fs_dent, myflags, ptr);
	    if (retval == WALK_STOP) {
		fs_dent_free(fs_dent);
		return 0;
	    }
	    else if (retval == WALK_ERROR) {
		fs_dent_free(fs_dent);
		return 1;
	    }
	}
	/* call the recursive function on the parent */
	else {
	    if (ntfs_find_file_rec(fs, &dinfo, fs_dent, fs_name, myflags,
		    flags, action, ptr)) {
		fs_dent_free(fs_dent);
		return 1;
	    }
	}
    }				/* end of name loop */

    fs_dent_free(fs_dent);
    return 0;
}
