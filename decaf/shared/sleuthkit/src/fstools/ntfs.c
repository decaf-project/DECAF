/*
** ntfs
** The Sleuth Kit 
**
** $Date: 2006-09-07 13:14:51 -0400 (Thu, 07 Sep 2006) $
**
** Content and meta data layer support for the NTFS file system
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

#include <ctype.h>

/*
 * NOTES TO SELF:
 *
 * - multiple ".." entries may exist
 */

/* 
 * How are we to handle the META flag? Is the MFT $Data Attribute META?
 */


/* needs to be predefined for proc_attrseq */
static uint8_t ntfs_proc_attrlist(NTFS_INFO *, FS_INODE *, FS_DATA *);



/* mini-design note:
 * The MFT has entries for every file and dir in the fs.
 * The first entry ($MFT) is for the MFT itself and it is used to find
 * the location of the entire table because it can become fragmented.
 * Therefore, the $Data attribute of $MFT is saved in the NTFS_INFO
 * structure for easy access.  We also use the size of the MFT as
 * a way to calculate the maximum MFT entry number (last_inum).
 *
 * Ok, that is simple, but getting the full $Data attribute can be tough
 * because $MFT may not fit into one MFT entry (i.e. an attribute list). 
 * We need to process the attribute list attribute to find out which
 * other entries to process.  But, the attribute list attribute comes
 * before any $Data attribute (so it could refer to an MFT that has not
 * yet been 'defined').  Although, the $Data attribute seems to always 
 * exist and define at least the run for the entry in the attribute list.
 *
 * So, the way this is solved is that generic mft_lookup is used to get
 * any MFT entry, even $MFT.  If $MFT is not cached then we calculate 
 * the address of where to read based on mutliplication and guessing.  
 * When we are loading the $MFT, we set 'loading_the_MFT' to 1 so
 * that we can update things as we go along.  When we read $MFT we
 * read all the attributes and save info about the $Data one.  If
 * there is an attribute list, we will have the location of the
 * additional MFT in the cached $Data location, which will be 
 * updated as we process the attribute list.  After each MFT
 * entry that we process while loading the MFT, the 'final_inum'
 * value is updated to reflect what we can currently load so 
 * that the sanity checks still work.
 */


/**********************************************************************
 *
 *  MISC FUNCS
 *
 **********************************************************************/

/* convert the NT Time (UTC hundred nanoseconds from 1/1/1601)
 * to UNIX (UTC seconds from 1/1/1970)
 *
 * The basic calculation is to remove the nanoseconds and then
 * subtract the number of seconds between 1601 and 1970
 * i.e. TIME - DELTA
 *
 */
uint32_t
nt2unixtime(uint64_t ntdate)
{
// (369*365 + 89) * 24 * 3600 * 10000000
#define	NSEC_BTWN_1601_1970	(uint64_t)(116444736000000000ULL)

    ntdate -= (uint64_t) NSEC_BTWN_1601_1970;
    ntdate /= (uint64_t) 10000000;

    return (uint32_t) ntdate;
}



/**********************************************************************
 *
 * Lookup Functions
 *
 **********************************************************************/




/*
 * Lookup up a given MFT entry (mftnum) in the MFT and save it in its
 * raw format in *mft.
 *
 * NOTE: This will remove the update sequence integrity checks in the
 * structure
 *
 * Return 0 on success and 1 on error
 */
static uint8_t
ntfs_dinode_lookup(NTFS_INFO * ntfs, ntfs_mft * mft, INUM_T mftnum)
{
    OFF_T mftaddr_b, mftaddr2_b, offset;
    unsigned int mftaddr_len = 0;
    int i;
    FS_INFO *fs = (FS_INFO *) & ntfs->fs_info;
    FS_DATA_RUN *data_run;
    ntfs_upd *upd;
    uint16_t sig_seq;

    /* sanity checks */
    if (!mft) {
	tsk_errno = TSK_ERR_FS_ARG;
	tsk_errstr2[0] = '\0';
	snprintf(tsk_errstr, TSK_ERRSTR_L, "mft_lookup: null mft buffer");
	return 1;
    }

    if (mftnum < fs->first_inum) {
	tsk_errno = TSK_ERR_FS_ARG;
	tsk_errstr2[0] = '\0';
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "mft_lookup: inode number is too small (%" PRIuINUM ")",
	    mftnum);
	return 1;
    }
    if (mftnum > fs->last_inum) {
	tsk_errno = TSK_ERR_FS_ARG;
	tsk_errstr2[0] = '\0';
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "mft_lookup: inode number is too large (%" PRIuINUM ")",
	    mftnum);
	return 1;
    }


    if (verbose)
	fprintf(stderr,
	    "ntfs_dinode_lookup: Processing MFT %" PRIuINUM "\n", mftnum);

    /* If mft_data (the cached $Data attribute of $MFT) is not there yet, 
     * then we have not started to load $MFT yet.  In that case, we will
     * 'cheat' and calculate where it goes.  This should only be for
     * $MFT itself, in which case the calculation is easy
     */
    if (!ntfs->mft_data) {

	/* This is just a random check with the assumption being that
	 * we don't want to just do a guess calculation for a very large
	 * MFT entry
	 */
	if (mftnum > NTFS_LAST_DEFAULT_INO) {
	    tsk_errno = TSK_ERR_FS_ARG;
	    tsk_errstr2[0] = '\0';
	    snprintf(tsk_errstr, TSK_ERRSTR_L,
		"Error trying to load a high MFT entry when the MFT itself has not been loaded (%"
		PRIuINUM ")", mftnum);
	    return 1;
	}

	mftaddr_b = ntfs->root_mft_addr + mftnum * ntfs->mft_rsize_b;
	mftaddr2_b = 0;
    }
    else {
	/* The MFT may not be in consecutive clusters, so we need to use its
	 * data attribute run list to find out what address to read
	 *
	 * This is why we cached it
	 */

	// will be set to the address of the MFT entry
	mftaddr_b = mftaddr2_b = 0;

	/* The byte offset within the $Data stream */
	offset = mftnum * ntfs->mft_rsize_b;

	/* NOTE: data_run values are in clusters 
	 *
	 * cycle through the runs in $Data and identify which
	 * has the MFT entry that we want
	 */
	for (data_run = ntfs->mft_data->run;
	    data_run != NULL; data_run = data_run->next) {

	    /* The length of this specific run */
	    OFF_T run_len = data_run->len * ntfs->csize_b;

	    /* Is our MFT entry is in this run somewhere ? */
	    if (offset < run_len) {

		if (verbose)
		    fprintf(stderr,
			"ntfs_dinode_lookup: Found in offset: %"
			PRIuDADDR "  size: %" PRIuDADDR " at offset: %"
			PRIuOFF "\n", data_run->addr, data_run->len,
			offset);

		/* special case where the MFT entry crosses
		 * a run (only happens when cluster size is 512-bytes
		 * and there are an odd number of clusters in the run)
		 */
		if (run_len < offset + ntfs->mft_rsize_b) {

		    if (verbose)
			fprintf(stderr,
			    "ntfs_dinode_lookup: Entry crosses run border\n");

		    if (data_run->next == NULL) {
			tsk_errno = TSK_ERR_FS_INODE_INT;
			tsk_errstr2[0] = '\0';
			snprintf(tsk_errstr, TSK_ERRSTR_L,
			    "mft_lookup: MFT entry crosses a cluster and there are no more clusters!");
			return 1;
		    }

		    /* Assign address where the remainder of the entry is */
		    mftaddr2_b = data_run->next->addr * ntfs->csize_b;
		    /* this should always be 512, but just in case */
		    mftaddr_len = run_len - offset;
		}

		/* Assign address of where the MFT entry starts */
		mftaddr_b = data_run->addr * ntfs->csize_b + offset;
		if (verbose)
		    fprintf(stderr,
			"ntfs_dinode_lookup: Entry address at: %"
			PRIuOFF "\n", mftaddr_b);
		break;
	    }

	    /* decrement the offset we are looking for */
	    offset -= run_len;
	}

	/* Did we find it? */
	if (!mftaddr_b) {
	    tsk_errno = TSK_ERR_FS_INODE_NUM;
	    tsk_errstr2[0] = '\0';
	    snprintf(tsk_errstr, TSK_ERRSTR_L,
		"mft_lookup: Error finding MFT entry %"
		PRIuINUM " in $MFT", mftnum);
	    return 1;
	}
    }


    /* can we do just one read or do we need multiple? */
    if (mftaddr2_b) {
	SSIZE_T cnt;
	/* read the first part into mft */
	cnt =
	    fs_read_random(&ntfs->fs_info, (char *) mft,
	    mftaddr_len, mftaddr_b);
	if (cnt != mftaddr_len) {
	    if (cnt != 1) {
		tsk_errno = TSK_ERR_FS_READ;
		tsk_errstr[0] = '\0';
	    }
	    snprintf(tsk_errstr2, TSK_ERRSTR_L,
		"ntfs_dinode_lookup: Error reading MFT Entry (part 1) at %"
		PRIuOFF, mftaddr_b);
	    return 1;
	}

	/* read the second part into mft */
	cnt = fs_read_random
	    (&ntfs->fs_info,
	    (char *) ((uintptr_t) mft + mftaddr_len),
	    ntfs->mft_rsize_b - mftaddr_len, mftaddr2_b);
	if (cnt != ntfs->mft_rsize_b - mftaddr_len) {
	    if (cnt != 1) {
		tsk_errno = TSK_ERR_FS_READ;
		tsk_errstr[0] = '\0';
	    }
	    snprintf(tsk_errstr2, TSK_ERRSTR_L,
		"ntfs_dinode_lookup: Error reading MFT Entry (part 2) at %"
		PRIuOFF, mftaddr2_b);
	    return 1;
	}
    }
    else {
	SSIZE_T cnt;
	/* read the raw entry into mft */
	cnt =
	    fs_read_random(&ntfs->fs_info, (char *) mft,
	    ntfs->mft_rsize_b, mftaddr_b);
	if (cnt != ntfs->mft_rsize_b) {
	    if (cnt != 1) {
		tsk_errno = TSK_ERR_FS_READ;
		tsk_errstr[0] = '\0';
	    }
	    snprintf(tsk_errstr2, TSK_ERRSTR_L,
		"ntfs_dinode_lookup: Error reading MFT Entry at %"
		PRIuOFF, mftaddr_b);
	    return 1;
	}
    }

    /* if we are saving into the NTFS_INFO structure, assign mnum too */
    if ((uintptr_t) mft == (uintptr_t) ntfs->mft)
	ntfs->mnum = mftnum;
    /* Sanity Check */
#if 0
    /* This is no longer applied because it caused too many problems
     * with images that had 0 and 1 etc. as values.  Testing shows that
     * even Windows XP doesn't care if entries have an invalid entry, so
     * this is no longer checked.  The update sequence check should find
     * corrupt entries
     * */
    if ((getu32(fs, mft->magic) != NTFS_MFT_MAGIC)
	&& (getu32(fs, mft->magic) != NTFS_MFT_MAGIC_BAAD)
	&& (getu32(fs, mft->magic) != NTFS_MFT_MAGIC_ZERO)) {
	tsk_errno = TSK_ERR_FS_INODE_INT;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "entry %d has an invalid MFT magic: %x",
	    mftnum, getu32(fs, mft->magic));
	return 1;
    }
#endif
    /* The MFT entries have error and integrity checks in them
     * called update sequences.  They must be checked and removed
     * so that later functions can process the data as normal. 
     * They are located in the last 2 bytes of each 512-byte sector
     *
     * We first verify that the the 2-byte value is a give value and
     * then replace it with what should be there
     */
    /* sanity check so we don't run over in the next loop */
    if ((getu16(fs, mft->upd_cnt) - 1) * ntfs->ssize_b > ntfs->mft_rsize_b) {
	tsk_errno = TSK_ERR_FS_GENFS;
	tsk_errstr2[0] = '\0';
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "More Update Sequence Entries than MFT size");
	return 1;
    }

    /* Apply the update sequence structure template */
    upd = (ntfs_upd *) ((uintptr_t) mft + getu16(fs, mft->upd_off));
    /* Get the sequence value that each 16-bit value should be */
    sig_seq = getu16(fs, upd->upd_val);
    /* cycle through each sector */
    for (i = 1; i < getu16(fs, mft->upd_cnt); i++) {
	uint8_t *new_val, *old_val;
	/* The offset into the buffer of the value to analyze */
	size_t offset = i * ntfs->ssize_b - 2;
	/* get the current sequence value */
	uint16_t cur_seq = getu16(fs, (uintptr_t) mft + offset);
	if (cur_seq != sig_seq) {
	    /* get the replacement value */
	    uint16_t cur_repl = getu16(fs, &upd->upd_seq + (i - 1) * 2);
	    tsk_errno = TSK_ERR_FS_GENFS;
	    tsk_errstr2[0] = '\0';
	    snprintf(tsk_errstr, TSK_ERRSTR_L,
		"Incorrect update sequence value in MFT entry\nSignature Value: 0x%"
		PRIx16 " Actual Value: 0x%" PRIx16
		" Replacement Value: 0x%" PRIx16
		"\nThis is typically because of a corrupted entry",
		sig_seq, cur_seq, cur_repl);
	    return 1;
	}

	new_val = &upd->upd_seq + (i - 1) * 2;
	old_val = (uint8_t *) ((uintptr_t) mft + offset);
	if (verbose)
	    fprintf(stderr,
		"ntfs_dinode_lookup: upd_seq %i   Replacing: %.4"
		PRIx16 "   With: %.4" PRIx16 "\n", i,
		getu16(fs, old_val), getu16(fs, new_val));
	*old_val++ = *new_val++;
	*old_val = *new_val;
    }

    return 0;
}



/*
 * given a cluster, return the allocation status or
 * -1 if an error occurs
 */
static int
is_clustalloc(NTFS_INFO * ntfs, DADDR_T addr)
{
    int bits_p_clust, b;
    DADDR_T base;
    bits_p_clust = 8 * ntfs->fs_info.block_size;

    /* While we are loading the MFT, assume that everything
     * is allocated.  This should only be needed when we are
     * dealing with an attribute list ... 
     */
    if (ntfs->loading_the_MFT == 1) {
	return 1;
    }
    else if (ntfs->bmap == NULL) {
	tsk_errno = TSK_ERR_FS_ARG;
	tsk_errstr2[0] = '\0';
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "is_clustalloc: Bitmap pointer is null: %" PRIuDADDR
	    "\n", addr);
	return -1;
    }

    /* Is the cluster too big? */
    if (addr > ntfs->fs_info.last_block) {
	tsk_errno = TSK_ERR_FS_INODE_INT;
	tsk_errstr2[0] = '\0';
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "is_clustalloc: cluster too large");
	return -1;
    }

    /* identify the base cluster in the bitmap file */
    base = addr / bits_p_clust;
    b = addr % bits_p_clust;

    /* is this the same as in the cached buffer? */
    if (base != ntfs->bmap_buf_off) {
	DADDR_T c = base;
	FS_DATA_RUN *run;
	DADDR_T fsaddr = 0;
	SSIZE_T cnt;

	/* get the file system address of the bitmap cluster */
	for (run = ntfs->bmap; run; run = run->next) {
	    if (run->len <= c) {
		c -= run->len;
	    }
	    else {
		fsaddr = run->addr + c;
		break;
	    }
	}

	if (fsaddr == 0) {
	    tsk_errno = TSK_ERR_FS_BLK_NUM;
	    tsk_errstr2[0] = '\0';
	    snprintf(tsk_errstr, TSK_ERRSTR_L,
		"is_clustalloc: cluster not found in bitmap: %"
		PRIuDADDR "", c);
	    return -1;
	}
	if (fsaddr > ntfs->fs_info.last_block) {
	    tsk_errno = TSK_ERR_FS_BLK_NUM;
	    snprintf(tsk_errstr, TSK_ERRSTR_L,
		"is_clustalloc: Cluster in bitmap too large for image: %"
		PRIuDADDR, fsaddr);
	    return -1;
	}
	ntfs->bmap_buf_off = base;
	cnt = fs_read_block
	    (&ntfs->fs_info, ntfs->bmap_buf,
	    ntfs->fs_info.block_size, fsaddr);
	if (cnt != ntfs->fs_info.block_size) {
	    if (cnt != 1) {
		tsk_errno = TSK_ERR_FS_READ;
		tsk_errstr[0] = '\0';
	    }
	    snprintf(tsk_errstr2, TSK_ERRSTR_L,
		"is_clustalloc: Error reading bitmap at %"
		PRIuDADDR, fsaddr);
	    return -1;
	}
    }

    /* identify if the cluster is allocated or not */
    return (isset(ntfs->bmap_buf->data, b)) ? 1 : 0;
}



/**********************************************************************
 *
 *  FS_DATA functions
 *
 **********************************************************************/


/* 
 * turn a non-resident runlist into the generic fs_data_run
 * structure
 *
 * The return value is a list of FS_DATA_RUN entries of len
 * runlen bytes (only set if non-NULL)
 *
 * Returns NULL on error and for Bad clust: Check tsk_errno to test
 */
static FS_DATA_RUN *
ntfs_make_data_run(NTFS_INFO * ntfs,
    ntfs_runlist * runlist, OFF_T * runlen)
{
    ntfs_runlist *run;
    FS_DATA_RUN *data_run, *data_run_head = NULL, *data_run_prev = NULL;
    unsigned int i, idx;
    DADDR_T prev_addr = 0;
    run = runlist;

    /* initialize if non-NULL */
    if (runlen)
	*runlen = 0;

    /* Cycle through each run in the runlist 
     * We go until we find an entry with no length
     * An entry with offset of 0 is for a sparse run
     */
    while (NTFS_RUNL_LENSZ(run) != 0) {
	int64_t offset = 0;

	/* allocate a new fs_data_run */
	if ((data_run = fs_data_run_alloc()) == NULL) {
	    return NULL;
	}

	/* make the list, unless its the first pass & then we set the head */
	if (data_run_prev)
	    data_run_prev->next = data_run;
	else
	    data_run_head = data_run;
	data_run_prev = data_run;

	/* These fields are a variable number of bytes long
	 * these for loops are the equivalent of the getuX macros
	 */
	idx = 0;
	/* Get the length of this run */
	for (i = 0, data_run->len = 0; i < NTFS_RUNL_LENSZ(run); i++) {
	    data_run->len |= (run->buf[idx++] << (i * 8));
	    if (verbose)
		fprintf(stderr,
		    "ntfs_make_data_run: Len idx: %i cur: %"
		    PRIu8 " (%" PRIx8 ") tot: %" PRIuDADDR
		    " (%" PRIxDADDR ")\n", i,
		    run->buf[idx - 1], run->buf[idx - 1],
		    data_run->len, data_run->len);
	}

	/* Update the length if we were passed a value */
	if (runlen)
	    *runlen += (data_run->len * ntfs->csize_b);

	/* Get the address of this run */
	for (i = 0, data_run->addr = 0; i < NTFS_RUNL_OFFSZ(run); i++) {
	    //data_run->addr |= (run->buf[idx++] << (i * 8));
	    offset |= (run->buf[idx++] << (i * 8));
	    if (verbose)
		fprintf(stderr,
		    "ntfs_make_data_run: Off idx: %i cur: %"
		    PRIu8 " (%" PRIx8 ") tot: %" PRIuDADDR
		    " (%" PRIxDADDR ")\n", i,
		    run->buf[idx - 1], run->buf[idx - 1], offset, offset);
	}

	/* offset value is signed so extend it to 64-bits */
	if ((int8_t) run->buf[idx - 1] < 0) {
	    for (; i < sizeof(offset); i++)
		offset |= (int64_t) ((int64_t) 0xff << (i * 8));
	}

	if (verbose)
	    fprintf(stderr,
		"ntfs_make_data_run: Signed offset: %"
		PRIdDADDR " Previous address: %"
		PRIdDADDR "\n", offset, prev_addr);

	/* The NT 4.0 version of NTFS uses an offset of -1 to represent
	 * a hole, so add the sparse flag and make it look like the 2K
	 * version with a offset of 0
	 *
	 * A user reported an issue where the $Bad file started with
	 * its offset as -1 and it was not NT (maybe a conversion)
	 * Change the check now to not limit to NT, but make sure
	 * that it is the first run
	 */
	if (((offset == -1) && (prev_addr == 0)) || ((offset == -1)
		&& (ntfs->ver == NTFS_VINFO_NT))) {
	    data_run->flags |= FS_DATA_SPARSE;
	    data_run->addr = 0;
	    if (verbose)
		fprintf(stderr, "ntfs_make_data_run: Sparse Run\n");
	}

	/* A Sparse file has a run with an offset of 0
	 * there is a special case though of the BOOT MFT entry which
	 * is the super block and has a legit offset of 0.
	 *
	 * The value given is a delta of the previous offset, so add 
	 * them for non-sparse files
	 *
	 * For sparse files the next run will have its offset relative 
	 * to the current "prev_addr" so skip that code
	 */
	else if ((offset) || (ntfs->mnum == NTFS_MFT_BOOT)) {
	    data_run->addr = prev_addr + offset;
	    prev_addr = data_run->addr;
	}
	else {
	    data_run->flags |= FS_DATA_SPARSE;
	    if (verbose)
		fprintf(stderr, "ntfs_make_data_run: Sparse Run\n");
	}


	/* Advance run */
	run = (ntfs_runlist *) ((uintptr_t) run + (1 + NTFS_RUNL_LENSZ(run)
		+ NTFS_RUNL_OFFSZ(run)));
    }

    /* special case for $BADCLUST, which is a sparse file whose size is
     * the entire file system.
     *
     * If there is only one run entry and it is sparse, then there are no
     * bad blocks, so get rid of it.
     */
    if ((data_run_head != NULL)
	&& (data_run_head->next == NULL)
	&& (data_run_head->flags & FS_DATA_SPARSE)) {
	free(data_run_head);
	data_run_head = NULL;
    }

    return data_run_head;
}



/*
 * Perform a walk on a given FS_DATA list.  The _action_ function is
 * called on each cluster of the run.  
 *
 * This gives us an interface to call an action on data and not care if
 * it is resident or not.
 *
 * used flag values: FS_FLAG_FILE_AONLY, FS_FLAG_FILE_SLACK, 
 * FS_FLAG_FILE_NOSPARSE, FS_FLAG_FILE_NOAOBRT
 *
 * Action uses: FS_FLAG_DATA_CONT
 *
 * No notion of META
 *
 * returns 1 on error and 0 on success
 */
uint8_t
ntfs_data_walk(NTFS_INFO * ntfs, INUM_T inum,
    FS_DATA * fs_data, int flags, FS_FILE_WALK_FN action, void *ptr)
{
    FS_INFO *fs = (FS_INFO *) & ntfs->fs_info;
    int myflags;
    int retval;

    if (verbose)
	fprintf(stderr,
	    "ntfs_data_walk: Processing file %" PRIuINUM "\n", inum);
    /* Process the resident buffer 
     */
    if (fs_data->flags & FS_DATA_RES) {
	char *buf = NULL;
	if ((flags & FS_FLAG_FILE_AONLY) == 0) {
	    if ((buf = mymalloc(fs_data->size)) == NULL) {
		return 1;
	    }
	    memcpy(buf, fs_data->buf, fs_data->size);
	}

	myflags =
	    FS_FLAG_DATA_CONT | FS_FLAG_DATA_ALLOC | FS_FLAG_DATA_RES;
	retval =
	    action(fs, /*ntfs->root_mft_addr*/ ntfs->root_mft_addr/fs->block_size, buf, fs_data->size, myflags,
	    ptr);

	if (retval == WALK_STOP) {
	    if ((flags & FS_FLAG_FILE_AONLY) == 0)
		free(buf);
	    return 0;
	}
	else if (retval == WALK_ERROR) {
	    if ((flags & FS_FLAG_FILE_AONLY) == 0)
		free(buf);
	    return 1;
	}
	if ((flags & FS_FLAG_FILE_AONLY) == 0)
	    free(buf);
    }
    /* non-resident */
    else {
	unsigned int a, bufsize;
	DADDR_T addr;
	DATA_BUF *data_buf = NULL;
	char *buf = NULL;
	OFF_T fsize;
	FS_DATA_RUN *fs_data_run;

	if (fs_data->flags & FS_DATA_COMP) {
	    tsk_errno = TSK_ERR_FS_FUNC;
	    snprintf(tsk_errstr, TSK_ERRSTR_L,
		"ERROR: TSK Cannot uncompress NTFS compressed files - aborting");
	    return 1;
	}

	/* if we want the slack space too, then use the runlen  */
	if (flags & FS_FLAG_FILE_SLACK)
	    fsize = fs_data->runlen;
	else
	    fsize = fs_data->size;
	if ((flags & FS_FLAG_FILE_AONLY) == 0) {
	    if ((data_buf = data_buf_alloc(fs->block_size)) == NULL) {
		return 1;
	    }
	    buf = data_buf->data;
	}

	fs_data_run = fs_data->run;
	/* cycle through the number of runs we have */
	while (fs_data_run) {

	    /* We may get a FILLER entry at the beginning of the run
	     * if we are processing a non-base file record because
	     * this $DATA attribute could not be the first in the bigger
	     * attribute. Therefore, do not error if it starts at 0
	     */
	    if (fs_data_run->flags & FS_DATA_FILLER) {
		if (fs_data_run->addr != 0) {
		    tsk_errno = TSK_ERR_FS_GENFS;
		    snprintf(tsk_errstr, TSK_ERRSTR_L,
			"Filler Entry exists in fs_data_run %"
			PRIuDADDR "@%" PRIuDADDR
			" - type: %d  id: %d", fs_data_run->len,
			fs_data_run->addr, fs_data->type, fs_data->id);
		    return 1;
		}
		else {
		    fs_data_run = fs_data_run->next;
		}
	    }

	    addr = fs_data_run->addr;
	    /* cycle through each cluster in the run */
	    for (a = 0; a < fs_data_run->len; a++) {

		/* If the address is too large then give an error */
		if (addr > fs->last_block) {
		    tsk_errno = TSK_ERR_FS_BLK_NUM;
		    snprintf(tsk_errstr, TSK_ERRSTR_L,
			"Invalid address in run (too large): %"
			PRIuDADDR "", addr);
		    return 1;
		}

		if ((flags & FS_FLAG_FILE_AONLY) == 0) {
		    SSIZE_T cnt;

		    /* sparse files just get 0s */
		    if (fs_data_run->flags & FS_DATA_SPARSE)
			memset(buf, 0, fs->block_size);

		    cnt = fs_read_block
			(fs, data_buf, fs->block_size, addr);
		    if (cnt != fs->block_size) {
			if (cnt != 1) {
			    tsk_errno = TSK_ERR_FS_READ;
			    tsk_errstr[0] = '\0';
			}
			snprintf(tsk_errstr2, TSK_ERRSTR_L,
			    "ntfs_data_walk: Error reading block at %"
			    PRIuDADDR, addr);
			return 1;
		    }
		}

		/* Do we read a full block, or just the remainder? */
		if ((OFF_T) fs->block_size < fsize)
		    bufsize = fs->block_size;
		else
		    bufsize = (int) fsize;

		myflags = FS_FLAG_DATA_CONT;
		retval = is_clustalloc(ntfs, addr);
		if (retval == -1) {
		    if ((flags & FS_FLAG_FILE_AONLY) == 0)
			data_buf_free(data_buf);
		    return 0;
		}
		else if (retval == 1) {
		    myflags |= FS_FLAG_DATA_ALLOC;
		}
		else if (retval == 0) {
		    myflags |= FS_FLAG_DATA_UNALLOC;
		}

		/* Only do sparse clusters if NOSPARSE is not set */
		if ((fs_data_run->flags & FS_DATA_SPARSE) &&
		    (0 == (flags & FS_FLAG_FILE_NOSPARSE))) {

		    retval = action(fs, addr, buf, bufsize, myflags, ptr);
		    if (retval == WALK_STOP) {
			if ((flags & FS_FLAG_FILE_AONLY) == 0)
			    data_buf_free(data_buf);
			return 0;
		    }
		    else if (retval == WALK_ERROR) {
			if ((flags & FS_FLAG_FILE_AONLY) == 0)
			    data_buf_free(data_buf);
			return 1;
		    }
		}

		else if ((fs_data_run->flags & FS_DATA_SPARSE) == 0) {
		    int retval;

		    retval = action(fs, addr, buf, bufsize, myflags, ptr);
		    if (retval == WALK_STOP) {
			if ((flags & FS_FLAG_FILE_AONLY) == 0)
			    data_buf_free(data_buf);
			return 0;
		    }
		    else if (retval == WALK_ERROR) {
			if ((flags & FS_FLAG_FILE_AONLY) == 0)
			    data_buf_free(data_buf);
			return 1;
		    }
		}

		if ((OFF_T) fs->block_size >= fsize)
		    break;

		fsize -= (OFF_T) fs->block_size;
		/* If it is a sparse run, don't increment the addr so that
		 * it always reads 0
		 */
		if ((fs_data_run->flags & FS_DATA_SPARSE) == 0)
		    addr++;
	    }

	    /* advance to the next run */
	    fs_data_run = fs_data_run->next;
	}

	if ((flags & FS_FLAG_FILE_AONLY) == 0)
	    data_buf_free(data_buf);
    }				/* end of non-res */

    return 0;
}



/* 
 * An attribute sequence is a linked list of the attributes in an MFT entry
 * 
 * This function takes a pointer to the beginning of the sequence,
 * examines each attribute and adds the data to the appropriate fields
 * of fs_inode
 *
 * len is the length of the attrseq buffer
 *
 * This is called by copy_inode and proc_attrlist
 *
 * return 1 on eror and 0 on success
 */
static uint8_t
ntfs_proc_attrseq(NTFS_INFO * ntfs,
    FS_INODE * fs_inode, ntfs_attr * attrseq, size_t len)
{
    ntfs_attr *attr = attrseq;
    FS_DATA *fs_data_attrl = NULL, *fs_data = NULL;
    char name[NTFS_MAXNAMLEN_UTF8 + 1];
    OFF_T runlen;
    FS_INFO *fs = (FS_INFO *) & ntfs->fs_info;

    if (verbose)
	fprintf(stderr,
	    "ntfs_proc_attrseq: Processing entry %"
	    PRIuINUM "\n", fs_inode->addr);

    /* Cycle through the list of attributes */
    for (; ((uintptr_t) attr >= (uintptr_t) attrseq)
	&& ((uintptr_t) attr <= ((uintptr_t) attrseq + len))
	&& (getu32(fs, attr->len) > 0
	    && (getu32(fs, attr->type) !=
		0xffffffff));
	attr = (ntfs_attr *) ((uintptr_t) attr + getu32(fs, attr->len))) {

	UTF16 *name16;
	UTF8 *name8;
	int retVal;

	/* Get the type of this attribute */
	uint32_t type = getu32(fs, attr->type);

	/* Copy the name and convert it to UTF8 */
	if (attr->nlen) {

	    name16 =
		(UTF16 *) ((uintptr_t) attr + getu16(fs, attr->name_off));
	    name8 = (UTF8 *) name;
	    retVal =
		fs_UTF16toUTF8(fs, (const UTF16 **) &name16,
		(UTF16 *) ((uintptr_t) name16 +
		    attr->nlen * 2),
		&name8,
		(UTF8 *) ((uintptr_t) name8 +
		    sizeof(name)), lenientConversion);

	    if (retVal != conversionOK) {
		if (verbose)
		    fprintf(stderr,
			"ntfs_proc_attrseq: Error converting NTFS attribute name to UTF8: %d %"
			PRIuINUM, retVal, fs_inode->addr);
		*name = '\0';
	    }

	    /* Make sure it is NULL Terminated */
	    else if ((uintptr_t) name8 > (uintptr_t) name + sizeof(name))
		name[sizeof(name)] = '\0';
	    else
		*name8 = '\0';
	}
	/* Call the unnamed $Data attribute, $Data */
	else if (type == NTFS_ATYPE_DATA) {
	    strncpy(name, "$Data", NTFS_MAXNAMLEN_UTF8 + 1);
	}
	else {
	    strncpy(name, "N/A", NTFS_MAXNAMLEN_UTF8 + 1);
	}


	/* 
	 * For resident attributes, we will copy the buffer into
	 * a FS_DATA buffer, which is stored in the FS_INODE
	 * structure
	 */
	if (attr->res == NTFS_MFT_RES) {

	    if (verbose)
		fprintf(stderr,
		    "ntfs_proc_attrseq: Resident Attribute in %"
		    PRIuINUM " Type: %" PRIu32 " Id: %"
		    PRIu16 " Name: %s\n", ntfs->mnum,
		    type, getu16(fs, attr->id), name);

	    /* Add this resident stream to the fs_inode->attr list */
	    fs_inode->attr =
		fs_data_put_str(fs_inode->attr, name, type,
		getu16(fs, attr->id),
		(void *) ((uintptr_t) attr +
		    getu16(fs,
			attr->c.r.soff)), getu32(fs, attr->c.r.ssize));

	    if (fs_inode->attr == NULL) {
		strncat(tsk_errstr2, " - proc_attrseq",
		    TSK_ERRSTR_L - strlen(tsk_errstr2));
		return 1;
	    }
	}
	/* For non-resident attributes, we will copy the runlist
	 * to the generic form and then save it in the FS_INODE->attr
	 * list
	 */
	else {
	    FS_DATA_RUN *fs_data_run;
	    uint8_t data_flag = 0;
	    uint16_t id = getu16(fs, attr->id);

	    if (verbose)
		fprintf(stderr,
		    "ntfs_proc_attrseq: Non-Resident Attribute in %"
		    PRIuINUM " Type: %" PRIu32 " Id: %"
		    PRIu16 " Name: %s  Start VCN: %"
		    PRIu64 "\n", ntfs->mnum, type, id,
		    name, getu64(fs, attr->c.nr.start_vcn));

	    /* convert the run to generic form */
	    if ((fs_data_run = ntfs_make_data_run(ntfs,
			(ntfs_runlist *) ((uintptr_t)
			    attr + getu16(fs, attr->c.nr.run_off)),
			&runlen)) == NULL) {
		if (tsk_errno != 0) {
		    strncat(tsk_errstr2, " - proc_attrseq",
			TSK_ERRSTR_L - strlen(tsk_errstr2));
		    return 1;
		}
	    }

	    /* Determine the flags based on compression and stuff */
	    data_flag = 0;
	    if (getu16(fs, attr->flags) & NTFS_ATTR_FLAG_COMP)
		data_flag |= FS_DATA_COMP;

	    if (getu16(fs, attr->flags) & NTFS_ATTR_FLAG_ENC)
		data_flag |= FS_DATA_ENC;

	    if (getu16(fs, attr->flags) & NTFS_ATTR_FLAG_SPAR)
		data_flag |= FS_DATA_SPAR;

	    /* SPECIAL CASE 
	     * We are in non-res section, so we know this
	     * isn't $STD_INFO and $FNAME
	     *
	     * When we are processing a non-base entry, we may
	     * find an attribute with an id of 0 and it is an
	     * extention of a previous run (i.e. non-zero start VCN)
	     * 
	     * We will lookup if we already have such an attribute
	     * and get its ID
	     *
	     * We could also check for a start_vcn if this does
	     * not fix the problem
	     */
	    if (id == 0) {
		FS_DATA *fs_data2 = fs_inode->attr;

		while ((fs_data2)
		    && (fs_data2->flags & FS_DATA_INUSE)) {

		    /* We found an attribute with the same name and type */
		    if ((fs_data2->type == type) &&
			(strcmp(fs_data2->name, name) == 0)) {
			id = fs_data2->id;
			if (verbose)
			    fprintf(stderr,
				"ntfs_proc_attrseq: Updating id from 0 to %"
				PRIu16 "\n", id);
			break;
		    }
		    fs_data2 = fs_data2->next;
		}
	    }

	    /* Add the run to the list */
	    if ((fs_inode->attr =
		    fs_data_put_run(fs_inode->attr,
			getu64(fs,
			    attr->c.nr.start_vcn),
			runlen, fs_data_run, name,
			type, id, getu64(fs, attr->c.nr.ssize),
			data_flag)) == NULL) {

		strncat(tsk_errstr2, " - proc_attrseq: put run",
		    TSK_ERRSTR_L - strlen(tsk_errstr2));
		return 1;
	    }
	}


	/* 
	 * Special Cases, where we grab additional information
	 * regardless if they are resident or not
	 */

	/* Standard Information (is always resident) */
	if (type == NTFS_ATYPE_SI) {
	    ntfs_attr_si *si;
	    if (attr->res != NTFS_MFT_RES) {
		tsk_errno = TSK_ERR_FS_INODE_INT;
		snprintf(tsk_errstr, TSK_ERRSTR_L,
		    "proc_attrseq: Standard Information Attribute is not resident!");
		return 1;
	    }
	    si = (ntfs_attr_si *) ((uintptr_t) attr +
		getu16(fs, attr->c.r.soff));
	    fs_inode->mtime = nt2unixtime(getu64(fs, si->mtime));
	    fs_inode->atime = nt2unixtime(getu64(fs, si->atime));
	    fs_inode->ctime = nt2unixtime(getu64(fs, si->ctime));
	    fs_inode->crtime = nt2unixtime(getu64(fs, si->crtime));
	    fs_inode->uid = getu32(fs, si->own_id);
	    fs_inode->mode |= (MODE_IXUSR | MODE_IXGRP | MODE_IXOTH);
	    if ((getu32(fs, si->dos) & NTFS_SI_RO) == 0)
		fs_inode->mode |= (MODE_IRUSR | MODE_IRGRP | MODE_IROTH);
	    if ((getu32(fs, si->dos) & NTFS_SI_HID) == 0)
		fs_inode->mode |= (MODE_IWUSR | MODE_IWGRP | MODE_IWOTH);
	}

	/* File Name (always resident) */
	else if (type == NTFS_ATYPE_FNAME) {
	    ntfs_attr_fname *fname;
	    FS_NAME *fs_name;
	    UTF16 *name16;
	    UTF8 *name8;
	    if (attr->res != NTFS_MFT_RES) {
		tsk_errno = TSK_ERR_FS_INODE_INT;
		snprintf(tsk_errstr, TSK_ERRSTR_L,
		    "proc_attr_seq: File Name Attribute is not resident!");
		return 1;
	    }
	    fname =
		(ntfs_attr_fname *) ((uintptr_t) attr +
		getu16(fs, attr->c.r.soff));
	    if (fname->nspace == NTFS_FNAME_DOS) {
		continue;
	    }

	    /* Seek to the end of the fs_name structures in FS_INODE */
	    if (fs_inode->name) {
		for (fs_name = fs_inode->name;
		    (fs_name) && (fs_name->next != NULL);
		    fs_name = fs_name->next) {
		}

		/* add to the end of the existing list */
		fs_name->next = (FS_NAME *) mymalloc(sizeof(FS_NAME));
		if (fs_name->next == NULL) {
		    return 1;
		}
		fs_name = fs_name->next;
		fs_name->next = NULL;
	    }
	    else {
		/* First name, so we start a list */
		fs_inode->name = fs_name =
		    (FS_NAME *) mymalloc(sizeof(FS_NAME));
		if (fs_name == NULL) {
		    return 1;
		}
		fs_name->next = NULL;
	    }

	    name16 = (UTF16 *) & fname->name;
	    name8 = (UTF8 *) fs_name->name;
	    retVal =
		fs_UTF16toUTF8(fs, (const UTF16 **) &name16,
		(UTF16 *) ((uintptr_t) name16 +
		    fname->nlen * 2),
		&name8,
		(UTF8 *) ((uintptr_t) name8 +
		    sizeof(fs_name->name)), lenientConversion);
	    if (retVal != conversionOK) {
		if (verbose)
		    fprintf(stderr,
			"proc_attr_seq: Error converting NTFS name in $FNAME to UTF8: %d",
			retVal);
		*name8 = '\0';
	    }
	    /* Make sure it is NULL Terminated */
	    else if ((uintptr_t) name8 >
		(uintptr_t) fs_name->name + sizeof(fs_name->name))
		fs_name->name[sizeof(fs_name->name)] = '\0';
	    else
		*name8 = '\0';

	    fs_name->par_inode = getu48(fs, fname->par_ref);
	    fs_name->par_seq = getu16(fs, fname->par_seq);
	}

	/* If this is an attribute list than we need to process
	 * it to get the list of other entries to read.  But, because
	 * of the wierd scenario of the $MFT having an attribute list
	 * and not knowing where the other MFT entires are yet, we wait 
	 * until the end of the attrseq to processes the list and then
	 * we should have the $Data attribute loaded
	 */
	else if (type == NTFS_ATYPE_ATTRLIST) {
	    if (fs_data_attrl) {
		tsk_errno = TSK_ERR_FS_FUNC;
		snprintf(tsk_errstr, TSK_ERRSTR_L,
		    "Multiple instances of attribute lists in the same MFT\n"
		    "I didn't realize that could happen, contact the developers");
		return 1;
	    }
	    fs_data_attrl = fs_data_lookup(fs_inode->attr,
		NTFS_ATYPE_ATTRLIST, getu16(fs, attr->id));
	    if (fs_data_attrl == NULL) {
		strncat(tsk_errstr2,
		    " - proc_attrseq: getting attribute list",
		    TSK_ERRSTR_L - strlen(tsk_errstr2));
		return 1;
	    }
	}
    }


    /* we recalc our size everytime through here.  It is not the most
     * effecient, but easiest with all of the processing of attribute
     * lists and such
     */
    fs_inode->size = 0;
    for (fs_data = fs_inode->attr;
	fs_data != NULL; fs_data = fs_data->next) {

	if ((fs_data->flags & FS_DATA_INUSE) == 0)
	    continue;
	/* we account for the size of $Data, and directory locations */
	if ((fs_data->type == NTFS_ATYPE_DATA) ||
	    (fs_data->type == NTFS_ATYPE_IDXROOT) ||
	    (fs_data->type == NTFS_ATYPE_IDXALLOC))
	    fs_inode->size += fs_data->size;
    }

    /* Are we currently in the process of loading $MFT? */
    if (ntfs->loading_the_MFT == 1) {

	/* If we don't even have a mini cached version, get it now 
	 * Even if we are not done because of attribute lists, then we
	 * should at least have the head of the list 
	 */
	if (!ntfs->mft_data) {

	    for (fs_data = fs_inode->attr;
		fs_data != NULL; fs_data = fs_data->next) {

		if ((fs_data->flags & FS_DATA_INUSE) &&
		    (fs_data->type == NTFS_ATYPE_DATA) &&
		    (strcmp(fs_data->name, "$Data") == 0)) {
		    ntfs->mft_data = fs_data;
		    break;
		}
	    }
	    // @@@ Is this needed here -- maybe it should be only in _open
	    if (!ntfs->mft_data) {
		tsk_errno = TSK_ERR_FS_GENFS;
		snprintf(tsk_errstr, TSK_ERRSTR_L,
		    "$Data not found while loading the MFT");
		return 1;
	    }
	}

	/* Update the inode count based on the current size 
	 * IF $MFT has an attribute list, this value will increase each
	 * time
	 */
	fs->inum_count = ntfs->mft_data->size / ntfs->mft_rsize_b;
	fs->last_inum = fs->inum_count - 1;
    }

    /* If there was an attribute list, process it now, we wait because
     * the list can contain MFT entries that are described in $Data
     * of this MFT entry.  For example, part of the $DATA attribute
     * could follow the ATTRLIST entry, so we read it first and then 
     * process the attribute list
     */
    if (fs_data_attrl) {
	if (ntfs_proc_attrlist(ntfs, fs_inode, fs_data_attrl)) {
	    return 1;
	}
    }

    return 0;
}



/********   Attribute List Action and Function ***********/


/*
 * Attribute lists are used when all of the attribute  headers can not
 * fit into one MFT entry.  This contains an entry for every attribute
 * and where they are located.  We process this to get the locations
 * and then call proc_attrseq on each of those, which adds the data
 * to the fs_inode structure.
 *
 * Return 1 on error and 0 on success
 */
static uint8_t
ntfs_proc_attrlist(NTFS_INFO * ntfs,
    FS_INODE * fs_inode, FS_DATA * fs_data_attrlist)
{
    ntfs_attrlist *list;
    char *buf;
    uintptr_t endaddr;
    FS_INFO *fs = (FS_INFO *) & ntfs->fs_info;
    ntfs_mft *mft;
    FS_LOAD_FILE load_file;
    INUM_T hist[256];
    uint16_t histcnt = 0;

    if (verbose)
	fprintf(stderr,
	    "ntfs_proc_attrlist: Processing entry %"
	    PRIuINUM "\n", fs_inode->addr);

    if ((mft = (ntfs_mft *) mymalloc(ntfs->mft_rsize_b)) == NULL) {
	return 1;
    }

    /* Clear the contents of the history buffer */
    memset(hist, 0, sizeof(hist));

    /* add ourselves to the history */
    hist[histcnt++] = ntfs->mnum;

    /* Get a copy of the attribute list stream using the above action */
    load_file.left = load_file.total = (size_t) fs_data_attrlist->size;
    load_file.base = load_file.cur = buf =
	mymalloc(fs_data_attrlist->size);
    if (buf == NULL) {
	free(mft);
	return 1;
    }
    endaddr = (uintptr_t) buf + fs_data_attrlist->size;
    if (ntfs_data_walk(ntfs, ntfs->mnum,
	    fs_data_attrlist, 0, load_file_action, (void *) &load_file)) {
	strncat(tsk_errstr2, " - processing attrlist",
	    TSK_ERRSTR_L - strlen(tsk_errstr2));
	free(mft);
	return 1;
    }

    /* this value should be zero, if not then we didn't read all of the
     * buffer
     */
    if (load_file.left > 0) {
	tsk_errno = TSK_ERR_FS_FWALK;
	snprintf(tsk_errstr2, TSK_ERRSTR_L,
	    "processing attrlist of entry %" PRIuINUM, ntfs->mnum);
	free(mft);
	free(buf);
	return 1;
    }


    /* Process the list & and call ntfs_proc_attr */
    for (list = (ntfs_attrlist *) buf;
	(list) && ((uintptr_t) list < endaddr)
	&& (getu16(fs, list->len) > 0);
	list =
	(ntfs_attrlist *) ((uintptr_t) list + getu16(fs, list->len))) {
	INUM_T mftnum;
	uint32_t type;
	uint16_t id, i;
	/* Which MFT is this attribute in? */
	mftnum = getu48(fs, list->file_ref);
	/* Check the history to see if we have already processed this
	 * one before (if we have then we can skip it as we grabbed all
	 * of them last time
	 */
	for (i = 0; i < histcnt; i++) {
	    if (hist[i] == mftnum)
		break;
	}

	if (hist[i] == mftnum)
	    continue;
	/* This is a new one, add it to the history, and process it */
	if (histcnt < 256)
	    hist[histcnt++] = mftnum;
	type = getu32(fs, list->type);
	id = getu16(fs, list->id);
	if (verbose)
	    fprintf(stderr,
		"ntfs_proc_attrlist: mft: %" PRIuINUM
		" type %" PRIu32 " id %" PRIu16
		"  VCN: %" PRIu64 "\n", mftnum, type,
		id, getu64(fs, list->start_vcn));
	/* 
	 * Read the MFT entry 
	 */
	/* Sanity check. */
	if (mftnum < ntfs->fs_info.first_inum ||
	    mftnum > ntfs->fs_info.last_inum) {

	    if (verbose) {
		/* this case can easily occur if the attribute list was non-resident and the cluster has been reallocated */

		fprintf(stderr,
		    "Invalid MFT file reference (%"
		    PRIuINUM
		    ") in the unallocated attribute list of MFT %"
		    PRIuINUM "", mftnum, ntfs->mnum);
	    }

	    continue;
	}

	if (ntfs_dinode_lookup(ntfs, mft, mftnum)) {
	    free(mft);
	    free(buf);
	    strncat(tsk_errstr2, " - proc_attrlist",
		TSK_ERRSTR_L - strlen(tsk_errstr2));
	    return 1;
	}

	/* verify that this entry refers to the original one */
	if (getu48(fs, mft->base_ref) != ntfs->mnum) {

	    /* Before we raise alarms, check if the original was
	     * unallocated.  If so, then the list entry could 
	     * have been reallocated, so we will just ignore it
	     */
	    if ((getu16(fs, ntfs->mft->flags) & NTFS_MFT_INUSE) == 0) {
		continue;
	    }
	    else {
		tsk_errno = TSK_ERR_FS_INODE_INT;
		snprintf(tsk_errstr, TSK_ERRSTR_L,
		    "Extension record %" PRIuINUM
		    " (file ref = %" PRIuINUM
		    ") is not for attribute list of %"
		    PRIuINUM "", mftnum, getu48(fs,
			mft->base_ref), ntfs->mnum);
		free(mft);
		free(buf);
		return 1;
	    }
	}
	/* 
	 * Process the attribute seq for this MFT entry and add them
	 * to the FS_INODE structure
	 */

	if (ntfs_proc_attrseq(ntfs, fs_inode, (ntfs_attr *) ((uintptr_t)
		    mft +
		    getu16(fs,
			mft->
			attr_off)),
		ntfs->mft_rsize_b - getu16(fs, mft->attr_off))) {
	    strncat(tsk_errstr2, "- proc_attrlist",
		TSK_ERRSTR_L - strlen(tsk_errstr2));
	    free(mft);
	    free(buf);
	    return 1;
	}
    }

    free(mft);
    free(buf);
    return 0;
}



/*
 * Copy the MFT entry saved in ntfs->mft into the generic structure 
 */
static uint8_t
ntfs_dinode_copy(NTFS_INFO * ntfs, FS_INODE * fs_inode)
{
    ntfs_mft *mft = ntfs->mft;
    ntfs_attr *attr;
    FS_INFO *fs = (FS_INFO *) & ntfs->fs_info;

    /* if the attributes list has been used previously, then make sure the 
     * flags are cleared 
     */
    if (fs_inode->attr)
	fs_data_clear_list(fs_inode->attr);

    /* If there are any name structures allocated, then free 'em */
    if (fs_inode->name) {
	FS_NAME *fs_name1, *fs_name2;
	fs_name1 = fs_inode->name;

	while (fs_name1) {
	    fs_name2 = fs_name1->next;
	    free(fs_name1);
	    fs_name1 = fs_name2;
	}
	fs_inode->name = NULL;
    }

    /* Set the fs_inode values from mft */
    fs_inode->nlink = getu16(fs, mft->link);
    fs_inode->seq = getu16(fs, mft->seq);
    fs_inode->addr = ntfs->mnum;

    /* Set the mode for file or directory */
    if (getu16(fs, ntfs->mft->flags) & NTFS_MFT_DIR)
	fs_inode->mode = FS_INODE_DIR;
    else
	fs_inode->mode = FS_INODE_REG;

    /* the following will be changed once we find the correct attribute,
     * but initialize them now just in case 
     */
    fs_inode->uid = 0;
    fs_inode->gid = 0;
    fs_inode->size = 0;
    fs_inode->mtime = 0;
    fs_inode->atime = 0;
    fs_inode->ctime = 0;
    fs_inode->dtime = 0;
    fs_inode->crtime = 0;

    /* add the flags */
    fs_inode->flags =
	((getu16(fs, ntfs->mft->flags) &
	    NTFS_MFT_INUSE) ? FS_FLAG_META_ALLOC : FS_FLAG_META_UNALLOC);

    fs_inode->flags |=
	(getu16(fs, ntfs->mft->link) ?
	FS_FLAG_META_LINK : FS_FLAG_META_UNLINK);

    /* MFT entries are only allocated when needed, so it has been used */
    fs_inode->flags |= FS_FLAG_META_USED;

    /* Process the attribute sequence to fill in the fs_inode->attr
     * list and the other info such as size and times
     */
    attr = (ntfs_attr *) ((uintptr_t) mft + getu16(fs, mft->attr_off));
    if (ntfs_proc_attrseq(ntfs, fs_inode, attr,
	    ntfs->mft_rsize_b - getu16(fs, mft->attr_off))) {
	return 1;
    }
    return 0;
}


/*
 * Read the mft entry and put it into the ntfs->mft structure
 * Also sets the ntfs->mnum value
 *
 * Return 1 on error and 0 on success
 */
uint8_t
ntfs_dinode_load(NTFS_INFO * ntfs, INUM_T mftnum)
{
    /* mft_lookup does a sanity check, so we can skip it here */
    if (ntfs_dinode_lookup(ntfs, ntfs->mft, mftnum))
	return 1;
    ntfs->mnum = mftnum;
    return 0;
}


/*
 * return the MFT entry in the generic FS_INODE format
 *
 * Return NULL on error 
 */
static FS_INODE *
ntfs_inode_lookup(FS_INFO * fs, INUM_T mftnum)
{
    NTFS_INFO *ntfs = (NTFS_INFO *) fs;
    FS_INODE *fs_inode = fs_inode_alloc(NTFS_NDADDR, NTFS_NIADDR);
    if (fs_inode == NULL)
	return NULL;

    /* Lookup inode and store it in the ntfs structure */
    if (ntfs_dinode_load(ntfs, mftnum)) {
	fs_inode_free(fs_inode);
	return NULL;
    }

    /* Copy the structure in ntfs to generic fs_inode */
    if (ntfs_dinode_copy(ntfs, fs_inode)) {
	fs_inode_free(fs_inode);
	return NULL;
    }

    return (fs_inode);
}




/**********************************************************************
 *
 *  Load special MFT structures into the NTFS_INFO structure
 *
 **********************************************************************/

/* The attrdef structure defines the types of attributes and gives a 
 * name value to the type number.
 *
 * We currently do not use this during the analysis (Because it has not
 * historically changed, but we do display it in fsstat 
 *
 * Return 1 on error and 0 on success
 */
static uint8_t
ntfs_load_attrdef(NTFS_INFO * ntfs)
{
    FS_INODE *fs_inode;
    FS_DATA *fs_data;
    FS_INFO *fs = &ntfs->fs_info;
    FS_LOAD_FILE load_file;

    /* if already loaded, return now */
    if (ntfs->attrdef)
	return 1;

    if ((fs_inode = ntfs_inode_lookup(fs, NTFS_MFT_ATTR)) == NULL)
	return 1;

    fs_data = fs_data_lookup_noid(fs_inode->attr, NTFS_ATYPE_DATA);
    if (!fs_data) {
	//("Data attribute not found in $Attr");
	fs_inode_free(fs_inode);
	return 1;
    }


    /* Get a copy of the attribute list stream using the above action */
    load_file.left = load_file.total = (size_t) fs_data->size;
    load_file.base = load_file.cur = mymalloc(fs_data->size);
    if (load_file.cur == NULL) {
	fs_inode_free(fs_inode);
	return 1;
    }
    ntfs->attrdef = (ntfs_attrdef *) load_file.base;

    if (ntfs_data_walk(ntfs, fs_inode->addr, fs_data,
	    0, load_file_action, (void *) &load_file)) {
	strncat(tsk_errstr2, " - load_attrdef",
	    TSK_ERRSTR_L - strlen(tsk_errstr2));
	fs_inode_free(fs_inode);
	return 1;
    }
    else if (load_file.left > 0) {
	tsk_errno = TSK_ERR_FS_FWALK;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "load_attrdef: space still left after walking $Attr data");
	fs_inode_free(fs_inode);
	return 1;
    }

    fs_inode_free(fs_inode);
    return 0;
}


/* 
 * return the name of the attribute type.  If the attribute has not
 * been loaded yet, it will be.
 *
 * Return 1 on error and 0 on success
 */
uint8_t
ntfs_attrname_lookup(FS_INFO * fs, uint16_t type, char *name, int len)
{
    NTFS_INFO *ntfs = (NTFS_INFO *) fs;
    ntfs_attrdef *attrdef;
    if (!ntfs->attrdef) {
	if (ntfs_load_attrdef(ntfs))
	    return 1;
    }

    attrdef = ntfs->attrdef;
    while (getu32(fs, attrdef->type)) {
	if (getu32(fs, attrdef->type) == type) {

	    UTF16 *name16 = (UTF16 *) attrdef->label;
	    UTF8 *name8 = (UTF8 *) name;
	    int retVal;
	    retVal =
		fs_UTF16toUTF8(fs, (const UTF16 **) &name16,
		(UTF16 *) ((uintptr_t) name16 +
		    sizeof(attrdef->
			label)),
		&name8,
		(UTF8 *) ((uintptr_t) name8 + len), lenientConversion);
	    if (retVal != conversionOK) {
		if (verbose)
		    fprintf(stderr,
			"attrname_lookup: Error converting NTFS attribute def label to UTF8: %d",
			retVal);
		break;
	    }

	    /* Make sure it is NULL Terminated */
	    else if ((uintptr_t) name8 > (uintptr_t) name + len)
		name[len] = '\0';
	    else
		*name8 = '\0';
	    return 0;
	}
	attrdef++;
    }
    /* If we didn't find it, then call it '?' */
    snprintf(name, len, "?");
    return 0;
}


/* Load the block bitmap $Data run  and allocate a buffer for a cache 
 *
 * return 1 on error and 0 on success
 * */
static uint8_t
ntfs_load_bmap(NTFS_INFO * ntfs)
{
    SSIZE_T cnt;
    ntfs_attr *attr;
    FS_INFO *fs = &ntfs->fs_info;

    /* Get data on the bitmap */
    if (ntfs_dinode_load(ntfs, NTFS_MFT_BMAP)) {
	return 1;
    }

    attr = (ntfs_attr *) ((uintptr_t) ntfs->mft +
	getu16(fs, ntfs->mft->attr_off));

    /* cycle through them */
    while (((uintptr_t) attr >= (uintptr_t) ntfs->mft)
	&& ((uintptr_t) attr <=
	    ((uintptr_t) ntfs->mft + (uintptr_t) ntfs->mft_rsize_b))
	&& (getu32(fs, attr->len) > 0
	    && (getu32(fs, attr->type) != 0xffffffff)
	    && (getu32(fs, attr->type) != NTFS_ATYPE_DATA))) {
	attr = (ntfs_attr *) ((uintptr_t) attr + getu32(fs, attr->len));
    }

    /* did we get it? */
    if (getu32(fs, attr->type) != NTFS_ATYPE_DATA) {
	tsk_errno = TSK_ERR_FS_INODE_INT;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "Error Finding Bitmap Data Attribute");
	return 1;
    }

    /* convert to generic form */
    ntfs->bmap = ntfs_make_data_run(ntfs,
	(ntfs_runlist
	    *) ((uintptr_t) attr + getu16(fs, attr->c.nr.run_off)), NULL);

    if (ntfs->bmap == NULL) {
	return 1;
    }

    ntfs->bmap_buf = data_buf_alloc(fs->block_size);
    if (ntfs->bmap_buf == NULL) {
	return 1;
    }

    /* Load the first cluster so that we have something there */
    ntfs->bmap_buf_off = 0;
    if (ntfs->bmap->addr > fs->last_block) {
	tsk_errno = TSK_ERR_FS_GENFS;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "ntfs_load_bmap: Bitmap too large for image size: %"
	    PRIuDADDR "", ntfs->bmap->addr);
	return 1;
    }
    cnt =
	fs_read_block(fs, ntfs->bmap_buf, fs->block_size,
	ntfs->bmap->addr);
    if (cnt != fs->block_size) {
	if (cnt != 1) {
	    tsk_errno = TSK_ERR_FS_READ;
	    tsk_errstr[0] = '\0';
	}
	snprintf(tsk_errstr2, TSK_ERRSTR_L,
	    "ntfs_load_bmap: Error reading block at %"
	    PRIuDADDR, ntfs->bmap->addr);
	return 1;
    }
    return 0;
}


/*
 * Load the VOLUME MFT entry and the VINFO attribute so that we
 * can identify the volume version of this.  
 *
 * Return 1 on error and 0 on success
 */
static uint8_t
ntfs_load_ver(NTFS_INFO * ntfs)
{
    FS_INFO *fs = (FS_INFO *) & ntfs->fs_info;
    FS_INODE *fs_inode;
    FS_DATA *fs_data;
    if ((fs_inode = ntfs_inode_lookup(fs, NTFS_MFT_VOL)) == NULL) {
	return 1;
    }

    /* cache the data attribute */
    fs_data = fs_data_lookup_noid(fs_inode->attr, NTFS_ATYPE_VINFO);
    if (!fs_data) {
	tsk_errno = TSK_ERR_FS_INODE_INT;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "Volume Info attribute not found in $Volume");
	fs_inode_free(fs_inode);
	return 1;
    }

    if ((fs_data->flags & FS_DATA_RES)
	&& (fs_data->size)) {
	ntfs_attr_vinfo *vinfo = (ntfs_attr_vinfo *) fs_data->buf;

	if ((vinfo->maj_ver == 1)
	    && (vinfo->min_ver == 2)) {
	    ntfs->ver = NTFS_VINFO_NT;
	}
	else if ((vinfo->maj_ver == 3)
	    && (vinfo->min_ver == 0)) {
	    ntfs->ver = NTFS_VINFO_2K;
	}
	else if ((vinfo->maj_ver == 3)
	    && (vinfo->min_ver == 1)) {
	    ntfs->ver = NTFS_VINFO_XP;
	}
	else {
	    tsk_errno = TSK_ERR_FS_GENFS;
	    snprintf(tsk_errstr, TSK_ERRSTR_L,
		"unknown version: %d.%d\n",
		vinfo->maj_ver, vinfo->min_ver);
	    fs_inode_free(fs_inode);
	    return 1;
	}
    }
    else {
	tsk_errno = TSK_ERR_FS_GENFS;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "load_version: VINFO is a non-resident attribute");
	return 1;
    }

    fs_inode_free(fs_inode);
    return 0;
}


/**********************************************************************
 *
 *  Exported Walk Functions
 *
 **********************************************************************/

/*
 *
 * flag values: FS_FLAG_FILE_AONLY, FS_FLAG_FILE_SLACK, FS_FLAG_FILE_NOSPARSE
 * FS_FLAG_FILE_NOID
 * 
 * nothing special is done for FS_FLAG_FILE_RECOVER
 *
 * action uses: FS_FLAG_DATA_CONT
 *
 * No notion of meta with NTFS
 *
 * a type of 0 will use $Data for files and IDXROOT for directories
 * an id of 0 will ignore the id and just find the first entry with the type
 *
 * Return 0 on success and 1 on error
 */
uint8_t
ntfs_file_walk(FS_INFO * fs,
    FS_INODE * fs_inode,
    uint32_t type, uint16_t id,
    int flags, FS_FILE_WALK_FN action, void *ptr)
{
    FS_DATA *fs_data;
    NTFS_INFO *ntfs = (NTFS_INFO *) fs;

    /* no data */
    if (fs_inode->attr == NULL) {
	tsk_errno = TSK_ERR_FS_ARG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "file_walk: attributes are NULL");
	return 1;
    }

    /* if no type was given, then use DATA for files and IDXROOT for dirs */
    if (type == 0) {
	if ((fs_inode->mode & FS_INODE_FMT) == FS_INODE_DIR)
	    type = NTFS_ATYPE_IDXROOT;
	else
	    type = NTFS_ATYPE_DATA;
    }

    /* 
     * Find the record with the correct type value 
     */
    if (flags & FS_FLAG_FILE_NOID) {
	fs_data = fs_data_lookup_noid(fs_inode->attr, type);
	if (!fs_data) {
	    tsk_errno = TSK_ERR_FS_ARG;
	    snprintf(tsk_errstr, TSK_ERRSTR_L,
		"ntfs_file_walk: type %" PRIu32 " not found in file",
		type);
	    return 1;
	}
    }
    else {
	fs_data = fs_data_lookup(fs_inode->attr, type, id);
	if (!fs_data) {
	    tsk_errno = TSK_ERR_FS_ARG;
	    snprintf(tsk_errstr, TSK_ERRSTR_L,
		"ntfs_file_walk: type %" PRIu32 "-%" PRIu16
		" not found in file", type, id);
	    return 1;
	}
    }


    /* process the content */
    return ntfs_data_walk(ntfs, fs_inode->addr, fs_data, flags, action,
	ptr);
}





/*
 * flags: FS_FLAG_DATA_ALLOC and FS_FLAG_UNALLOC
 *
 * @@@ We should probably consider some data META, but it is tough with
 * the NTFS design ...
 */
uint8_t
ntfs_block_walk(FS_INFO * fs,
    DADDR_T start_blk,
    DADDR_T end_blk, int flags, FS_BLOCK_WALK_FN action, void *ptr)
{
    char *myname = "ntfs_block_walk";
    NTFS_INFO *ntfs = (NTFS_INFO *) fs;
    DADDR_T addr;
    DATA_BUF *data_buf;
    int myflags;
    /*
     * Sanity checks.
     */
    if (start_blk < fs->first_block || start_blk > fs->last_block) {
	tsk_errno = TSK_ERR_FS_WALK_RNG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "%s: start block: %" PRIuDADDR "", myname, start_blk);
	return 1;
    }
    else if (end_blk < fs->first_block || end_blk > fs->last_block) {
	tsk_errno = TSK_ERR_FS_FUNC;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "%s: last block: %" PRIuDADDR "", myname, end_blk);
	return 1;
    }

    if ((data_buf = data_buf_alloc(fs->block_size)) == NULL) {
	return 1;
    }

    /* Cycle through the blocks */
    for (addr = start_blk; addr <= end_blk; addr++) {
	int retval;

	/* identify if the cluster is allocated or not */
	retval = is_clustalloc(ntfs, addr);
	if (retval == -1) {
	    data_buf_free(data_buf);
	    return 1;
	}
	else if (retval == 1) {
	    myflags = FS_FLAG_DATA_ALLOC;
	}
	else {
	    myflags = FS_FLAG_DATA_UNALLOC;
	}

	if (flags & myflags) {
	    SSIZE_T cnt;

	    cnt = fs_read_block(fs, data_buf, fs->block_size, addr);
	    if (cnt != fs->block_size) {
		if (cnt != 1) {
		    tsk_errno = TSK_ERR_FS_READ;
		    tsk_errstr[0] = '\0';
		}
		snprintf(tsk_errstr2, TSK_ERRSTR_L,
		    "ntfs_block_walk: Error reading block at %"
		    PRIuDADDR, addr);
		data_buf_free(data_buf);
		return 1;
	    }

	    retval = action(fs, addr, data_buf->data, myflags, ptr);
	    if (retval == WALK_STOP) {
		data_buf_free(data_buf);
		return 0;
	    }
	    else if (retval == WALK_ERROR) {
		data_buf_free(data_buf);
		return 1;
	    }
	}
    }

    data_buf_free(data_buf);
    return 0;
}



/*
 * inode_walk
 *
 * Flags: FS_FLAG_META_ALLOC, FS_FLAG_META_UNALLOC, FS_FLAG_META_LINK,
 * FS_FLAG_META_UNLINK, FS_FLAG_META_USED
 *
 * Not used: FS_FLAG_META_UNUSED (Only allocated when needed)
 */
uint8_t
ntfs_inode_walk(FS_INFO * fs,
    INUM_T start_inum,
    INUM_T end_inum, int flags, FS_INODE_WALK_FN action, void *ptr)
{
    NTFS_INFO *ntfs = (NTFS_INFO *) fs;
    int myflags;
    INUM_T mftnum;
    FS_INODE *fs_inode;

    /*
     * Sanity checks.
     */
    if (start_inum < fs->first_inum) {
	tsk_errno = TSK_ERR_FS_WALK_RNG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "inode_walk: Starting inode number is too small (%"
	    PRIuINUM ")", start_inum);
	return 1;
    }
    if (start_inum > fs->last_inum) {
	tsk_errno = TSK_ERR_FS_WALK_RNG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "inode_walk: Starting inode number is too large (%"
	    PRIuINUM ")", start_inum);
	return 1;
    }
    if (end_inum < fs->first_inum) {
	tsk_errno = TSK_ERR_FS_FUNC;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "inode_walk: Ending inode number is too small (%"
	    PRIuINUM ")", end_inum);
	return 1;
    }
    if (end_inum > fs->last_inum) {
	tsk_errno = TSK_ERR_FS_FUNC;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "Ending inode number is too large (%" PRIuINUM ")", end_inum);
	return 1;
    }

    if ((fs_inode = fs_inode_alloc(NTFS_NDADDR, NTFS_NIADDR)) == NULL) {
	return 1;
    }

    for (mftnum = start_inum; mftnum <= end_inum; mftnum++) {
	int retval;

	/* read MFT entry in to NTFS_INFO */
	if (ntfs_dinode_load(ntfs, mftnum)) {
	    fs_inode_free(fs_inode);
	    return 1;
	}

	/* we only want to look at base file records 
	 * (extended are because the base could not fit into one)
	 */
	if (getu48(fs, ntfs->mft->base_ref) != NTFS_MFT_BASE)
	    continue;

	/* NOTE: We could add a sanity check here with the MFT bitmap
	 * to validate of the INUSE flag and bitmap are in agreement
	 */
	/* check flags */
	myflags =
	    ((getu16(fs, ntfs->mft->flags) &
		NTFS_MFT_INUSE) ? FS_FLAG_META_ALLOC :
	    FS_FLAG_META_UNALLOC);
	myflags |=
	    (getu16(fs,
		ntfs->mft->
		link) ? FS_FLAG_META_LINK : FS_FLAG_META_UNLINK);

	/* MFT entries are only allocated when needed, so it must have 
	 * been used
	 */
	myflags |= FS_FLAG_META_USED;
	if ((flags & myflags) != myflags)
	    continue;

	/* copy into generic format */
	if (ntfs_dinode_copy(ntfs, fs_inode)) {
	    fs_inode_free(fs_inode);
	    return 1;
	}

	/* call action */
	retval = action(fs, fs_inode, myflags, ptr);

	if (retval == WALK_STOP) {
	    fs_inode_free(fs_inode);
	    return 0;
	}
	else if (retval == WALK_ERROR) {
	    fs_inode_free(fs_inode);
	    return 1;
	}
    }

    fs_inode_free(fs_inode);
    return 0;
}


static uint8_t
ntfs_fscheck(FS_INFO * fs, FILE * hFile)
{
    tsk_errno = TSK_ERR_FS_FUNC;
    snprintf(tsk_errstr, TSK_ERRSTR_L,
	"fscheck not implemented for NTFS yet");
    return 1;
}


static uint8_t
ntfs_fsstat(FS_INFO * fs, FILE * hFile)
{
    NTFS_INFO *ntfs = (NTFS_INFO *) fs;
    FS_INODE *fs_inode;
    FS_DATA *fs_data;
    char asc[512];
    ntfs_attrdef *attrdeftmp;
    fprintf(hFile, "FILE SYSTEM INFORMATION\n");
    fprintf(hFile, "--------------------------------------------\n");
    fprintf(hFile, "File System Type: NTFS\n");
    fprintf(hFile,
	"Volume Serial Number: %.16" PRIX64
	"\n", getu64(fs, ntfs->fs->serial));
    fprintf(hFile, "OEM Name: %c%c%c%c%c%c%c%c\n",
	ntfs->fs->oemname[0],
	ntfs->fs->oemname[1],
	ntfs->fs->oemname[2],
	ntfs->fs->oemname[3],
	ntfs->fs->oemname[4],
	ntfs->fs->oemname[5], ntfs->fs->oemname[6], ntfs->fs->oemname[7]);
    /*
     * Volume 
     */
    fs_inode = ntfs_inode_lookup(fs, NTFS_MFT_VOL);
    if (!fs_inode) {
	tsk_errno = TSK_ERR_FS_INODE_NUM;
	strncat(tsk_errstr2, " - fsstat: Error finding Volume MFT Entry",
	    TSK_ERRSTR_L - strlen(tsk_errstr2));
	return 1;
    }

    fs_data = fs_data_lookup_noid(fs_inode->attr, NTFS_ATYPE_VNAME);
    if (!fs_data) {
	tsk_errno = TSK_ERR_FS_INODE_INT;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "Volume Name attribute not found in $Volume");
	return 1;
    }

    if ((fs_data->flags & FS_DATA_RES)
	&& (fs_data->size)) {

	UTF16 *name16 = (UTF16 *) fs_data->buf;
	UTF8 *name8 = (UTF8 *) asc;
	int retVal;
	retVal =
	    fs_UTF16toUTF8(fs, (const UTF16 **) &name16,
	    (UTF16 *) ((uintptr_t) name16 +
		(int) fs_data->
		size), &name8,
	    (UTF8 *) ((uintptr_t) name8 + sizeof(asc)), lenientConversion);
	if (retVal != conversionOK) {
	    if (verbose)
		fprintf(stderr,
		    "fsstat: Error converting NTFS Volume label to UTF8: %d",
		    retVal);
	    *name8 = '\0';
	}

	/* Make sure it is NULL Terminated */
	else if ((uintptr_t) name8 > (uintptr_t) asc + sizeof(asc))
	    asc[sizeof(asc)] = '\0';
	else
	    *name8 = '\0';
	fprintf(hFile, "Volume Name: %s\n", asc);
    }

    fs_inode_free(fs_inode);
    fs_inode = NULL;
    fs_data = NULL;
    if (ntfs->ver == NTFS_VINFO_NT)
	fprintf(hFile, "Version: Windows NT\n");
    else if (ntfs->ver == NTFS_VINFO_2K)
	fprintf(hFile, "Version: Windows 2000\n");
    else if (ntfs->ver == NTFS_VINFO_XP)
	fprintf(hFile, "Version: Windows XP\n");
    fprintf(hFile, "\nMETADATA INFORMATION\n");
    fprintf(hFile, "--------------------------------------------\n");
    fprintf(hFile,
	"First Cluster of MFT: %" PRIu64 "\n",
	getu64(fs, ntfs->fs->mft_clust));
    fprintf(hFile,
	"First Cluster of MFT Mirror: %"
	PRIu64 "\n", getu64(fs, ntfs->fs->mftm_clust));
    fprintf(hFile,
	"Size of MFT Entries: %" PRIu16 " bytes\n", ntfs->mft_rsize_b);
    fprintf(hFile,
	"Size of Index Records: %" PRIu16 " bytes\n", ntfs->idx_rsize_b);
    fprintf(hFile,
	"Range: %" PRIuINUM " - %" PRIuINUM
	"\n", fs->first_inum, fs->last_inum);
    fprintf(hFile, "Root Directory: %" PRIuINUM "\n", fs->root_inum);
    fprintf(hFile, "\nCONTENT INFORMATION\n");
    fprintf(hFile, "--------------------------------------------\n");
    fprintf(hFile, "Sector Size: %" PRIu16 "\n", ntfs->ssize_b);
    fprintf(hFile, "Cluster Size: %" PRIu16 "\n", ntfs->csize_b);
    fprintf(hFile,
	"Total Cluster Range: %" PRIuDADDR
	" - %" PRIuDADDR "\n", fs->first_block, fs->last_block);
    fprintf(hFile,
	"Total Sector Range: 0 - %" PRIu64
	"\n", getu64(fs, ntfs->fs->vol_size_s) - 1);
    /* 
     * Attrdef Info 
     */
    fprintf(hFile, "\n$AttrDef Attribute Values:\n");
    if (!ntfs->attrdef) {
	if (ntfs_load_attrdef(ntfs)) {
	    fprintf(hFile, "Error loading attribute definitions\n");
	    goto attrdef_egress;
	}
    }

    attrdeftmp = ntfs->attrdef;
    while (getu32(fs, attrdeftmp->type)) {
	UTF16 *name16 = (UTF16 *) attrdeftmp->label;
	UTF8 *name8 = (UTF8 *) asc;
	int retVal;
	retVal =
	    fs_UTF16toUTF8(fs, (const UTF16 **) &name16,
	    (UTF16 *) ((uintptr_t) name16 +
		sizeof(attrdeftmp->
		    label)),
	    &name8,
	    (UTF8 *) ((uintptr_t) name8 + sizeof(asc)), lenientConversion);
	if (retVal != conversionOK) {
	    if (verbose)
		fprintf(stderr,
		    "fsstat: Error converting NTFS attribute def label to UTF8: %d",
		    retVal);
	    *name8 = '\0';
	}

	/* Make sure it is NULL Terminated */
	else if ((uintptr_t) name8 > (uintptr_t) asc + sizeof(asc))
	    asc[sizeof(asc)] = '\0';
	else
	    *name8 = '\0';
	fprintf(hFile, "%s (%" PRIu32 ")   ",
	    asc, getu32(fs, attrdeftmp->type));
	if ((getu64(fs, attrdeftmp->minsize) == 0) &&
	    (getu64(fs, attrdeftmp->maxsize) == 0xffffffffffffffffULL)) {

	    fprintf(hFile, "Size: No Limit");
	}
	else {
	    fprintf(hFile, "Size: %" PRIu64 "-%" PRIu64,
		getu64(fs, attrdeftmp->minsize),
		getu64(fs, attrdeftmp->maxsize));
	}

	fprintf(hFile, "   Flags: %s%s%s\n",
	    (getu32(fs, attrdeftmp->flags) &
		NTFS_ATTRDEF_FLAGS_RES ? "Resident" :
		""), (getu32(fs,
		    attrdeftmp->
		    flags) &
		NTFS_ATTRDEF_FLAGS_NONRES ?
		"Non-resident" : ""),
	    (getu32(fs, attrdeftmp->flags) &
		NTFS_ATTRDEF_FLAGS_IDX ? ",Index" : ""));
	attrdeftmp++;
    }

  attrdef_egress:

    return 0;
}


/************************* istat *******************************/

#define NTFS_PRINT_WIDTH   8
typedef struct {
    FILE *hFile;
    int idx;
} NTFS_PRINT_ADDR;
static uint8_t
print_addr_act(FS_INFO * fs, DADDR_T addr,
    char *buf, unsigned int size, int flags, void *ptr)
{
    NTFS_PRINT_ADDR *print = (NTFS_PRINT_ADDR *) ptr;
    fprintf(print->hFile, "%" PRIuDADDR " ", addr);
    if (++(print->idx) == NTFS_PRINT_WIDTH) {
	fprintf(print->hFile, "\n");
	print->idx = 0;
    }

    return WALK_CONT;
}


static uint8_t
ntfs_istat(FS_INFO * fs, FILE * hFile,
    INUM_T inum, int numblock, int32_t sec_skew)
{
    FS_INODE *fs_inode;
    FS_DATA *fs_data;
    NTFS_INFO *ntfs = (NTFS_INFO *) fs;

    fs_inode = ntfs_inode_lookup(fs, inum);
    if (fs_inode == NULL) {
	strncat(tsk_errstr2, " - istat",
	    TSK_ERRSTR_L - strlen(tsk_errstr2));
	return 1;
    }

    fprintf(hFile, "MFT Entry Header Values:\n");
    fprintf(hFile,
	"Entry: %" PRIuINUM
	"        Sequence: %" PRIu32 "\n", inum, fs_inode->seq);
    if (getu48(fs, ntfs->mft->base_ref) != 0) {
	fprintf(hFile,
	    "Base File Record: %" PRIu64 "\n",
	    (uint64_t) getu48(fs, ntfs->mft->base_ref));
    }

    fprintf(hFile,
	"$LogFile Sequence Number: %" PRIu64
	"\n", getu64(fs, ntfs->mft->lsn));
    fprintf(hFile, "%sAllocated %s\n",
	(fs_inode->
	    flags & FS_FLAG_META_ALLOC) ? "" :
	"Not ", (fs_inode->mode & FS_INODE_DIR) ? "Directory" : "File");
    fprintf(hFile, "Links: %u\n", fs_inode->nlink);

    /* STANDARD_INFORMATION info */
    fs_data = fs_data_lookup_noid(fs_inode->attr, NTFS_ATYPE_SI);
    if (fs_data) {
	ntfs_attr_si *si = (ntfs_attr_si *) fs_data->buf;
	int a = 0;
	fprintf(hFile, "\n$STANDARD_INFORMATION Attribute Values:\n");
	fprintf(hFile, "Flags: ");
	if (getu32(fs, si->dos) & NTFS_SI_RO)
	    fprintf(hFile, "%sRead Only", a++ == 0 ? "" : ", ");
	if (getu32(fs, si->dos) & NTFS_SI_HID)
	    fprintf(hFile, "%sHidden", a++ == 0 ? "" : ", ");
	if (getu32(fs, si->dos) & NTFS_SI_SYS)
	    fprintf(hFile, "%sSystem", a++ == 0 ? "" : ", ");
	if (getu32(fs, si->dos) & NTFS_SI_ARCH)
	    fprintf(hFile, "%sArchive", a++ == 0 ? "" : ", ");
	if (getu32(fs, si->dos) & NTFS_SI_DEV)
	    fprintf(hFile, "%sDevice", a++ == 0 ? "" : ", ");
	if (getu32(fs, si->dos) & NTFS_SI_NORM)
	    fprintf(hFile, "%sNormal", a++ == 0 ? "" : ", ");
	if (getu32(fs, si->dos) & NTFS_SI_TEMP)
	    fprintf(hFile, "%sTemporary", a++ == 0 ? "" : ", ");
	if (getu32(fs, si->dos) & NTFS_SI_SPAR)
	    fprintf(hFile, "%sSparse", a++ == 0 ? "" : ", ");
	if (getu32(fs, si->dos) & NTFS_SI_REP)
	    fprintf(hFile, "%sReparse Point", a++ == 0 ? "" : ", ");
	if (getu32(fs, si->dos) & NTFS_SI_COMP)
	    fprintf(hFile, "%sCompressed", a++ == 0 ? "" : ", ");
	if (getu32(fs, si->dos) & NTFS_SI_OFF)
	    fprintf(hFile, "%sOffline", a++ == 0 ? "" : ", ");
	if (getu32(fs, si->dos) & NTFS_SI_NOIDX)
	    fprintf(hFile, "%sNot Content Indexed", a++ == 0 ? "" : ", ");
	if (getu32(fs, si->dos) & NTFS_SI_ENC)
	    fprintf(hFile, "%sEncrypted", a++ == 0 ? "" : ", ");
	fprintf(hFile, "\n");
	fprintf(hFile,
	    "Owner ID: %" PRIu32
	    "     Security ID: %" PRIu32 "\n",
	    getu32(fs, si->own_id), getu32(fs, si->sec_id));
	if (getu32(fs, si->maxver) != 0) {
	    fprintf(hFile,
		"Version %" PRIu32 " of %" PRIu32
		"\n", getu32(fs, si->ver), getu32(fs, si->maxver));
	}

	if (getu64(fs, si->quota) != 0) {
	    fprintf(hFile, "Quota Charged: %" PRIu64 "\n",
		getu64(fs, si->quota));
	}

	if (getu64(fs, si->usn) != 0) {
	    fprintf(hFile,
		"Last User Journal Update Sequence Number: %"
		PRIu64 "\n", getu64(fs, si->usn));
	}


	/* Times - take it from fs_inode instead of redoing the work */

	if (sec_skew != 0) {
	    fprintf(hFile, "\nAdjusted times:\n");
	    fs_inode->mtime -= sec_skew;
	    fs_inode->atime -= sec_skew;
	    fs_inode->ctime -= sec_skew;
	    fs_inode->crtime -= sec_skew;
	    fprintf(hFile, "Created:\t%s", ctime(&fs_inode->crtime));
	    fprintf(hFile, "File Modified:\t%s", ctime(&fs_inode->mtime));
	    fprintf(hFile, "MFT Modified:\t%s", ctime(&fs_inode->ctime));
	    fprintf(hFile, "Accessed:\t%s", ctime(&fs_inode->atime));
	    fs_inode->mtime += sec_skew;
	    fs_inode->atime += sec_skew;
	    fs_inode->ctime += sec_skew;
	    fs_inode->crtime += sec_skew;
	    fprintf(hFile, "\nOriginal times:\n");
	}

	fprintf(hFile, "Created:\t%s", ctime(&fs_inode->crtime));
	fprintf(hFile, "File Modified:\t%s", ctime(&fs_inode->mtime));
	fprintf(hFile, "MFT Modified:\t%s", ctime(&fs_inode->ctime));
	fprintf(hFile, "Accessed:\t%s", ctime(&fs_inode->atime));
    }

    /* $FILE_NAME Information */
    fs_data = fs_data_lookup_noid(fs_inode->attr, NTFS_ATYPE_FNAME);
    if (fs_data) {

	ntfs_attr_fname *fname = (ntfs_attr_fname *) fs_data->buf;
	time_t cr_time, m_time, c_time, a_time;
	uint64_t flags;
	int a = 0;
	fprintf(hFile, "\n$FILE_NAME Attribute Values:\n");
	flags = getu64(fs, fname->flags);
	fprintf(hFile, "Flags: ");
	if (flags & NTFS_FNAME_FLAGS_DIR)
	    fprintf(hFile, "%sDirectory", a++ == 0 ? "" : ", ");
	if (flags & NTFS_FNAME_FLAGS_DEV)
	    fprintf(hFile, "%sDevice", a++ == 0 ? "" : ", ");
	if (flags & NTFS_FNAME_FLAGS_NORM)
	    fprintf(hFile, "%sNormal", a++ == 0 ? "" : ", ");
	if (flags & NTFS_FNAME_FLAGS_RO)
	    fprintf(hFile, "%sRead Only", a++ == 0 ? "" : ", ");
	if (flags & NTFS_FNAME_FLAGS_HID)
	    fprintf(hFile, "%sHidden", a++ == 0 ? "" : ", ");
	if (flags & NTFS_FNAME_FLAGS_SYS)
	    fprintf(hFile, "%sSystem", a++ == 0 ? "" : ", ");
	if (flags & NTFS_FNAME_FLAGS_ARCH)
	    fprintf(hFile, "%sArchive", a++ == 0 ? "" : ", ");
	if (flags & NTFS_FNAME_FLAGS_TEMP)
	    fprintf(hFile, "%sTemp", a++ == 0 ? "" : ", ");
	if (flags & NTFS_FNAME_FLAGS_SPAR)
	    fprintf(hFile, "%sSparse", a++ == 0 ? "" : ", ");
	if (flags & NTFS_FNAME_FLAGS_REP)
	    fprintf(hFile, "%sReparse Point", a++ == 0 ? "" : ", ");
	if (flags & NTFS_FNAME_FLAGS_COMP)
	    fprintf(hFile, "%sCompressed", a++ == 0 ? "" : ", ");
	if (flags & NTFS_FNAME_FLAGS_ENC)
	    fprintf(hFile, "%sEncrypted", a++ == 0 ? "" : ", ");
	if (flags & NTFS_FNAME_FLAGS_OFF)
	    fprintf(hFile, "%sOffline", a++ == 0 ? "" : ", ");
	if (flags & NTFS_FNAME_FLAGS_NOIDX)
	    fprintf(hFile, "%sNot Content Indexed", a++ == 0 ? "" : ", ");
	if (flags & NTFS_FNAME_FLAGS_IDXVIEW)
	    fprintf(hFile, "%sIndex View", a++ == 0 ? "" : ", ");
	fprintf(hFile, "\n");
	/* We could look this up in the attribute, but we already did
	 * the work */
	if (fs_inode->name) {
	    FS_NAME *fs_name = fs_inode->name;
	    fprintf(hFile, "Name: ");
	    while (fs_name) {
		fprintf(hFile, "%s", fs_name->name);
		fs_name = fs_name->next;
		if (fs_name)
		    fprintf(hFile, ", ");
		else
		    fprintf(hFile, "\n");
	    }
	}

	fprintf(hFile,
	    "Parent MFT Entry: %" PRIu64
	    " \tSequence: %" PRIu16 "\n",
	    (uint64_t) getu48(fs, fname->par_ref),
	    getu16(fs, fname->par_seq));
	fprintf(hFile,
	    "Allocated Size: %" PRIu64
	    "   \tActual Size: %" PRIu64 "\n",
	    getu64(fs, fname->alloc_fsize), getu64(fs, fname->real_fsize));
	/* 
	 * Times 
	 */
	cr_time = nt2unixtime(getu64(fs, fname->crtime));
	/* altered - modified */
	m_time = nt2unixtime(getu64(fs, fname->mtime));
	/* MFT modified */
	c_time = nt2unixtime(getu64(fs, fname->ctime));
	/* Access */
	a_time = nt2unixtime(getu64(fs, fname->atime));
	if (sec_skew != 0) {
	    fprintf(hFile, "\nAdjusted times:\n");
	    cr_time -= sec_skew;
	    m_time -= sec_skew;
	    a_time -= sec_skew;
	    c_time -= sec_skew;
	    fprintf(hFile, "Created:\t%s", ctime(&cr_time));
	    fprintf(hFile, "File Modified:\t%s", ctime(&m_time));
	    fprintf(hFile, "MFT Modified:\t%s", ctime(&c_time));
	    fprintf(hFile, "Accessed:\t%s", ctime(&a_time));
	    cr_time += sec_skew;
	    m_time += sec_skew;
	    a_time += sec_skew;
	    c_time += sec_skew;
	    fprintf(hFile, "\nOriginal times:\n");
	}

	fprintf(hFile, "Created:\t%s", ctime(&cr_time));
	fprintf(hFile, "File Modified:\t%s", ctime(&m_time));
	fprintf(hFile, "MFT Modified:\t%s", ctime(&c_time));
	fprintf(hFile, "Accessed:\t%s", ctime(&a_time));
    }


    /* $OBJECT_ID Information */
    fs_data = fs_data_lookup_noid(fs_inode->attr, NTFS_ATYPE_OBJID);
    if (fs_data) {
	ntfs_attr_objid *objid = (ntfs_attr_objid *) fs_data->buf;
	uint64_t id1, id2;
	fprintf(hFile, "\n$OBJECT_ID Attribute Values:\n");
	id1 = getu64(fs, objid->objid1);
	id2 = getu64(fs, objid->objid2);
	fprintf(hFile,
	    "Object Id: %.8" PRIx32 "-%.4" PRIx16
	    "-%.4" PRIx16 "-%.4" PRIx16 "-%.12"
	    PRIx64 "\n",
	    (uint32_t) (id2 >> 32) & 0xffffffff,
	    (uint16_t) (id2 >> 16) & 0xffff,
	    (uint16_t) (id2 & 0xffff),
	    (uint16_t) (id1 >> 48) & 0xffff, (uint64_t) (id1 & (uint64_t)
		0x0000ffffffffffffULL));
	/* The rest of the  fields do not always exist.  Check the attr size */
	if (fs_data->size > 16) {
	    id1 = getu64(fs, objid->orig_volid1);
	    id2 = getu64(fs, objid->orig_volid2);
	    fprintf(hFile,
		"Birth Volume Id: %.8" PRIx32 "-%.4"
		PRIx16 "-%.4" PRIx16 "-%.4" PRIx16
		"-%.12" PRIx64 "\n",
		(uint32_t) (id2 >> 32) & 0xffffffff,
		(uint16_t) (id2 >> 16) & 0xffff,
		(uint16_t) (id2 & 0xffff),
		(uint16_t) (id1 >> 48) & 0xffff,
		(uint64_t) (id1 & (uint64_t)
		    0x0000ffffffffffffULL));
	}

	if (fs_data->size > 32) {
	    id1 = getu64(fs, objid->orig_objid1);
	    id2 = getu64(fs, objid->orig_objid2);
	    fprintf(hFile,
		"Birth Object Id: %.8" PRIx32 "-%.4"
		PRIx16 "-%.4" PRIx16 "-%.4" PRIx16
		"-%.12" PRIx64 "\n",
		(uint32_t) (id2 >> 32) & 0xffffffff,
		(uint16_t) (id2 >> 16) & 0xffff,
		(uint16_t) (id2 & 0xffff),
		(uint16_t) (id1 >> 48) & 0xffff,
		(uint64_t) (id1 & (uint64_t)
		    0x0000ffffffffffffULL));
	}

	if (fs_data->size > 48) {
	    id1 = getu64(fs, objid->orig_domid1);
	    id2 = getu64(fs, objid->orig_domid2);
	    fprintf(hFile,
		"Birth Domain Id: %.8" PRIx32 "-%.4"
		PRIx16 "-%.4" PRIx16 "-%.4" PRIx16
		"-%.12" PRIx64 "\n",
		(uint32_t) (id2 >> 32) & 0xffffffff,
		(uint16_t) (id2 >> 16) & 0xffff,
		(uint16_t) (id2 & 0xffff),
		(uint16_t) (id1 >> 48) & 0xffff,
		(uint64_t) (id1 & (uint64_t)
		    0x0000ffffffffffffULL));
	}
    }

    /* Attribute List Information */
    fs_data = fs_data_lookup_noid(fs_inode->attr, NTFS_ATYPE_ATTRLIST);
    if (fs_data) {
	char *buf;
	ntfs_attrlist *list;
	uintptr_t endaddr;
	FS_LOAD_FILE load_file;

	fprintf(hFile, "\n$ATTRIBUTE_LIST Attribute Values:\n");

	/* Get a copy of the attribute list stream  */
	load_file.total = load_file.left = (size_t) fs_data->size;
	load_file.cur = load_file.base = buf = mymalloc(fs_data->size);
	if (buf == NULL) {
	    return 1;
	}

	endaddr = (uintptr_t) buf + fs_data->size;
	if (ntfs_data_walk(ntfs, fs_inode->addr, fs_data,
		0, load_file_action, (void *) &load_file)) {
	    fprintf(hFile, "error reading attribute list buffer\n");
	    tsk_errno = 0;
	    goto egress;
	}

	/* this value should be zero, if not then we didn't read all of the
	 * buffer
	 */
	if (load_file.left > 0) {
	    fprintf(hFile, "error reading attribute list buffer\n");
	    goto egress;
	}

	/* Process the list & print the details */
	for (list = (ntfs_attrlist *) buf;
	    (list) && ((uintptr_t) list < endaddr)
	    && (getu16(fs, list->len) > 0);
	    list =
	    (ntfs_attrlist *) ((uintptr_t) list + getu16(fs, list->len))) {
	    fprintf(hFile,
		"Type: %" PRIu32 "-%" PRIu16
		" \tMFT Entry: %" PRIu64 " \tVCN: %"
		PRIu64 "\n", getu32(fs, list->type),
		getu16(fs, list->id),
		(uint64_t) getu48(fs, list->file_ref),
		getu64(fs, list->start_vcn));
	}
      egress:
	free(buf);
    }

    /* Print all of the attributes */
    fs_data = fs_inode->attr;
    fprintf(hFile, "\nAttributes: \n");
    while ((fs_data)
	&& (fs_data->flags & FS_DATA_INUSE)) {

	char type[512];
	if (ntfs_attrname_lookup(fs, fs_data->type, type, 512)) {
	    fprintf(hFile, "error looking attribute name\n");
	    break;
	}
	fprintf(hFile,
	    "Type: %s (%" PRIu32 "-%" PRIu16
	    ")   Name: %s   %sResident%s%s%s   size: %"
	    PRIuOFF "\n", type, fs_data->type,
	    fs_data->id, fs_data->name,
	    (fs_data->
		flags & FS_DATA_NONRES) ? "Non-" :
	    "",
	    (fs_data->
		flags & FS_DATA_ENC) ? ", Encrypted"
	    : "",
	    (fs_data->
		flags & FS_DATA_COMP) ?
	    ", Compressed" : "",
	    (fs_data->
		flags & FS_DATA_SPAR) ? ", Sparse" : "", fs_data->size);

	/* print the layout if it is non-resident and not "special" */
	if (fs_data->flags & FS_DATA_NONRES) {
	    NTFS_PRINT_ADDR print_addr;
	    print_addr.idx = 0;
	    print_addr.hFile = hFile;
	    if (fs->file_walk(fs, fs_inode, fs_data->type,
		    fs_data->id,
		    (FS_FLAG_FILE_AONLY |
			FS_FLAG_FILE_SLACK),
		    print_addr_act, (void *) &print_addr)) {
		fprintf(hFile, "\nError walking file\n");
		tsk_errno = 0;
	    }
	    if (print_addr.idx != 0)
		fprintf(hFile, "\n");
	}

	fs_data = fs_data->next;
    }

    fs_inode_free(fs_inode);
    return 0;
}



/* JOURNAL CODE - MOVE TO NEW FILE AT SOME POINT */

uint8_t
ntfs_jopen(FS_INFO * fs, INUM_T inum)
{
    tsk_errno = TSK_ERR_FS_FUNC;
    snprintf(tsk_errstr, TSK_ERRSTR_L,
	"NTFS Journal is not yet supported\n");
    return 1;
}

uint8_t
ntfs_jentry_walk(FS_INFO * fs, int flags,
    FS_JENTRY_WALK_FN action, void *ptr)
{
    tsk_errno = TSK_ERR_FS_FUNC;
    snprintf(tsk_errstr, TSK_ERRSTR_L,
	"NTFS Journal is not yet supported\n");
    return 1;
}


uint8_t
ntfs_jblk_walk(FS_INFO * fs, DADDR_T start,
    DADDR_T end, int flags, FS_JBLK_WALK_FN action, void *ptr)
{
    tsk_errno = TSK_ERR_FS_FUNC;
    snprintf(tsk_errstr, TSK_ERRSTR_L,
	"NTFS Journal is not yet supported\n");
    return 1;
}



static void
ntfs_close(FS_INFO * fs)
{
    NTFS_INFO *ntfs = (NTFS_INFO *) fs;
    free((char *) ntfs->mft);
    free((char *) ntfs->fs);
    fs_data_run_free(ntfs->bmap);
    data_buf_free(ntfs->bmap_buf);
    fs_inode_free(ntfs->mft_inode);
    free(fs);
}


/* Return NULL on error */
FS_INFO *
ntfs_open(IMG_INFO * img_info, SSIZE_T offset, uint8_t ftype, uint8_t test)
{
    char *myname = "ntfs_open";
    NTFS_INFO *ntfs;
    FS_INFO *fs;
    unsigned int len;
    SSIZE_T cnt;


    if ((ftype & FSMASK) != NTFS_TYPE) {
	tsk_errno = TSK_ERR_FS_ARG;
	snprintf(tsk_errstr, TSK_ERRSTR_L, "Invalid FS type in ntfs_open");
	return NULL;
    }

    if ((ntfs = (NTFS_INFO *) mymalloc(sizeof(*ntfs))) == NULL) {
	return NULL;
    }
    fs = &(ntfs->fs_info);

    fs->ftype = ftype;
    fs->flags = FS_HAVE_SEQ;
    fs->img_info = img_info;
    fs->offset = offset;

    ntfs->loading_the_MFT = 0;
    ntfs->bmap = NULL;
    ntfs->bmap_buf = NULL;

    /* Read the boot sector */
    len = roundup(sizeof(ntfs_sb), NTFS_DEV_BSIZE);
    ntfs->fs = (ntfs_sb *) mymalloc(len);
    if (ntfs->fs == NULL) {
	free(ntfs);
	return NULL;
    }

    cnt = fs_read_random(fs, (char *) ntfs->fs, len, (OFF_T) 0);
    if (cnt != len) {
        free(ntfs->fs);
        free(ntfs);
	if (cnt != 1) {
	    tsk_errno = TSK_ERR_FS_READ;
	    tsk_errstr[0] = '\0';
	}
	snprintf(tsk_errstr2, TSK_ERRSTR_L,
	    "%s: Error reading boot sector.", myname);
	return NULL;
    }

    /* Check the magic value */
    if (fs_guessu16(fs, ntfs->fs->magic, NTFS_FS_MAGIC)) {
	free(ntfs->fs);
	free(ntfs);
	tsk_errno = TSK_ERR_FS_MAGIC;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "Not a NTFS file system (magic)");
	return NULL;
    }


    /*
     * block calculations : although there are no blocks in ntfs,
     * we are using a cluster as a "block"
     */

    ntfs->ssize_b = getu16(fs, ntfs->fs->ssize);
    if (ntfs->ssize_b % 512) {
	free(ntfs->fs);
	free(ntfs);
	tsk_errno = TSK_ERR_FS_MAGIC;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "Not a NTFS file system (invalid sector size)");
	return NULL;
    }

    if ((ntfs->fs->csize != 0x01) &&
	(ntfs->fs->csize != 0x02) &&
	(ntfs->fs->csize != 0x04) &&
	(ntfs->fs->csize != 0x08) &&
	(ntfs->fs->csize != 0x10) &&
	(ntfs->fs->csize != 0x20) && (ntfs->fs->csize != 0x40)
	&& (ntfs->fs->csize != 0x80)) {

	free(ntfs->fs);
	free(ntfs);
	tsk_errno = TSK_ERR_FS_MAGIC;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "Not a NTFS file system (invalid cluster size)");
	return NULL;
    }

    ntfs->csize_b = ntfs->fs->csize * ntfs->ssize_b;
    fs->first_block = 0;
    /* This field is defined as 64-bits but according to the
     * NTFS drivers in Linux, windows only uses 32-bits
     */
    fs->block_count =
	(DADDR_T) getu32(fs, ntfs->fs->vol_size_s) / ntfs->fs->csize;
    fs->last_block = fs->block_count - 1;
    fs->block_size = ntfs->csize_b;
    fs->dev_bsize = NTFS_DEV_BSIZE;
    if (ntfs->fs->mft_rsize_c > 0)
	ntfs->mft_rsize_b = ntfs->fs->mft_rsize_c * ntfs->csize_b;
    else
	/* if the mft_rsize_c is not > 0, then it is -log2(rsize_b) */
	ntfs->mft_rsize_b = 1 << -ntfs->fs->mft_rsize_c;
    if (ntfs->mft_rsize_b % 512) {
	free(ntfs->fs);
	free(ntfs);
	tsk_errno = TSK_ERR_FS_MAGIC;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "Not a NTFS file system (invalid MFT entry size)");
	return NULL;
    }

    if (ntfs->fs->idx_rsize_c > 0)
	ntfs->idx_rsize_b = ntfs->fs->idx_rsize_c * ntfs->csize_b;
    else
	/* if the idx_rsize_c is not > 0, then it is -log2(rsize_b) */
	ntfs->idx_rsize_b = 1 << -ntfs->fs->idx_rsize_c;
    if (ntfs->idx_rsize_b % 512) {
	free(ntfs->fs);
	free(ntfs);
	tsk_errno = TSK_ERR_FS_MAGIC;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "Not a NTFS file system (invalid idx record size)");
	return NULL;
    }

    ntfs->root_mft_addr = getu64(fs, ntfs->fs->mft_clust) * ntfs->csize_b;
    if (getu64(fs, ntfs->fs->mft_clust) > fs->last_block) {
	free(ntfs);
	free(ntfs->fs);
	tsk_errno = TSK_ERR_FS_MAGIC;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "Not a NTFS file system (invalid starting MFT clust)");
	return NULL;
    }

    /*
     * Other initialization: caches, callbacks.
     */
    fs->inode_walk = ntfs_inode_walk;
    fs->block_walk = ntfs_block_walk;
    fs->file_walk = ntfs_file_walk;
    fs->inode_lookup = ntfs_inode_lookup;
    fs->dent_walk = ntfs_dent_walk;
    fs->fsstat = ntfs_fsstat;
    fs->fscheck = ntfs_fscheck;
    fs->istat = ntfs_istat;
    fs->close = ntfs_close;

    /*
     * inode
     */

    /* allocate the buffer to hold mft entries */
    ntfs->mft = (ntfs_mft *) mymalloc(ntfs->mft_rsize_b);
    if (ntfs->mft == NULL) {
	free(ntfs->fs);
	free(ntfs);
	return NULL;
    }

    ntfs->mnum = 0;
    fs->root_inum = NTFS_ROOTINO;
    fs->first_inum = NTFS_FIRSTINO;
    fs->last_inum = NTFS_LAST_DEFAULT_INO;
    ntfs->mft_data = NULL;

    /* load the data run for the MFT table into ntfs->mft */
    ntfs->loading_the_MFT = 1;
    if (ntfs_dinode_lookup(ntfs, ntfs->mft, NTFS_MFT_MFT)) {
	free(ntfs->mft);
	free(ntfs->fs);
	free(ntfs);
	return NULL;
    }

    ntfs->mft_inode = fs_inode_alloc(NTFS_NDADDR, NTFS_NIADDR);
    if (ntfs->mft_inode == NULL) {
	free(ntfs->mft);
	free(ntfs->fs);
	free(ntfs);
	return NULL;
    }
    if (ntfs_dinode_copy(ntfs, ntfs->mft_inode)) {
	fs_inode_free(ntfs->mft_inode);
	free(ntfs->mft);
	free(ntfs->fs);
	free(ntfs);
	return NULL;

    }
    /* cache the data attribute 
     *
     * This will likely be done already by proc_attrseq, but this
     * should be quick
     */
    ntfs->mft_data =
	fs_data_lookup_noid(ntfs->mft_inode->attr, NTFS_ATYPE_DATA);
    if (!ntfs->mft_data) {
	fs_inode_free(ntfs->mft_inode);
	free(ntfs->mft);
	free(ntfs->fs);
	free(ntfs);
	strncat(tsk_errstr2, " - Data Attribute not found in $MFT",
	    TSK_ERRSTR_L - strlen(tsk_errstr2));
	return NULL;
    }

    /* Get the inode count based on the table size */
    fs->inum_count = ntfs->mft_data->size / ntfs->mft_rsize_b;
    fs->last_inum = fs->inum_count - 1;

    /* reset the flag that we are no longer loading $MFT */
    ntfs->loading_the_MFT = 0;

    /* load the version of the file system */
    if (ntfs_load_ver(ntfs)) {
	fs_inode_free(ntfs->mft_inode);
	free(ntfs->mft);
	free(ntfs->fs);
	free(ntfs);
	return NULL;
    }

    /* load the data block bitmap data run into ntfs_info */
    if (ntfs_load_bmap(ntfs)) {
	fs_inode_free(ntfs->mft_inode);
	free(ntfs->mft);
	free(ntfs->fs);
	free(ntfs);
	return NULL;
    }

    /* set this to NULL and it will be loaded if needed */
    ntfs->attrdef = NULL;
    fs->jblk_walk = ntfs_jblk_walk;
    fs->jentry_walk = ntfs_jentry_walk;
    fs->jopen = ntfs_jopen;
    fs->journ_inum = 0;

    if (verbose) {
	fprintf(stderr,
	    "ssize: %" PRIu16
	    " csize: %d serial: %" PRIx64 "\n",
	    getu16(fs, ntfs->fs->ssize),
	    ntfs->fs->csize, getu64(fs, ntfs->fs->serial));
	fprintf(stderr,
	    "mft_rsize: %d idx_rsize: %d vol: %d mft: %"
	    PRIu64 " mft_mir: %" PRIu64 "\n",
	    ntfs->mft_rsize_b, ntfs->idx_rsize_b,
	    (int) fs->block_count, getu64(fs,
		ntfs->fs->mft_clust), getu64(fs, ntfs->fs->mftm_clust));
    }

    return fs;
}
