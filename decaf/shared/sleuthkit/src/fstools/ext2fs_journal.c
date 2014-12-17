/*
   @@@ UNALLOC only if seq is less - alloc can be less than block if it wrapped around ...
** ext2fs_journal
** The Sleuth Kit 
**
** $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
**
** Journaling code for EXT3FS image
**
** Brian Carrier [carrier@sleuthkit.org]
** Copyright (c) 2006 Brian Carrier, Basis Technology.  All Rights reserved
** Copyright (c) 2004-2005 Brian Carrier.  All rights reserved 
**
**
** This software is distributed under the Common Public License 1.0
**
*/

#include "fs_tools.h"
#include "ext2fs.h"



/* Everything in the journal is in big endian */
#define big_getu32(x)	\
	(uint32_t)((((uint8_t *)x)[3] <<  0) + \
	(((uint8_t *)x)[2] <<  8) + \
	(((uint8_t *)x)[1] << 16) + \
	(((uint8_t *)x)[0] << 24) )


/*
 *
 */

static uint8_t
load_sb_action(FS_INFO * fs, DADDR_T addr, char *buf, unsigned int size,
    int flags, void *ptr)
{
    ext2fs_journ_sb *sb;
    EXT2FS_INFO *ext2fs = (EXT2FS_INFO *) fs;
    EXT2FS_JINFO *jinfo = ext2fs->jinfo;

    if (size < 1024) {
	tsk_errno = TSK_ERR_FS_FUNC;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "FS block size is less than 1024, not supported in journal yet");
	return WALK_ERROR;
    }

    sb = (ext2fs_journ_sb *) buf;

    if (big_getu32(sb->magic) != EXT2_JMAGIC) {
	tsk_errno = TSK_ERR_FS_MAGIC;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "Journal inode %" PRIuINUM
	    " does not have a valid magic value: %" PRIx32,
	    jinfo->j_inum, big_getu32(sb->magic));
	return WALK_ERROR;
    }

    jinfo->bsize = big_getu32(sb->bsize);
    jinfo->first_block = big_getu32(sb->first_blk);
    jinfo->last_block = big_getu32(sb->num_blk) - 1;
    jinfo->start_blk = big_getu32(sb->start_blk);
    jinfo->start_seq = big_getu32(sb->start_seq);

    return WALK_STOP;
}

/* Place journal data in *fs
 *
 * Return 0 on success and 1 on error 
 * */
uint8_t
ext2fs_jopen(FS_INFO * fs, INUM_T inum)
{
    EXT2FS_INFO *ext2fs = (EXT2FS_INFO *) fs;
    EXT2FS_JINFO *jinfo;

    if (!fs) {
	tsk_errno = TSK_ERR_FS_ARG;
	snprintf(tsk_errstr, TSK_ERRSTR_L, "ext2fs_jopen: fs is null");
	return 1;
    }

    ext2fs->jinfo = jinfo =
	(EXT2FS_JINFO *) mymalloc(sizeof(EXT2FS_JINFO));
    if (jinfo == NULL) {
	return 1;
    }
    jinfo->j_inum = inum;

    jinfo->fs_inode = fs->inode_lookup(fs, inum);
    if (!jinfo->fs_inode) {
	free(jinfo);
	return 1;
//      error("error finding journal inode %" PRIu32, inum);
    }

    if (fs->file_walk(fs, jinfo->fs_inode, 0, 0, FS_FLAG_FILE_NOID,
	    load_sb_action, NULL)) {
	tsk_errno = TSK_ERR_FS_FWALK;
	snprintf(tsk_errstr, TSK_ERRSTR_L, "Error loading ext3 journal");
	fs_inode_free(jinfo->fs_inode);
	free(jinfo);
	return 1;
    }

    if (verbose)
	fprintf(stderr,
	    "journal opened at inode %" PRIuINUM " bsize: %" PRIu32
	    " First JBlk: %" PRIuDADDR " Last JBlk: %" PRIuDADDR "\n",
	    inum, jinfo->bsize, jinfo->first_block, jinfo->last_block);

    return 0;
}


/* Limitations: does not use the action or any flags 
 *
 * return 0 on success and 1 on error
 * */
uint8_t
ext2fs_jentry_walk(FS_INFO * fs, int flags, FS_JENTRY_WALK_FN action,
    void *ptr)
{
    EXT2FS_INFO *ext2fs = (EXT2FS_INFO *) fs;
    EXT2FS_JINFO *jinfo = ext2fs->jinfo;
    char *journ;
    FS_LOAD_FILE buf1;
    DADDR_T i;
    int b_desc_seen = 0;


    if (jinfo == NULL) {
	tsk_errno = TSK_ERR_FS_ARG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "ext2fs_jentry_walk: journal is not open");
	return 1;
    }

    if (jinfo->fs_inode->size != (jinfo->last_block + 1) * jinfo->bsize) {
	tsk_errno = TSK_ERR_FS_ARG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "ext2fs_jentry_walk: journal file size is different from \nsize reported in journal super block");
	return 1;
    }

    /* Load the journal into a buffer */
    buf1.left = buf1.total = jinfo->fs_inode->size;
    journ = buf1.cur = buf1.base = mymalloc(buf1.left);
    if (journ == NULL) {
	return 1;
    }

    if (fs->file_walk(fs, jinfo->fs_inode, 0, 0, FS_FLAG_FILE_NOID,
	    load_file_action, (void *) &buf1)) {
	free(journ);
	return 1;
    }

    if (buf1.left > 0) {
	tsk_errno = TSK_ERR_FS_FWALK;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "ext2fs_jentry_walk: Buffer not fully copied");
	free(journ);
	return 1;
    }


    /* Process the journal 
     * Cycle through each block
     */
    printf("JBlk\tDescriptrion\n");

    /* Note that 'i' is incremented when we find a descriptor block and
     * process its contents. */
    for (i = 0; i < jinfo->last_block; i++) {
	ext2fs_journ_head *head;


	/* if there is no magic, then it is a normal block 
	 * These should be accounted for when we see its corresponding
	 * descriptor.  We get the 'unknown' when its desc has
	 * been reused, it is in the next batch to be overwritten,
	 * or if it has not been used before
	 */
	head = (ext2fs_journ_head *) & journ[i * jinfo->bsize];
	if (big_getu32(head->magic) != EXT2_JMAGIC) {
	    if (i < jinfo->first_block) {
		printf("%" PRIuDADDR ":\tUnused\n", i);
	    }

#if 0
	    /* For now, we ignore the case of the iitial entries before a descriptor, it is too hard ... */

	    else if (b_desc_seen == 0) {
		ext2fs_journ_head *head2 = NULL;
		DADDR_T a;
		int next_head = 0, next_seq = 0;
		ext2fs_journ_dentry *dentry;

		/* This occurs when the log cycled around 
		 * We need to find out where the descriptor is
		 * and where we need to end */
		b_desc_seen = 1;

		for (a = i; a < jinfo->last_block; a++) {
		    head2 =
			(ext2fs_journ_head *) & journ[a * jinfo->bsize];
		    if ((big_getu32(head2->magic) == EXT2_JMAGIC)) {
			next_head = a;
			next_seq = big_getu32(head2->entry_seq);
			break;
		    }

		}
		if (next_head == 0) {
		    printf("%" PRIuDADDR ":\tFS Block Unknown\n", i);
		}

		/* Find the last descr in the journ */
		for (a = jinfo->last_block; a > i; a--) {
		    head2 =
			(ext2fs_journ_head *) & journ[a * jinfo->bsize];
		    if ((big_getu32(head2->magic) == EXT2_JMAGIC)
			&& (big_getu32(head2->entry_type) ==
			    EXT2_J_ETYPE_DESC)
			&& (next_seq == big_getu32(head2->entry_seq))) {
			break;

// @@@@ We should abort if we reach a commit before  descriptor

		    }
		}

		/* We did not find a descriptor in the journ! 
		 * print unknown for the rest of the journ
		 */
		if (a == i) {
		    printf("%" PRIuDADDR ":\tFS Block Unknown\n", i);
		    continue;
		}


		dentry =
		    (ext2fs_journ_dentry *) ((uintptr_t) head2 +
		    sizeof(ext2fs_journ_head));;


		/* Cycle through the descriptor entries */
		while ((uintptr_t) dentry <=
		    ((uintptr_t) head2 + jinfo->bsize -
			sizeof(ext2fs_journ_head))) {


		    /* Only start to look after the index in the desc has looped */
		    if (++a <= jinfo->last_block) {
			ext2fs_journ_head *head3;

			/* Look at the block that this entry refers to */
			head3 =
			    (ext2fs_journ_head *) & journ[i *
			    jinfo->bsize];
			if ((big_getu32(head3->magic) == EXT2_JMAGIC)) {
			    i--;
			    break;
			}

			/* If it doesn't have the magic, then it is a
			 * journal entry and we print the FS info */
			printf("%" PRIuDADDR ":\tFS Block %" PRIu32 "\n",
			    i, big_getu32(dentry->fs_blk));

			/* Our counter is over the end of the journ */
			if (++i > jinfo->last_block)
			    break;

		    }

		    /* Increment to the next */
		    if (big_getu32(dentry->flag) & EXT2_J_DENTRY_LAST)
			break;

		    /* If the SAMEID value is set, then we advance by the size of the entry, otherwise add 16 for the ID */
		    else if (big_getu32(dentry->flag) &
			EXT2_J_DENTRY_SAMEID)
			dentry =
			    (ext2fs_journ_dentry *) ((uintptr_t) dentry +
			    sizeof(ext2fs_journ_dentry));

		    else
			dentry =
			    (ext2fs_journ_dentry *) ((uintptr_t) dentry +
			    sizeof(ext2fs_journ_dentry)
			    + 16);

		}
	    }
#endif
	    else {
		printf("%" PRIuDADDR ":\tUnallocated FS Block Unknown\n",
		    i);
	    }
	}

	/* The super block */
	else if ((big_getu32(head->entry_type) == EXT2_J_ETYPE_SB1) ||
	    (big_getu32(head->entry_type) == EXT2_J_ETYPE_SB2)) {
	    printf("%" PRIuDADDR ":\tSuperblock (seq: %" PRIu32 ")\n", i,
		big_getu32(head->entry_seq));
	}

	/* Revoke Block */
	else if (big_getu32(head->entry_type) == EXT2_J_ETYPE_REV) {
	    printf("%" PRIuDADDR ":\t%sRevoke Block (seq: %" PRIu32 ")\n",
		i, ((i < jinfo->start_blk)
		    || (big_getu32(head->entry_seq) <
			jinfo->
			start_seq)) ? "Unallocated " : "Allocated ",
		big_getu32(head->entry_seq));
	}

	/* The commit is the end of the entries */
	else if (big_getu32(head->entry_type) == EXT2_J_ETYPE_COM) {
	    printf("%" PRIuDADDR ":\t%sCommit Block (seq: %" PRIu32 ")\n",
		i, ((i < jinfo->start_blk)
		    || (big_getu32(head->entry_seq) <
			jinfo->
			start_seq)) ? "Unallocated " : "Allocated ",
		big_getu32(head->entry_seq));
	}

	/* The descriptor describes the FS blocks that follow it */
	else if (big_getu32(head->entry_type) == EXT2_J_ETYPE_DESC) {
	    ext2fs_journ_dentry *dentry;
	    ext2fs_journ_head *head2;
	    int unalloc = 0;

	    b_desc_seen = 1;


	    /* Is this an unallocated journ block or sequence */
	    if ((i < jinfo->start_blk) ||
		(big_getu32(head->entry_seq) < jinfo->start_seq))
		unalloc = 1;

	    printf("%" PRIuDADDR ":\t%sDescriptor Block (seq: %" PRIu32
		")\n", i, (unalloc) ? "Unallocated " : "Allocated ",
		big_getu32(head->entry_seq));

	    dentry =
		(ext2fs_journ_dentry *) ((uintptr_t) head +
		sizeof(ext2fs_journ_head));;

	    /* Cycle through the descriptor entries to account for the journal blocks */
	    while ((uintptr_t) dentry <=
		((uintptr_t) head + jinfo->bsize -
		    sizeof(ext2fs_journ_head))) {


		/* Our counter is over the end of the journ */
		if (++i > jinfo->last_block)
		    break;


		/* Look at the block that this entry refers to */
		head2 = (ext2fs_journ_head *) & journ[i * jinfo->bsize];
		if ((big_getu32(head2->magic) == EXT2_JMAGIC) &&
		    (big_getu32(head2->entry_seq) >=
			big_getu32(head->entry_seq))) {
		    i--;
		    break;
		}

		/* If it doesn't have the magic, then it is a
		 * journal entry and we print the FS info */
		printf("%" PRIuDADDR ":\t%sFS Block %" PRIu32 "\n", i,
		    (unalloc) ? "Unallocated " : "Allocated ",
		    big_getu32(dentry->fs_blk));

		/* Increment to the next */
		if (big_getu32(dentry->flag) & EXT2_J_DENTRY_LAST)
		    break;

		/* If the SAMEID value is set, then we advance by the size of the entry, otherwise add 16 for the ID */
		else if (big_getu32(dentry->flag) & EXT2_J_DENTRY_SAMEID)
		    dentry =
			(ext2fs_journ_dentry *) ((uintptr_t) dentry +
			sizeof(ext2fs_journ_dentry));

		else
		    dentry =
			(ext2fs_journ_dentry *) ((uintptr_t) dentry +
			sizeof(ext2fs_journ_dentry) + 16);
	    }
	}
    }

    free(journ);
    return 0;
}





/* 
 * Limitations for 1st version: start must equal end and action is ignored
 *
 * Return 0 on success and 1 on error
 */
uint8_t
ext2fs_jblk_walk(FS_INFO * fs, DADDR_T start, DADDR_T end, int flags,
    FS_JBLK_WALK_FN action, void *ptr)
{
    EXT2FS_INFO *ext2fs = (EXT2FS_INFO *) fs;
    EXT2FS_JINFO *jinfo = ext2fs->jinfo;
    char *journ;
    FS_LOAD_FILE buf1;
    DADDR_T i;
    ext2fs_journ_head *head;

    if (jinfo == NULL) {
	tsk_errno = TSK_ERR_FS_ARG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "ext2fs_jblk_walk: journal is not open");
	return 1;
    }

    if (jinfo->last_block < end) {
	tsk_errno = TSK_ERR_FS_WALK_RNG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "ext2fs_jblk_walk: end is too large ");
	return 1;
    }

    if (start != end) {
	tsk_errno = TSK_ERR_FS_ARG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "ext2fs_blk_walk: only start == end is currently supported");
	return 1;
    }

    if (jinfo->fs_inode->size != (jinfo->last_block + 1) * jinfo->bsize) {
	tsk_errno = TSK_ERR_FS_FUNC;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "ext2fs_jblk_walk: journal file size is different from size reported in journal super block");
	return 1;
    }


    /* Load into buffer and then process it 
     * Only get the minimum needed
     */
    buf1.left = buf1.total = (end + 1) * jinfo->bsize;
    journ = buf1.cur = buf1.base = mymalloc(buf1.left);
    if (journ == NULL) {
	return 1;
    }

    if (fs->file_walk(fs, jinfo->fs_inode, 0, 0, FS_FLAG_FILE_NOID,
	    load_file_action, (void *) &buf1)) {
	free(journ);
	return 1;
    }

    if (buf1.left > 0) {
	tsk_errno = TSK_ERR_FS_FWALK;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "ext2fs_jblk_walk: Buffer not fully copied");
	free(journ);
	return 1;
    }

    head = (ext2fs_journ_head *) & journ[end * jinfo->bsize];


    /* Check if our target block is a journal data structure.
     * 
     * If not, 
     * we need to look for its descriptor to see if it has been
     * escaped
     */
    if (big_getu32(head->magic) != EXT2_JMAGIC) {

	/* cycle backwards until we find a desc block */
	for (i = end - 1; i >= 0; i--) {
	    ext2fs_journ_dentry *dentry;
	    int diff;

	    head = (ext2fs_journ_head *) & journ[i * jinfo->bsize];

	    if (big_getu32(head->magic) != EXT2_JMAGIC)
		continue;

	    /* If we get a commit, then any desc we find will not
	     * be for our block, so forget about it */
	    if (big_getu32(head->entry_type) == EXT2_J_ETYPE_COM)
		break;

	    /* Skip any other data structure types */
	    if (big_getu32(head->entry_type) != EXT2_J_ETYPE_DESC)
		continue;

	    /* We now have the previous descriptor 
	     *
	     * NOTE: We have no clue if this is the correct 
	     * descriptor if it is not the current 'run' of 
	     * transactions, but this is the best we can do
	     */
	    diff = end - i;

	    dentry =
		(ext2fs_journ_dentry *) (&journ[i * jinfo->bsize] +
		sizeof(ext2fs_journ_head));

	    while ((uintptr_t) dentry <=
		((uintptr_t) & journ[(i + 1) * jinfo->bsize] -
		    sizeof(ext2fs_journ_head))) {

		if (--diff == 0) {
		    if (big_getu32(dentry->flag) & EXT2_J_DENTRY_ESC) {
			journ[end * jinfo->bsize] = 0xC0;
			journ[end * jinfo->bsize + 1] = 0x3B;
			journ[end * jinfo->bsize + 2] = 0x39;
			journ[end * jinfo->bsize + 3] = 0x98;
		    }
		    break;
		}

		/* If the SAMEID value is set, then we advance by the size of the entry, otherwise add 16 for the ID */
		if (big_getu32(dentry->flag) & EXT2_J_DENTRY_SAMEID)
		    dentry =
			(ext2fs_journ_dentry *) ((uintptr_t) dentry +
			sizeof(ext2fs_journ_dentry));
		else
		    dentry =
			(ext2fs_journ_dentry *) ((uintptr_t) dentry +
			sizeof(ext2fs_journ_dentry) + 16);

	    }
	    break;
	}
    }

    if (fwrite(&journ[end * jinfo->bsize], jinfo->bsize, 1, stdout) != 1) {
	tsk_errno = TSK_ERR_FS_WRITE;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "ext2fs_jblk_walk: error writing buffer block");
	free(journ);
	return 1;
    }

    free(journ);
    return 0;
}
