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
** Copyright (c) 2002-2003 Brian Carrier, @stake Inc.  All rights reserved
** 
** Copyright (c) 1997,1998,1999, International Business Machines          
** Corporation and others. All Rights Reserved.
*/

/* TCT 
 * LICENSE
 *	This software is distributed under the IBM Public License.
 * AUTHOR(S)
 *	Wietse Venema
 *	IBM T.J. Watson Research
 *	P.O. Box 704
 *	Yorktown Heights, NY 10598, USA
 --*/

#include "fs_tools.h"
#include "fs_types.h"
#include "ext2fs.h"


/* ext2fs_group_load - load block group descriptor into cache 
 *
 * return 1 on error and 0 on success
 *
 * */
static uint8_t
ext2fs_group_load(EXT2FS_INFO * ext2fs, EXT2_GRPNUM_T grp_num)
{
    ext2fs_gd *gd;
    OFF_T offs;
    SSIZE_T cnt;

    /*
     * Sanity check
     */
    if (grp_num < 0 || grp_num >= ext2fs->groups_count) {
	tsk_errno = TSK_ERR_FS_ARG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "ext2fs_group_load: invalid cylinder group number: %"
	    PRI_EXT2GRP "", grp_num);
	tsk_errstr2[0] = '\0';
	return 1;
    }


    if (ext2fs->grp_buf == NULL) {
	if ((ext2fs->grp_buf =
		(ext2fs_gd *) mymalloc(sizeof(ext2fs_gd))) == NULL) {
	    return 1;
	}
    }
    else if (ext2fs->grp_num == grp_num) {
	return 0;
    }

    gd = ext2fs->grp_buf;

    /*
     * We're not reading group descriptors often, so it is OK to do small
     * reads instead of cacheing group descriptors in a large buffer.
     */
    offs = ext2fs->groups_offset + grp_num * sizeof(ext2fs_gd);
    cnt = fs_read_random(&ext2fs->fs_info, (char *) gd,
	sizeof(ext2fs_gd), offs);
    if (cnt != sizeof(ext2fs_gd)) {
	if (cnt != -1) {
	    tsk_errno = TSK_ERR_FS_READ;
	    tsk_errstr[0] = '\0';
	}
	snprintf(tsk_errstr2, TSK_ERRSTR_L,
	    "ext2fs_group_load: Group descriptor %d at %"
	    PRIuOFF, grp_num, offs);
	return 1;
    }
    ext2fs->grp_num = grp_num;

    if (verbose) {
	FS_INFO *fs = (FS_INFO *) & ext2fs->fs_info;
	fprintf(stderr,
	    "\tgroup %" PRI_EXT2GRP ": %" PRIu16 "/%" PRIu16
	    " free blocks/inodes\n", grp_num, getu16(fs,
		gd->
		bg_free_blocks_count),
	    getu16(fs, gd->bg_free_inodes_count));
    }

    return 0;
}

/* ext2fs_print_map - print a bitmap */

static void
ext2fs_print_map(UCHAR * map, int len)
{
    int i;

    for (i = 0; i < len; i++) {
	if (i > 0 && i % 10 == 0)
	    putc('|', stderr);
	putc(isset(map, i) ? '1' : '.', stderr);
    }
    putc('\n', stderr);
}


/* ext2fs_bmap_load - look up block bitmap & load into cache 
 *
 * return 1 on error and 0 on success
 * */
static uint8_t
ext2fs_bmap_load(EXT2FS_INFO * ext2fs, EXT2_GRPNUM_T grp_num)
{
    FS_INFO *fs = (FS_INFO *) & ext2fs->fs_info;
    SSIZE_T cnt;

    /*
     * Look up the group descriptor info.  The load will do the sanity check.
     */
    if ((ext2fs->grp_buf == NULL) || (ext2fs->grp_num != grp_num)) {
	if (ext2fs_group_load(ext2fs, grp_num)) {
	    return 1;
	}
    }

    if (ext2fs->bmap_buf == NULL) {
	if ((ext2fs->bmap_buf =
		(unsigned char *) mymalloc(fs->block_size)) == NULL) {
	    return 1;
	}
    }
    else if (ext2fs->bmap_grp_num == grp_num)
	return 0;

    /*
     * Look up the block allocation bitmap.
     */
    if (getu32(fs, ext2fs->grp_buf->bg_block_bitmap) > fs->last_block) {
	tsk_errno = TSK_ERR_FS_BLK_NUM;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "ext2fs_bmap_load: Block too large for image: %" PRIu32
	    "", getu32(fs, ext2fs->grp_buf->bg_block_bitmap));
	tsk_errstr2[0] = '\0';
	return 1;
    }

    cnt = fs_read_block_nobuf(fs, (char *) ext2fs->bmap_buf,
	ext2fs->fs_info.block_size,
	(DADDR_T) getu32(fs, ext2fs->grp_buf->bg_block_bitmap));

    if (cnt != ext2fs->fs_info.block_size) {
	if (cnt != -1) {
	    tsk_errno = TSK_ERR_FS_READ;
	    tsk_errstr[0] = '\0';
	}
	snprintf(tsk_errstr2, TSK_ERRSTR_L,
	    "ext2fs_bmap_load: Bitmap group %d at %"
	    PRIu32, grp_num, getu32(fs, ext2fs->grp_buf->bg_block_bitmap));
    }

    ext2fs->bmap_grp_num = grp_num;

    if (verbose > 1)
	ext2fs_print_map(ext2fs->bmap_buf,
	    getu32(fs, ext2fs->fs->s_blocks_per_group));

    return 0;
}


/* ext2fs_imap_load - look up inode bitmap & load into cache 
 *
 * return 0 on success and 1 on error
 * */
static uint8_t
ext2fs_imap_load(EXT2FS_INFO * ext2fs, EXT2_GRPNUM_T grp_num)
{
    FS_INFO *fs = (FS_INFO *) & ext2fs->fs_info;
    SSIZE_T cnt;

    /*
     * Look up the group descriptor info.
     */
    if ((ext2fs->grp_buf == NULL) || (ext2fs->grp_num != grp_num)) {
	if (ext2fs_group_load(ext2fs, grp_num)) {
	    return 1;
	}
    }

    /* Allocate the cache buffer and exit if map is already loaded */
    if (ext2fs->imap_buf == NULL) {
	if ((ext2fs->imap_buf =
		(UCHAR *) mymalloc(fs->block_size)) == NULL) {
	    return 1;
	}
    }
    else if (ext2fs->imap_grp_num == grp_num) {
	return 0;
    }

    /*
     * Look up the inode allocation bitmap.
     */
    if (getu32(fs, ext2fs->grp_buf->bg_inode_bitmap) > fs->last_block) {
	tsk_errno = TSK_ERR_FS_BLK_NUM;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "ext2fs_imap_load: Block too large for image: %" PRIu32
	    "", getu32(fs, ext2fs->grp_buf->bg_inode_bitmap));
    }

    cnt = fs_read_block_nobuf(fs,
	(char *) ext2fs->imap_buf,
	ext2fs->fs_info.block_size,
	(DADDR_T) getu32(fs, ext2fs->grp_buf->bg_inode_bitmap));

    if (cnt != ext2fs->fs_info.block_size) {
	if (cnt != -1) {
	    tsk_errno = TSK_ERR_FS_READ;
	    tsk_errstr[0] = '\0';
	}
	snprintf(tsk_errstr2, TSK_ERRSTR_L,
	    "ext2fs_imap_load: Inode bitmap %d at %"
	    PRIu32, grp_num, getu32(fs, ext2fs->grp_buf->bg_inode_bitmap));
    }

    ext2fs->imap_grp_num = grp_num;
    if (verbose > 1)
	ext2fs_print_map(ext2fs->imap_buf,
	    getu32(fs, ext2fs->fs->s_inodes_per_group));

    return 0;
}

/* ext2fs_dinode_load - look up disk inode & load into cache 
 *
 * return 1 on error and 0 on success
 * */

static uint8_t
ext2fs_dinode_load(EXT2FS_INFO * ext2fs, INUM_T inum)
{
    ext2fs_inode *dino;
    EXT2_GRPNUM_T grp_num;
    OFF_T addr;
    SSIZE_T cnt;
    INUM_T rel_inum;
    FS_INFO *fs = (FS_INFO *) & ext2fs->fs_info;

    /*
     * Sanity check.
     */
    if ((inum < fs->first_inum) || (inum > fs->last_inum)) {
	tsk_errno = TSK_ERR_FS_INODE_NUM;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "ext2fs_dinode_load: address: %" PRIuINUM, inum);
	tsk_errstr2[0] = '\0';
	return 1;
    }

    /* Allocate the buffer or return if already loaded */
    if (ext2fs->dino_buf == NULL) {
	if ((ext2fs->dino_buf =
		(ext2fs_inode *) mymalloc(ext2fs->inode_size)) == NULL) {
	    return 1;
	}
    }
    else if (ext2fs->dino_inum == inum) {
	return 0;
    }

    dino = ext2fs->dino_buf;

    /*
     * Look up the group descriptor for this inode. 
     */
    grp_num = (inum - fs->first_inum) /
	getu32(fs, ext2fs->fs->s_inodes_per_group);

    if ((ext2fs->grp_buf == NULL) || (ext2fs->grp_num != grp_num)) {
	if (ext2fs_group_load(ext2fs, grp_num)) {
	    return 1;
	}
    }

    /*
     * Look up the inode table block for this inode.
     */
    rel_inum =
	(inum - 1) - getu32(fs, ext2fs->fs->s_inodes_per_group) * grp_num;
    addr = (OFF_T) getu32(fs,
	ext2fs->grp_buf->bg_inode_table) *
	(OFF_T) fs->block_size + rel_inum * (OFF_T) ext2fs->inode_size;

    cnt = fs_read_random(fs, (char *) dino, ext2fs->inode_size, addr);
    if (cnt != ext2fs->inode_size) {
	if (cnt != -1) {
	    tsk_errno = TSK_ERR_FS_READ;
	    tsk_errstr[0] = '\0';
	}
	snprintf(tsk_errstr2, TSK_ERRSTR_L,
	    "ext2fs_dinode_load: Inode %" PRIuINUM
	    " from %" PRIuOFF, inum, addr);
	return 1;
    }

    ext2fs->dino_inum = inum;
    if (verbose)
	fprintf(stderr,
	    "%" PRIuINUM " m/l/s=%o/%d/%" PRIuOFF
	    " u/g=%d/%d macd=%lu/%lu/%lu/%lu\n",
	    inum, getu16(fs, dino->i_mode),
	    getu16(fs, dino->i_nlink),
	    (getu32(fs, dino->i_size) +
		(getu16(fs, dino->i_mode) & EXT2_IN_REG) ? (uint64_t)
		getu32(fs, dino->i_size_high) << 32 : 0), getu16(fs,
		dino->
		i_uid) +
	    (getu16(fs, dino->i_uid_high) << 16), getu16(fs,
		dino->i_gid) +
	    (getu16(fs, dino->i_gid_high) << 16), (ULONG) getu32(fs,
		dino->
		i_mtime),
	    (ULONG) getu32(fs, dino->i_atime), (ULONG) getu32(fs,
		dino->i_ctime), (ULONG) getu32(fs, dino->i_dtime));

    return 0;
}

/* ext2fs_dinode_copy - copy cached disk inode into generic inode 
 *
 * returns 1 on error and 0 on success
 * */
static uint8_t
ext2fs_dinode_copy(EXT2FS_INFO * ext2fs, FS_INODE * fs_inode)
{
    int i;
    ext2fs_inode *in = ext2fs->dino_buf;
    FS_INFO *fs = (FS_INFO *) & ext2fs->fs_info;
    ext2fs_sb *sb = ext2fs->fs;
    EXT2_GRPNUM_T grp_num;
    INUM_T ibase = 0;

    if (ext2fs->dino_buf == NULL) {
	tsk_errno = TSK_ERR_FS_ARG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "ext2fs_dinode_copy: dino_buf is NULL");
	tsk_errstr2[0] = '\0';
	return 1;
    }

    fs_inode->mode = getu16(fs, in->i_mode);
    fs_inode->nlink = getu16(fs, in->i_nlink);

    fs_inode->size = getu32(fs, in->i_size);

    fs_inode->addr = ext2fs->dino_inum;

    /* the general size value in the inode is only 32-bits,
     * but the i_dir_acl value is used for regular files to 
     * hold the upper 32-bits 
     *
     * The RO_COMPAT_LARGE_FILE flag in the super block will identify
     * if there are any large files in the file system
     */
    if ((fs_inode->mode & EXT2_IN_REG) &&
	(getu32(fs, sb->s_feature_ro_compat) &
	    EXT2FS_FEATURE_RO_COMPAT_LARGE_FILE)) {
	fs_inode->size += ((uint64_t) getu32(fs, in->i_size_high) << 32);
    }

    fs_inode->uid =
	getu16(fs, in->i_uid) + (getu16(fs, in->i_uid_high) << 16);
    fs_inode->gid =
	getu16(fs, in->i_gid) + (getu16(fs, in->i_gid_high) << 16);
    fs_inode->mtime = getu32(fs, in->i_mtime);
    fs_inode->atime = getu32(fs, in->i_atime);
    fs_inode->ctime = getu32(fs, in->i_ctime);
    fs_inode->dtime = getu32(fs, in->i_dtime);

    fs_inode->seq = 0;

    if (fs_inode->link) {
	free(fs_inode->link);
	fs_inode->link = NULL;
    }

    if (fs_inode->direct_count != EXT2FS_NDADDR
	|| fs_inode->indir_count != EXT2FS_NIADDR) {
	if ((fs_inode =
		fs_inode_realloc(fs_inode, EXT2FS_NDADDR,
		    EXT2FS_NIADDR)) == NULL) {
	    return 1;
	}
    }

    for (i = 0; i < EXT2FS_NDADDR; i++)
	fs_inode->direct_addr[i] = gets32(fs, in->i_block[i]);

    for (i = 0; i < EXT2FS_NIADDR; i++)
	fs_inode->indir_addr[i] =
	    gets32(fs, in->i_block[i + EXT2FS_NDADDR]);


    /* set the link string 
     * the size check prevents us from trying to allocate a huge amount of
     * memory for a bad inode value
     */
    if (((fs_inode->mode & FS_INODE_FMT) == FS_INODE_LNK) &&
	(fs_inode->size < EXT2FS_MAXPATHLEN) && (fs_inode->size >= 0)) {
	unsigned int count = 0;


	if ((fs_inode->link = mymalloc(fs_inode->size + 1)) == NULL)
	    return 1;

	/* it is located directly in the pointers */
	if (fs_inode->size < 4 * (EXT2FS_NDADDR + EXT2FS_NIADDR)) {
	    unsigned int j;

	    for (i = 0; i < (EXT2FS_NDADDR + EXT2FS_NIADDR) &&
		count < fs_inode->size; i++) {
		char *ptr = (char *) &in->i_block[i];
		for (j = 0; j < 4 && count < fs_inode->size; j++) {
		    fs_inode->link[count++] = ptr[j];
		}
	    }
	    fs_inode->link[count] = '\0';

	    /* clear the values to avoid the prog from reading them */
	    for (i = 0; i < EXT2FS_NDADDR; i++)
		fs_inode->direct_addr[i] = 0;
	    for (i = 0; i < EXT2FS_NIADDR; i++)
		fs_inode->indir_addr[i] = 0;
	}

	/* it is in blocks */
	else {
	    FS_INFO *fs = (FS_INFO *) & ext2fs->fs_info;
	    DATA_BUF *data_buf;
	    char *ptr = fs_inode->link;

	    if ((data_buf = data_buf_alloc(fs->block_size)) == NULL)
		return 1;

	    /* we only need to do the direct blocks due to the limit 
	     * on path length */
	    for (i = 0; i < EXT2FS_NDADDR && count < fs_inode->size; i++) {
		SSIZE_T cnt;

		int read_count =
		    (fs_inode->size - count <
		    fs->block_size) ? fs_inode->size -
		    count : fs->block_size;

		cnt = fs_read_block(fs, data_buf, fs->block_size,
		    fs_inode->direct_addr[i]);

		if (cnt != fs->block_size) {
		    if (cnt != -1) {
			tsk_errno = TSK_ERR_FS_READ;
			tsk_errstr[0] = '\0';
		    }
		    snprintf(tsk_errstr2, TSK_ERRSTR_L,
			"ext2fs_dinode_copy: symlink destination from %"
			PRIuDADDR, fs_inode->direct_addr[i]);
		    data_buf_free(data_buf);
		    return 1;
		}

		memcpy(ptr, data_buf->data, read_count);
		count += read_count;
		ptr = (char *) ((uintptr_t) ptr + count);
	    }

	    /* terminate the string */
	    *ptr = '\0';
	    data_buf_free(data_buf);
	}

    }

    /* Fill in the flags value */
    grp_num = (ext2fs->dino_inum - fs->first_inum) /
	getu32(fs, ext2fs->fs->s_inodes_per_group);

    if (ext2fs->imap_grp_num != grp_num) {
	if (ext2fs_imap_load(ext2fs, grp_num)) {
	    return 1;
	}
    }

    ibase = grp_num * getu32(fs, ext2fs->fs->s_inodes_per_group) +
	fs->first_inum;

    /*
     * Apply the allocated/unallocated restriction.
     */
    fs_inode->flags = (isset(ext2fs->imap_buf, ext2fs->dino_inum - ibase) ?
	FS_FLAG_META_ALLOC : FS_FLAG_META_UNALLOC);

    fs_inode->flags |= (fs_inode->nlink ?
	FS_FLAG_META_LINK : FS_FLAG_META_UNLINK);

    /*
     * Apply the used/unused restriction.
     */
    fs_inode->flags |= (fs_inode->ctime ?
	FS_FLAG_META_USED : FS_FLAG_META_UNUSED);

    return 0;
}

/* ext2fs_inode_lookup - lookup inode, external interface 
 *
 * Returns NULL on error
 * */

static FS_INODE *
ext2fs_inode_lookup(FS_INFO * fs, INUM_T inum)
{
    EXT2FS_INFO *ext2fs = (EXT2FS_INFO *) fs;
    FS_INODE *fs_inode;

    if ((fs_inode = fs_inode_alloc(EXT2FS_NDADDR, EXT2FS_NIADDR)) == NULL)
	return NULL;

    if (ext2fs_dinode_load(ext2fs, inum)) {
	fs_inode_free(fs_inode);
	return NULL;
    }

    if (ext2fs_dinode_copy(ext2fs, fs_inode)) {
	fs_inode_free(fs_inode);
	return NULL;
    }

    return (fs_inode);
}

/* ext2fs_inode_walk - inode iterator 
 *
 * flags used: FS_FLAG_META_USED, FS_FLAG_META_UNUSED,
 *  FS_FLAG_META_LINK, FS_FLAG_META_UNLINK,
 *  FS_FLAG_META_ALLOC, FS_FLAG_META_UNALLOC
 *
 *  Return 1 on error and 0 on success
*/

uint8_t
ext2fs_inode_walk(FS_INFO * fs, INUM_T start_inum, INUM_T end_inum,
    int flags, FS_INODE_WALK_FN action, void *ptr)
{
    char *myname = "extXfs_inode_walk";
    EXT2FS_INFO *ext2fs = (EXT2FS_INFO *) fs;
    EXT2_GRPNUM_T grp_num;
    INUM_T inum;
    INUM_T ibase = 0;
    FS_INODE *fs_inode;
    int myflags;


    /*
     * Sanity checks.
     */
    if (start_inum < fs->first_inum || start_inum > fs->last_inum) {
	tsk_errno = TSK_ERR_FS_WALK_RNG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "%s: start inode: %" PRIuINUM "", myname, start_inum);
	tsk_errstr2[0] = '\0';
	return 1;
    }

    if (end_inum < fs->first_inum || end_inum > fs->last_inum
	|| end_inum < start_inum) {
	tsk_errno = TSK_ERR_FS_WALK_RNG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "%s: end inode: %" PRIuINUM "", myname, end_inum);
	tsk_errstr2[0] = '\0';
	return 1;
    }

    if ((fs_inode = fs_inode_alloc(EXT2FS_NDADDR, EXT2FS_NIADDR)) == NULL)
	return 1;

    /*
     * Iterate.
     */
    for (inum = start_inum; inum <= end_inum; inum++) {
	int retval;

	/*
	 * Be sure to use the proper group descriptor data. XXX Linux inodes
	 * start at 1, as in Fortran.
	 */
	grp_num = (inum - 1) / getu32(fs, ext2fs->fs->s_inodes_per_group);
	if ((ext2fs->imap_buf == NULL)
	    || (ext2fs->imap_grp_num != grp_num)) {
	    if (ext2fs_imap_load(ext2fs, grp_num)) {
		return 1;
	    }
	    ibase =
		grp_num * getu32(fs, ext2fs->fs->s_inodes_per_group) + 1;
	}

	/* In case we didn't need to load it the bitmap */
	else if (inum == start_inum) {
	    ibase =
		grp_num * getu32(fs, ext2fs->fs->s_inodes_per_group) + 1;
	}

	/*
	 * Apply the allocated/unallocated restriction.
	 */
	myflags = (isset(ext2fs->imap_buf, inum - ibase) ?
	    FS_FLAG_META_ALLOC : FS_FLAG_META_UNALLOC);
	if ((flags & myflags) != myflags)
	    continue;

	if (ext2fs_dinode_load(ext2fs, inum)) {
	    fs_inode_free(fs_inode);
	    return 1;
	}

	/*
	 * Apply the linked/unlinked restriction.
	 */
	myflags |= (getu16(fs, ext2fs->dino_buf->i_nlink) ?
	    FS_FLAG_META_LINK : FS_FLAG_META_UNLINK);

	if ((flags & myflags) != myflags)
	    continue;

	/*
	 * Apply the used/unused restriction.
	 */

	myflags |= (getu32(fs, ext2fs->dino_buf->i_ctime) ?
	    FS_FLAG_META_USED : FS_FLAG_META_UNUSED);

	if ((flags & myflags) != myflags)
	    continue;

	/*
	 * Fill in a file system-independent inode structure and pass control
	 * to the application.
	 */
	if (ext2fs_dinode_copy(ext2fs, fs_inode)) {
	    fs_inode_free(fs_inode);
	    return 1;
	}
	fs_inode->flags = myflags;

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

    /*
     * Cleanup.
     */
    fs_inode_free(fs_inode);
    return 0;
}

/* ext2fs_block_walk - block iterator 
 *
 * flags: FS_FLAG_DATA_ALLOC, FS_FLAG_DATA_UNALLOC, FS_FLAG_DATA_CONT,
 *  FS_FLAG_DATA_META
 *
 *  Return 1 on error and 0 on success
*/

uint8_t
ext2fs_block_walk(FS_INFO * fs, DADDR_T start_blk, DADDR_T end_blk,
    int flags, FS_BLOCK_WALK_FN action, void *ptr)
{
    char *myname = "extXfs_block_walk";
    EXT2FS_INFO *ext2fs = (EXT2FS_INFO *) fs;
    DATA_BUF *data_buf;
    EXT2_GRPNUM_T grp_num;
    DADDR_T addr;
    DADDR_T dbase = 0;		/* first block number in group */
    DADDR_T dmin = 0;		/* first block after inodes */
    int myflags;

    /*
     * Sanity checks.
     */
    if (start_blk < fs->first_block || start_blk > fs->last_block) {
	tsk_errno = TSK_ERR_FS_WALK_RNG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "%s: start block: %" PRIuDADDR, myname, start_blk);
	tsk_errstr2[0] = '\0';
	return 1;
    }
    if (end_blk < fs->first_block || end_blk > fs->last_block
	|| end_blk < start_blk) {
	tsk_errno = TSK_ERR_FS_WALK_RNG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "%s: end block: %" PRIuDADDR, myname, end_blk);
	tsk_errstr2[0] = '\0';
	return 1;
    }

    if ((data_buf = data_buf_alloc(fs->block_size)) == NULL) {
	return 1;
    }

    /*
     * Iterate. This is not as tricky as it could be, because the free list
     * map covers the entire disk partition, including blocks occupied by
     * group descriptor blocks, bit maps, and other non-data blocks.
     */
    for (addr = start_blk; addr <= end_blk; addr++) {
	SSIZE_T cnt;

	/*
	 * Be sure to use the right group descriptor information. XXX There
	 * appears to be an off-by-one discrepancy between bitmap offsets and
	 * disk block numbers.
	 *
	 * Addendum: this offset is controlled by the super block's
	 * s_first_data_block field.
	 */
#define INODE_TABLE_SIZE(ext2fs) \
    ((getu32(fs, ext2fs->fs->s_inodes_per_group) * ext2fs->inode_size - 1) \
           / fs->block_size + 1)

	/* This is meta data that is not described in the groups */
	if (addr < ext2fs->first_data_block) {
	    myflags = FS_FLAG_DATA_META | FS_FLAG_DATA_ALLOC;

	    if ((flags & myflags) == myflags) {
		int retval;
		cnt = fs_read_block(fs, data_buf, fs->block_size, addr);

		if (cnt != fs->block_size) {
		    if (cnt != -1) {
			tsk_errno = TSK_ERR_FS_READ;
			tsk_errstr[0] = '\0';
		    }
		    snprintf(tsk_errstr2, TSK_ERRSTR_L,
			"ext2fs_block_walk: metadata block %"
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
	    continue;
	}

	grp_num = ext2_dtog_lcl(fs, ext2fs->fs, addr);

	/* Lookup bitmap if not loaded */
	if ((ext2fs->bmap_buf == NULL)
	    || (ext2fs->bmap_grp_num != grp_num)) {

	    if (ext2fs_bmap_load(ext2fs, grp_num)) {
		data_buf_free(data_buf);
		return 1;
	    }

	    dbase = ext2_cgbase_lcl(fs, ext2fs->fs, grp_num);
	    dmin = getu32(fs, ext2fs->grp_buf->bg_inode_table) +
		INODE_TABLE_SIZE(ext2fs);

	    if (verbose)
		fprintf(stderr,
		    "block_walk: loading group %" PRI_EXT2GRP
		    " dbase %" PRIuDADDR " bmap +%" PRIuDADDR
		    " imap +%" PRIuDADDR " inos +%" PRIuDADDR "..%"
		    PRIuDADDR "\n", grp_num, dbase,
		    (DADDR_T) getu32(fs, ext2fs->grp_buf->bg_block_bitmap)
		    - dbase, (DADDR_T) getu32(fs,
			ext2fs->grp_buf->
			bg_inode_bitmap) - dbase,
		    (DADDR_T) getu32(fs,
			ext2fs->grp_buf->bg_inode_table) -
		    dbase, dmin - 1 - dbase);

	}
	/* In case we didn't need to load the bmap */
	else if ((addr == start_blk) || (addr == ext2fs->first_data_block)) {
	    dbase = ext2_cgbase_lcl(fs, ext2fs->fs, grp_num);
	    dmin = getu32(fs, ext2fs->grp_buf->bg_inode_table) +
		INODE_TABLE_SIZE(ext2fs);
	}


	/*
	 * Pass blocks of interest to the application. Identify meta blocks
	 * (any blocks that can't be allocated for file/directory data).
	 *
	 * XXX With sparse superblock placement, most block groups have the
	 * block and inode bitmaps where one would otherwise find the backup
	 * superblock and the backup group descriptor blocks. The inode
	 * blocks are in the normal place, though. This leaves little gaps
	 * between the bitmaps and the inode table - and ext2fs will use
	 * those blocks for file/directory data blocks. So we must properly
	 * account for those gaps between meta blocks.
	 *
	 * Thus, superblocks and group descriptor blocks are sometimes overlaid
	 * by bitmap blocks. This means that one can still assume that the
	 * locations of superblocks and group descriptor blocks are reserved.
	 * They just happen to be reserved for something else :-)
	 */
	myflags = (isset(ext2fs->bmap_buf, addr - dbase) ?
	    FS_FLAG_DATA_ALLOC : FS_FLAG_DATA_UNALLOC);

	if ((addr >= dbase
		&& addr < getu32(fs, ext2fs->grp_buf->bg_block_bitmap))
	    || (addr == getu32(fs, ext2fs->grp_buf->bg_block_bitmap))
	    || (addr == getu32(fs, ext2fs->grp_buf->bg_inode_bitmap))
	    || (addr >= getu32(fs, ext2fs->grp_buf->bg_inode_table)
		&& addr < dmin))
	    myflags |= FS_FLAG_DATA_META;
	else
	    myflags |= FS_FLAG_DATA_CONT;

	/*
	   if ((myflags & FS_FLAG_DATA_META)
	   && (myflags & FS_FLAG_DATA_UNALLOC)) {
	   remark("unallocated meta block %" PRIuDADDR "!! dbase %"
	   PRIuDADDR " dmin %" PRIuDADDR "", addr, dbase, dmin);
	   }
	 */

	if ((flags & myflags) == myflags) {
	    SSIZE_T cnt;
	    int retval;
	    cnt = fs_read_block(fs, data_buf, fs->block_size, addr);

	    if (cnt != fs->block_size) {
		if (cnt != -1) {
		    tsk_errno = TSK_ERR_FS_READ;
		    tsk_errstr[0] = '\0';
		}
		snprintf(tsk_errstr2, TSK_ERRSTR_L,
		    "ext2fs_block_walk: block %" PRIuDADDR, addr);
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

    /*
     * Cleanup.
     */
    data_buf_free(data_buf);
    return 0;
}



/**************************************************************************
 * 
 * FILE WALKING
 *
 **************************************************************************/

/*
 * addr is the address of a block that contains file content.  Read it
 * and pass it to the action.
 *
 *  return the amount processed, 0 if the action wanted to
 *  stop, and -1 if an error occurred
 */
static SSIZE_T
ext2fs_file_walk_direct(FS_INFO * fs, DATA_BUF * buf[],
    OFF_T length, uint32_t addr, int flags,
    FS_FILE_WALK_FN action, void *ptr)
{
    OFF_T read_count;
    int myflags;

    read_count = (length < buf[0]->size ? length : buf[0]->size);

    if (addr > fs->last_block) {
	tsk_errno = TSK_ERR_FS_INODE_INT;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "ext2: Invalid direct address (too large): %"
	    PRIuDADDR "", (DADDR_T) addr);
	tsk_errstr2[0] = '\0';
	return -1;
    }

    // @@@ We do not check allocation status here
    myflags = FS_FLAG_DATA_CONT;

    if (addr == 0) {
	if (0 == (flags & FS_FLAG_FILE_NOSPARSE)) {
	    int retval;

	    if ((flags & FS_FLAG_FILE_AONLY) == 0)
		memset(buf[0]->data, 0, read_count);

	    retval = action(fs, addr, buf[0]->data, read_count,
		myflags, ptr);
	    if (retval == WALK_STOP)
		return 0;
	    else if (retval == WALK_ERROR)
		return -1;
	}
    }
    else {
	int retval;
	OFF_T rounded_size = roundup(read_count, EXT2FS_DEV_BSIZE);
	if ((flags & FS_FLAG_FILE_AONLY) == 0) {
	    SSIZE_T cnt;
	    cnt = fs_read_block(fs, buf[0], rounded_size, addr);
	    if (cnt != (SSIZE_T) rounded_size) {
		if (cnt != -1) {
		    tsk_errno = TSK_ERR_FS_READ;
		    tsk_errstr[0] = '\0';
		}
		snprintf(tsk_errstr2, TSK_ERRSTR_L,
		    "ext2fs_file_walk_direct: block %" PRIuDADDR,
		    (DADDR_T) addr);
		return -1;
	    }
	}

	retval = action(fs, addr, buf[0]->data, read_count, myflags, ptr);
	if (retval == WALK_STOP)
	    return 0;
	else if (retval == WALK_ERROR)
	    return -1;
    }

    return (read_count);
}


/*
 * Process the data at block addr as a list of pointers at level _level_
 *
 * Return -1 on error, 0 if the action wants to stop, or the number of
 * bytes that were processed.
 */
static SSIZE_T
ext2fs_file_walk_indir(FS_INFO * fs, DATA_BUF * buf[], OFF_T length,
    uint32_t addr, int level, int flags, FS_FILE_WALK_FN action, void *ptr)
{
    char *myname = "extXfs_file_walk_indir";
    OFF_T todo_count = length;
    uint32_t *iaddr;
    unsigned int n;

    if (verbose)
	fprintf(stderr, "%s: level %d block %" PRIu32 "\n", myname, level,
	    addr);

    if (addr > fs->last_block) {
	tsk_errno = TSK_ERR_FS_INODE_INT;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "ext2fs: Indirect block address too large: %"
	    PRIuDADDR, (DADDR_T) addr);
	tsk_errstr2[0] = '\0';
	return -1;

    }

    /*
     * Read a block of disk addresses.
     */
    if (addr == 0) {
	memset(buf[level]->data, 0, buf[level]->size);
    }
    else {
	SSIZE_T cnt;
	cnt = fs_read_block(fs, buf[level], buf[level]->size, addr);
	if (cnt != buf[level]->size) {
	    if (cnt != -1) {
		tsk_errno = TSK_ERR_FS_READ;
		tsk_errstr[0] = '\0';
	    }
	    snprintf(tsk_errstr2, TSK_ERRSTR_L,
		"ext2fs_file_walk_indir: Block %" PRIuDADDR,
		(DADDR_T) addr);
	    return -1;
	}
    }

    if (flags & FS_FLAG_FILE_META) {
	int myflags = FS_FLAG_DATA_META;
	int retval;
	retval =
	    action(fs, addr, buf[level]->data, buf[level]->size, myflags,
	    ptr);
	if (retval == WALK_STOP)
	    return 0;
	else if (retval == WALK_ERROR)
	    return -1;
    }

    /*
     * For each disk address, copy a direct block or process an indirect
     * block.
     */
    iaddr = (uint32_t *) buf[level]->data;
    for (n = 0; todo_count > 0 && n < buf[level]->size / sizeof(*iaddr);
	n++) {
	OFF_T prevcnt = todo_count;

	if (getu32(fs, (uint8_t *) & iaddr[n]) > fs->last_block) {
	    tsk_errno = TSK_ERR_FS_INODE_INT;
	    snprintf(tsk_errstr, TSK_ERRSTR_L,
		"ext2fs: Address in indirect block too large: %"
		PRIu32, getu32(fs, (uint8_t *) & iaddr[n]));
	    tsk_errstr2[0] = '\0';
	    return -1;
	}


	if (level == 1)
	    todo_count -= ext2fs_file_walk_direct(fs, buf, todo_count,
		getu32(fs, (uint8_t *) & iaddr[n]), flags, action, ptr);
	else
	    todo_count -= ext2fs_file_walk_indir(fs, buf, todo_count,
		getu32(fs,
		    (uint8_t *) &
		    iaddr[n]), level - 1, flags, action, ptr);

	/* nothing was updated, so we should go now */
	if (prevcnt == todo_count)
	    return 0;
	else if (prevcnt < todo_count)
	    return -1;
    }

    return (length - todo_count);
}


/*      
 * flag values: FS_FLAG_FILE_NOSPARSE, FS_FLAG_FILE_AONLY, FS_FLAG_FILE_SLACK
 * FS_FLAG_FILE_META
 *
 * nothing special is done for FS_FLAG_FILE_RECOVER
 *
 * The action will use the flags: FS_FLAG_DATA_CONT, FS_FLAG_DATA_META
 * -- @@@ Currently do not do _ALLOC and _UNALLOC
 *  
 * The type and id fields are ignored with EXT2FS
 *
 * return 1 on error and 0 on success
 *
 */
uint8_t
ext2fs_file_walk(FS_INFO * fs, FS_INODE * inode, uint32_t type,
    uint16_t id, int flags, FS_FILE_WALK_FN action, void *ptr)
{
    OFF_T length = 0;
    DATA_BUF **buf;
    int level, n;
    SSIZE_T read_b = 0;


    if (verbose)
	fprintf(stderr,
	    "ext2fs_file_walk: Processing file %" PRIuINUM "\n",
	    inode->addr);

    /*
     * Initialize a buffer for each level of indirection that is supported by
     * this inode. The level 0 buffer is sized to the logical block size used
     * for files. The level 1.. buffers are sized to the block size used for
     * indirect blocks.
     */
    if ((buf =
	    (DATA_BUF **) mymalloc(sizeof(*buf) *
		(inode->indir_count + 1))) == NULL) {
	return 1;
    }

    if ((buf[0] = data_buf_alloc(fs->block_size)) == NULL) {
	free(buf);
	return 1;
    }

    length = inode->size;

    /* Roundup if we want the slack space on the final fragment */
    if (flags & FS_FLAG_FILE_SLACK)
	length = roundup(length, fs->block_size);

    /*
     * Read the file blocks. First the direct blocks, then the indirect ones.
     */

    for (n = 0; length > 0 && n < inode->direct_count; n++) {
	read_b = ext2fs_file_walk_direct(fs, buf, length,
	    inode->direct_addr[n], flags, action, ptr);

	if (read_b == -1) {
	    free(buf);
	    data_buf_free(buf[0]);
	    return 1;
	}
	else if (read_b == 0) {
	    length = 0;
	    break;
	}
	length -= read_b;
    }

    if (length > 0) {
	for (level = 1; level <= inode->indir_count; level++) {
	    if ((buf[level] = data_buf_alloc(fs->block_size)) == NULL) {
		int f;
		for (f = 0; f < level; f++)
		    free(buf[f]);
		free(buf);
		return 1;
	    }
	}

	for (level = 1; length > 0 && level <= inode->indir_count; level++) {

	    read_b = ext2fs_file_walk_indir(fs, buf, length,
		(uint32_t) inode->
		indir_addr[level - 1], level, flags, action, ptr);

	    if ((read_b == 0) || (read_b == -1)) {
		break;
	    }

	    length -= read_b;
	}
	/*
	 * Cleanup.
	 */
	for (level = 1; level <= inode->indir_count; level++)
	    data_buf_free(buf[level]);

    }

    data_buf_free(buf[0]);
    free((char *) buf);

    if (read_b == -1)
	return 1;
    else
	return 0;
}



static uint8_t
ext2fs_fscheck(FS_INFO * fs, FILE * hFile)
{
    tsk_errno = TSK_ERR_FS_FUNC;
    snprintf(tsk_errstr, TSK_ERRSTR_L,
	"fscheck not implemented yet for Ext3");
    tsk_errstr2[0] = '\0';
    return 1;
}


/* Return 0 on success and 1 on error */

static uint8_t
ext2fs_fsstat(FS_INFO * fs, FILE * hFile)
{
    unsigned int i;
    EXT2FS_INFO *ext2fs = (EXT2FS_INFO *) fs;
    ext2fs_sb *sb = ext2fs->fs;
    int ibpg;
    time_t tmptime;


    fprintf(hFile, "FILE SYSTEM INFORMATION\n");
    fprintf(hFile, "--------------------------------------------\n");

    fprintf(hFile, "File System Type: %s\n",
	(fs->ftype == EXT3FS) ? "Ext3" : "Ext2");
    fprintf(hFile, "Volume Name: %s\n", sb->s_volume_name);
    fprintf(hFile, "Volume ID: %" PRIx64 "%" PRIx64 "\n",
	getu64(fs, &sb->s_uuid[8]), getu64(fs, &sb->s_uuid[0]));

    tmptime = getu32(fs, sb->s_wtime);
    fprintf(hFile, "\nLast Written at: %s",
	(tmptime > 0) ? asctime(localtime(&tmptime)) : "empty");
    tmptime = getu32(fs, sb->s_lastcheck);
    fprintf(hFile, "Last Checked at: %s",
	(tmptime > 0) ? asctime(localtime(&tmptime)) : "empty");

    tmptime = getu32(fs, sb->s_mtime);
    fprintf(hFile, "\nLast Mounted at: %s",
	(tmptime > 0) ? asctime(localtime(&tmptime)) : "empty");

    /* State of the file system */
    if (getu16(fs, sb->s_state) & EXT2FS_STATE_VALID)
	fprintf(hFile, "Unmounted properly\n");
    else
	fprintf(hFile, "Unmounted Improperly\n");

    if (sb->s_last_mounted != '\0')
	fprintf(hFile, "Last mounted on: %s\n", sb->s_last_mounted);

    fprintf(hFile, "\nSource OS: ");
    switch (getu32(fs, sb->s_creator_os)) {
    case EXT2FS_OS_LINUX:
	fprintf(hFile, "Linux\n");
	break;
    case EXT2FS_OS_HURD:
	fprintf(hFile, "HURD\n");
	break;
    case EXT2FS_OS_MASIX:
	fprintf(hFile, "MASIX\n");
	break;
    case EXT2FS_OS_FREEBSD:
	fprintf(hFile, "FreeBSD\n");
	break;
    case EXT2FS_OS_LITES:
	fprintf(hFile, "LITES\n");
	break;
    default:
	fprintf(hFile, "%x\n", getu32(fs, sb->s_creator_os));
	break;
    }

    if (getu32(fs, sb->s_rev_level) == EXT2FS_REV_ORIG)
	fprintf(hFile, "Static Structure\n");
    else
	fprintf(hFile, "Dynamic Structure\n");


    /* add features */
    if (getu32(fs, sb->s_feature_compat)) {
	fprintf(hFile, "Compat Features: ");

	if (getu32(fs, sb->s_feature_compat) &
	    EXT2FS_FEATURE_COMPAT_DIR_PREALLOC)
	    fprintf(hFile, "Dir Prealloc, ");
	if (getu32(fs, sb->s_feature_compat) &
	    EXT2FS_FEATURE_COMPAT_IMAGIC_INODES)
	    fprintf(hFile, "iMagic inodes, ");
	if (getu32(fs, sb->s_feature_compat) &
	    EXT2FS_FEATURE_COMPAT_HAS_JOURNAL)
	    fprintf(hFile, "Journal, ");
	if (getu32(fs, sb->s_feature_compat) &
	    EXT2FS_FEATURE_COMPAT_EXT_ATTR)
	    fprintf(hFile, "Ext Attributes, ");
	if (getu32(fs, sb->s_feature_compat) &
	    EXT2FS_FEATURE_COMPAT_RESIZE_INO)
	    fprintf(hFile, "Resize Inode, ");
	if (getu32(fs, sb->s_feature_compat) &
	    EXT2FS_FEATURE_COMPAT_DIR_INDEX)
	    fprintf(hFile, "Dir Index");

	fprintf(hFile, "\n");
    }

    if (getu32(fs, sb->s_feature_incompat)) {
	fprintf(hFile, "InCompat Features: ");

	if (getu32(fs, sb->s_feature_incompat) &
	    EXT2FS_FEATURE_INCOMPAT_COMPRESSION)
	    fprintf(hFile, "Compression, ");
	if (getu32(fs, sb->s_feature_incompat) &
	    EXT2FS_FEATURE_INCOMPAT_FILETYPE)
	    fprintf(hFile, "Filetype, ");
	if (getu32(fs, sb->s_feature_incompat) &
	    EXT2FS_FEATURE_INCOMPAT_RECOVER)
	    fprintf(hFile, "Needs Recovery, ");
	if (getu32(fs, sb->s_feature_incompat) &
	    EXT2FS_FEATURE_INCOMPAT_JOURNAL_DEV)
	    fprintf(hFile, "Journal Dev");

	fprintf(hFile, "\n");
    }

    if (getu32(fs, sb->s_feature_ro_compat)) {
	fprintf(hFile, "Read Only Compat Features: ");

	if (getu32(fs, sb->s_feature_ro_compat) &
	    EXT2FS_FEATURE_RO_COMPAT_SPARSE_SUPER)
	    fprintf(hFile, "Sparse Super, ");
	if (getu32(fs, sb->s_feature_ro_compat) &
	    EXT2FS_FEATURE_RO_COMPAT_LARGE_FILE)
	    fprintf(hFile, "Has Large Files, ");
	if (getu32(fs, sb->s_feature_ro_compat) &
	    EXT2FS_FEATURE_RO_COMPAT_BTREE_DIR)
	    fprintf(hFile, "Btree Dir");

	fprintf(hFile, "\n");

    }

    /* Print journal information */
    if (getu32(fs, sb->s_feature_compat) &
	EXT2FS_FEATURE_COMPAT_HAS_JOURNAL) {

	fprintf(hFile, "\nJournal ID: %" PRIx64 "%" PRIx64 "\n",
	    getu64(fs, &sb->s_journal_uuid[8]),
	    getu64(fs, &sb->s_journal_uuid[0]));

	if (getu32(fs, sb->s_journal_inum) != 0)
	    fprintf(hFile, "Journal Inode: %" PRIu32 "\n",
		getu32(fs, sb->s_journal_inum));

	if (getu32(fs, sb->s_journal_dev) != 0)
	    fprintf(hFile, "Journal Device: %" PRIu32 "\n",
		getu32(fs, sb->s_journal_dev));


    }

    fprintf(hFile, "\nMETADATA INFORMATION\n");
    fprintf(hFile, "--------------------------------------------\n");

    fprintf(hFile, "Inode Range: %" PRIuINUM " - %" PRIuINUM "\n",
	fs->first_inum, fs->last_inum);
    fprintf(hFile, "Root Directory: %" PRIuINUM "\n", fs->root_inum);

    fprintf(hFile, "Free Inodes: %" PRIu32 "\n",
	getu32(fs, sb->s_free_inode_count));


    if (getu32(fs, sb->s_last_orphan)) {
	uint32_t or_in;
	fprintf(hFile, "Orphan Inodes: ");
	or_in = getu32(fs, sb->s_last_orphan);
	while (or_in) {
	    FS_INODE *fsi;

	    if ((or_in > fs->last_inum) || (or_in < fs->first_inum))
		break;

	    fprintf(hFile, "%" PRIu32 ", ", or_in);

	    /* Get the next one */
	    fsi = ext2fs_inode_lookup(fs, or_in);
	    /* Ignore this error */
	    if (!fsi) {
		tsk_errno = 0;
		tsk_errstr[0] = '\0';
		break;
	    }

	    or_in = fsi->dtime;
	    fs_inode_free(fsi);
	}
	fprintf(hFile, "\n");
    }

    fprintf(hFile, "\nCONTENT INFORMATION\n");
    fprintf(hFile, "--------------------------------------------\n");

    fprintf(hFile, "Block Range: %" PRIuDADDR " - %" PRIuDADDR "\n",
	fs->first_block, fs->last_block);

    fprintf(hFile, "Block Size: %u\n", fs->block_size);

    if (getu32(fs, sb->s_first_data_block))
	fprintf(hFile,
	    "Reserved Blocks Before Block Groups: %" PRIu32 "\n",
	    getu32(fs, sb->s_first_data_block));

    fprintf(hFile, "Free Blocks: %" PRIu32 "\n",
	getu32(fs, sb->s_free_blocks_count));

    fprintf(hFile, "\nBLOCK GROUP INFORMATION\n");
    fprintf(hFile, "--------------------------------------------\n");

    fprintf(hFile, "Number of Block Groups: %d\n", ext2fs->groups_count);

    fprintf(hFile, "Inodes per group: %" PRIu32 "\n",
	getu32(fs, sb->s_inodes_per_group));
    fprintf(hFile, "Blocks per group: %" PRIu32 "\n",
	getu32(fs, sb->s_blocks_per_group));


    /* number of blocks the inodes consume */
    ibpg = (getu32(fs, sb->s_inodes_per_group) * ext2fs->inode_size +
	fs->block_size - 1) / fs->block_size;

    for (i = 0; i < ext2fs->groups_count; i++) {
	DADDR_T cg_base;
	INUM_T inum;

	if (ext2fs_group_load(ext2fs, i)) {
	    return 1;
	}
	fprintf(hFile, "\nGroup: %d:\n", i);

	inum = fs->first_inum + gets32(fs, sb->s_inodes_per_group) * i;
	fprintf(hFile, "  Inode Range: %" PRIuINUM " - ", inum);

	if ((inum + gets32(fs, sb->s_inodes_per_group) - 1) <
	    fs->last_inum)
	    fprintf(hFile, "%" PRIuINUM "\n",
		inum + gets32(fs, sb->s_inodes_per_group) - 1);
	else
	    fprintf(hFile, "%" PRIuINUM "\n", fs->last_inum);


	cg_base = ext2_cgbase_lcl(fs, sb, i);

	fprintf(hFile, "  Block Range: %" PRIuDADDR " - %" PRIuDADDR "\n",
	    cg_base,
	    ((ext2_cgbase_lcl(fs, sb, i + 1) - 1) < fs->last_block) ?
	    (ext2_cgbase_lcl(fs, sb, i + 1) - 1) : fs->last_block);


	fprintf(hFile, "  Layout:\n");

	/* only print the super block data if we are not in a sparse
	 * group 
	 */
	if (((getu32(fs, ext2fs->fs->s_feature_ro_compat) &
		    EXT2FS_FEATURE_RO_COMPAT_SPARSE_SUPER) &&
		(cg_base != getu32(fs, ext2fs->grp_buf->bg_block_bitmap)))
	    || ((getu32(fs,
			ext2fs->fs->
			s_feature_ro_compat) &
		    EXT2FS_FEATURE_RO_COMPAT_SPARSE_SUPER) == 0)) {

	    OFF_T boff;

	    /* the super block is the first 1024 bytes */
	    fprintf(hFile,
		"    Super Block: %" PRIuDADDR " - %" PRIuDADDR "\n",
		cg_base,
		cg_base +
		((sizeof(ext2fs_sb) + fs->block_size -
			1) / fs->block_size) - 1);

	    boff = roundup(sizeof(ext2fs_sb), fs->block_size);

	    /* Group Descriptors */
	    fprintf(hFile, "    Group Descriptor Table: %" PRIuDADDR " - ",
		(cg_base + (boff + fs->block_size - 1) / fs->block_size));

	    boff += (ext2fs->groups_count * sizeof(ext2fs_gd));
	    fprintf(hFile, "%" PRIuDADDR "\n",
		((cg_base +
			(boff + fs->block_size - 1) / fs->block_size) -
		    1));
	}


	/* The block bitmap is a full block */
	fprintf(hFile, "    Data bitmap: %" PRIu32 " - %" PRIu32 "\n",
	    getu32(fs, ext2fs->grp_buf->bg_block_bitmap),
	    getu32(fs, ext2fs->grp_buf->bg_block_bitmap));


	/* The inode bitmap is a full block */
	fprintf(hFile, "    Inode bitmap: %" PRIu32 " - %" PRIu32 "\n",
	    getu32(fs, ext2fs->grp_buf->bg_inode_bitmap),
	    getu32(fs, ext2fs->grp_buf->bg_inode_bitmap));


	fprintf(hFile, "    Inode Table: %" PRIu32 " - %" PRIu32 "\n",
	    getu32(fs, ext2fs->grp_buf->bg_inode_table),
	    getu32(fs, ext2fs->grp_buf->bg_inode_table) + ibpg - 1);


	fprintf(hFile, "    Data Blocks: ");

	/* If we are in a sparse group, display the other addresses */
	if ((getu32(fs, ext2fs->fs->s_feature_ro_compat) &
		EXT2FS_FEATURE_RO_COMPAT_SPARSE_SUPER) &&
	    (cg_base == getu32(fs, ext2fs->grp_buf->bg_block_bitmap))) {

	    /* it goes from the end of the inode bitmap to before the
	     * table
	     *
	     * This hard coded aspect does not scale ...
	     */
	    fprintf(hFile, "%" PRIu32 " - %" PRIu32 ", ",
		getu32(fs, ext2fs->grp_buf->bg_inode_bitmap) + 1,
		getu32(fs, ext2fs->grp_buf->bg_inode_table) - 1);
	}

	fprintf(hFile, "%" PRIuDADDR " - %" PRIuDADDR "\n",
	    (uint64_t) getu32(fs,
		ext2fs->grp_buf->bg_inode_table) + ibpg,
	    ((ext2_cgbase_lcl(fs, sb, i + 1) - 1) <
		fs->last_block) ? (ext2_cgbase_lcl(fs, sb,
		    i + 1) - 1) : fs->last_block);


	/* Print the free info */


	/* The last group may not have a full number of blocks */
	if (i != (ext2fs->groups_count - 1)) {
	    fprintf(hFile, "  Free Inodes: %" PRIu16 " (%d%%)\n",
		getu16(fs, ext2fs->grp_buf->bg_free_inodes_count),
		(100 *
		    getu16(fs,
			ext2fs->grp_buf->bg_free_inodes_count)) /
		getu32(fs, sb->s_inodes_per_group));

	    fprintf(hFile, "  Free Blocks: %" PRIu16 " (%d%%)\n",
		getu16(fs, ext2fs->grp_buf->bg_free_blocks_count),
		(100 *
		    getu16(fs,
			ext2fs->grp_buf->bg_free_blocks_count)) /
		getu32(fs, sb->s_blocks_per_group));
	}
	else {
	    int num_left;

	    num_left = fs->last_inum % gets32(fs, sb->s_inodes_per_group);
	    if (num_left == 0)
		num_left = getu32(fs, sb->s_inodes_per_group);

	    fprintf(hFile, "  Free Inodes: %" PRIu16 " (%d%%)\n",
		getu16(fs, ext2fs->grp_buf->bg_free_inodes_count),
		(100 *
		    getu16(fs,
			ext2fs->grp_buf->bg_free_inodes_count)) /
		num_left);

	    /* Now blocks */
	    num_left =
		fs->block_count % getu32(fs, sb->s_blocks_per_group);
	    if (num_left == 0)
		num_left = getu32(fs, sb->s_blocks_per_group);

	    fprintf(hFile, "  Free Blocks: %" PRIu16 " (%d%%)\n",
		getu16(fs, ext2fs->grp_buf->bg_free_blocks_count),
		(100 *
		    getu16(fs,
			ext2fs->grp_buf->bg_free_blocks_count)) /
		num_left);
	}

	fprintf(hFile, "  Total Directories: %" PRIu16 "\n",
	    getu16(fs, ext2fs->grp_buf->bg_used_dirs_count));
    }

    return 0;
}


/************************* istat *******************************/

static void
ext2fs_make_acl_str(char *str, int len, uint16_t perm)
{
    int i = 0;

    if (perm & EXT2_PACL_PERM_READ) {
	snprintf(&str[i], len - 1, "Read");
	i += 4;
    }
    if (perm & EXT2_PACL_PERM_WRITE) {
	if (i) {
	    snprintf(&str[i], len - 1, ", ");
	    i += 2;
	}
	snprintf(&str[i], len - 1, "Write");
	i += 5;
    }
    if (perm & EXT2_PACL_PERM_EXEC) {
	if (i) {
	    snprintf(&str[i], len - 1, ", ");
	    i += 2;
	}
	snprintf(&str[i], len - 1, "Execute");
	i += 7;
    }
}




/* indirect block accounting */
#define EXT2FS_INDIR_SIZ   64

typedef struct {
    FILE *hFile;
    int idx;
    DADDR_T indirl[EXT2FS_INDIR_SIZ];
    unsigned char indir_idx;
} EXT2FS_PRINT_ADDR;


/* Callback for istat to print the block addresses */
static uint8_t
print_addr_act(FS_INFO * fs, DADDR_T addr, char *buf,
    unsigned int size, int flags, void *ptr)
{
    EXT2FS_PRINT_ADDR *print = (EXT2FS_PRINT_ADDR *) ptr;

    if (flags & FS_FLAG_DATA_CONT) {
	int i, s;
	/* cycle through the blocks if they exist */
	for (i = 0, s = size; s > 0; s -= fs->block_size, i++) {

	    /* sparse file */
	    if (addr)
		fprintf(print->hFile, "%" PRIuDADDR " ", addr + i);
	    else
		fprintf(print->hFile, "0 ");

	    if (++(print->idx) == 8) {
		fprintf(print->hFile, "\n");
		print->idx = 0;
	    }
	}
    }

    /* this must be an indirect block pointer, so put it in the list */
    else if (flags & FS_FLAG_DATA_META) {
	if (print->indir_idx < EXT2FS_INDIR_SIZ)
	    print->indirl[(print->indir_idx)++] = addr;
    }
    return WALK_CONT;
}


/* Return 1 on error and 0 on success */
static uint8_t
ext2fs_istat(FS_INFO * fs, FILE * hFile, INUM_T inum, int numblock,
    int32_t sec_skew)
{
    EXT2FS_INFO *ext2fs = (EXT2FS_INFO *) fs;
    FS_INODE *fs_inode;
    char ls[12];
    EXT2FS_PRINT_ADDR print;

    if ((fs_inode = ext2fs_inode_lookup(fs, inum)) == NULL) {
	strncat(tsk_errstr2, " - istat",
	    TSK_ERRSTR_L - strlen(tsk_errstr2));
	return 1;
    }
    fprintf(hFile, "inode: %" PRIuINUM "\n", inum);
    fprintf(hFile, "%sAllocated\n",
	(fs_inode->flags & FS_FLAG_META_ALLOC) ? "" : "Not ");

    fprintf(hFile, "Group: %lu\n", (ULONG) ext2fs->grp_num);
    fprintf(hFile, "Generation Id: %" PRIu32 "\n",
	getu32(fs, ext2fs->dino_buf->i_generation));

    if (fs_inode->link)
	fprintf(hFile, "symbolic link to: %s\n", fs_inode->link);

    fprintf(hFile, "uid / gid: %d / %d\n",
	(int) fs_inode->uid, (int) fs_inode->gid);

    make_ls(fs_inode->mode, ls);
    fprintf(hFile, "mode: %s\n", ls);

    /* Print the device ids */
    if (((fs_inode->mode & FS_INODE_FMT) == FS_INODE_BLK) ||
	((fs_inode->mode & FS_INODE_FMT) == FS_INODE_CHR)) {
	fprintf(hFile, "Device Major: %" PRIu8 "   Minor: %" PRIu8 "\n",
	    ext2fs->dino_buf->i_block[0][1],
	    ext2fs->dino_buf->i_block[0][0]);
    }

    if (getu32(fs, ext2fs->dino_buf->i_flags)) {
	fprintf(hFile, "Flags: ");
	if (getu32(fs, ext2fs->dino_buf->i_flags) & EXT2_IN_SECDEL)
	    fprintf(hFile, "Secure Delete, ");

	if (getu32(fs, ext2fs->dino_buf->i_flags) & EXT2_IN_UNRM)
	    fprintf(hFile, "Undelete, ");

	if (getu32(fs, ext2fs->dino_buf->i_flags) & EXT2_IN_COMP)
	    fprintf(hFile, "Compressed, ");

	if (getu32(fs, ext2fs->dino_buf->i_flags) & EXT2_IN_SYNC)
	    fprintf(hFile, "Sync Updates, ");

	if (getu32(fs, ext2fs->dino_buf->i_flags) & EXT2_IN_IMM)
	    fprintf(hFile, "Immutable, ");

	if (getu32(fs, ext2fs->dino_buf->i_flags) & EXT2_IN_APPEND)
	    fprintf(hFile, "Append Only, ");

	if (getu32(fs, ext2fs->dino_buf->i_flags) & EXT2_IN_NODUMP)
	    fprintf(hFile, "Do Not Dump, ");

	if (getu32(fs, ext2fs->dino_buf->i_flags) & EXT2_IN_NOA)
	    fprintf(hFile, "No A-Time, ");

	fprintf(hFile, "\n");
    }

    fprintf(hFile, "size: %" PRIuOFF "\n", fs_inode->size);
    fprintf(hFile, "num of links: %lu\n", (ULONG) fs_inode->nlink);

    /* Ext attribute are stored in a block with a header and a list
     * of entries that are aligned to 4-byte boundaries.  The attr
     * value is stored at the end of the block.  There are 4 null bytes
     * in between the headers and values 
     */
    if (getu32(fs, ext2fs->dino_buf->i_file_acl) != 0) {
	char *buf;
	ext2fs_ea_header *ea_head;
	ext2fs_ea_entry *ea_entry;
	SSIZE_T cnt;

	if ((buf = mymalloc(fs->block_size)) == NULL) {
	    fs_inode_free(fs_inode);
	    return 1;
	}

	fprintf(hFile, "\nExtended Attributes  (Block: %" PRIu32 ")\n",
	    getu32(fs, ext2fs->dino_buf->i_file_acl));

	/* Is the value too big? */
	if (getu32(fs, ext2fs->dino_buf->i_file_acl) > fs->last_block) {
	    fprintf(hFile,
		"Extended Attributes block is larger than file system\n");
	    goto egress_ea;
	}

	cnt = fs_read_block_nobuf(fs, buf, fs->block_size,
	    (DADDR_T) getu32(fs, ext2fs->dino_buf->i_file_acl));

	if (cnt != fs->block_size) {
	    if (cnt != -1) {
		tsk_errno = TSK_ERR_FS_READ;
		tsk_errstr[0] = '\0';
	    }
	    snprintf(tsk_errstr2, TSK_ERRSTR_L,
		"ext2fs_istat: ACL block %" PRIu32,
		getu32(fs, ext2fs->dino_buf->i_file_acl));
	    fs_inode_free(fs_inode);
	    free(buf);
	    return 1;
	}


	/* Check the header */
	ea_head = (ext2fs_ea_header *) buf;
	if (getu32(fs, ea_head->magic) != EXT2_EA_MAGIC) {
	    fprintf(hFile, "Incorrect extended attribute header: %x\n",
		getu32(fs, ea_head->magic));
	}


	/* Cycle through each entry - at the top of the block */
	for (ea_entry = (ext2fs_ea_entry *) & ea_head->entry;
	    ((uintptr_t) ea_entry <
		((uintptr_t) buf + fs->block_size -
		    sizeof(ext2fs_ea_entry)));
	    ea_entry =
	    (ext2fs_ea_entry *) ((uintptr_t) ea_entry +
		EXT2_EA_LEN(ea_entry->nlen))) {

	    char name[256];

	    /* Stop if the first four bytes are NULL */
	    if ((ea_entry->nlen == 0) && (ea_entry->nidx == 0) &&
		(getu16(fs, ea_entry->val_off) == 0))
		break;

	    /* The Linux src does not allow this */
	    if (getu32(fs, ea_entry->val_blk) != 0) {
		fprintf(hFile,
		    "Attribute has non-zero value block - skipping\n");
		continue;
	    }


	    /* Is the value location and size valid? */
	    if ((getu32(fs, ea_entry->val_off) > fs->block_size) ||
		((getu16(fs, ea_entry->val_off) +
			getu32(fs,
			    ea_entry->val_size)) > fs->block_size)) {
		continue;
	    }


	    /* Copy the name into a buffer - not NULL term */
	    strncpy(name, (char *) &ea_entry->name, ea_entry->nlen);
	    name[ea_entry->nlen] = '\0';


	    /* User assigned attributes - setfattr / getfattr */
	    if ((ea_entry->nidx == EXT2_EA_IDX_USER) ||
		(ea_entry->nidx == EXT2_EA_IDX_TRUSTED) ||
		(ea_entry->nidx == EXT2_EA_IDX_SECURITY)) {
		char val[256];

		strncpy(val,
		    &buf[getu16(fs, ea_entry->val_off)],
		    getu32(fs,
			ea_entry->val_size) > 256 ? 256 : getu32(fs,
			ea_entry->val_size));

		val[getu32(fs, ea_entry->val_size) > 256 ?
		    256 : getu32(fs, ea_entry->val_size)] = '\0';

		if (ea_entry->nidx == EXT2_EA_IDX_USER)
		    fprintf(hFile, "user.%s=%s\n", name, val);
		else if (ea_entry->nidx == EXT2_EA_IDX_TRUSTED)
		    fprintf(hFile, "trust.%s=%s\n", name, val);
		else if (ea_entry->nidx == EXT2_EA_IDX_SECURITY)
		    fprintf(hFile, "security.%s=%s\n", name, val);

	    }


	    /* POSIX ACL - setfacl / getfacl stuff */
	    else if ((ea_entry->nidx == EXT2_EA_IDX_POSIX_ACL_ACCESS) ||
		(ea_entry->nidx == EXT2_EA_IDX_POSIX_ACL_DEFAULT)) {

		ext2fs_pos_acl_entry_lo *acl_lo;
		ext2fs_pos_acl_head *acl_head;

		if (ea_entry->nidx == EXT2_EA_IDX_POSIX_ACL_ACCESS)
		    fprintf(hFile, "POSIX Access Control List Entries:\n");
		else if (ea_entry->nidx == EXT2_EA_IDX_POSIX_ACL_DEFAULT)
		    fprintf(hFile,
			"POSIX Default Access Control List Entries:\n");

		/* examine the header */
		acl_head =
		    (ext2fs_pos_acl_head *) &
		    buf[getu16(fs, ea_entry->val_off)];

		if (getu32(fs, acl_head->ver) != 1) {
		    fprintf(hFile, "Invalid ACL Header Version: %d\n",
			getu32(fs, acl_head->ver));
		    continue;
		}

		/* The first entry starts after the header */
		acl_lo =
		    (ext2fs_pos_acl_entry_lo *) ((uintptr_t) acl_head +
		    sizeof(ext2fs_pos_acl_head));


		/* Cycle through the values */
		while ((uintptr_t) acl_lo <
		    ((uintptr_t) buf +
			getu16(fs, ea_entry->val_off) + getu32(fs,
			    ea_entry->val_size))) {

		    char perm[64];
		    int len;

		    /* Make a string from the permissions */
		    ext2fs_make_acl_str(perm, 64,
			getu16(fs, acl_lo->perm));

		    switch (getu16(fs, acl_lo->tag)) {
		    case EXT2_PACL_TAG_USERO:

			fprintf(hFile, "  uid: %d: %s\n",
			    (int) fs_inode->uid, perm);
			len = sizeof(ext2fs_pos_acl_entry_sh);
			break;

		    case EXT2_PACL_TAG_GRPO:
			fprintf(hFile, "  gid: %d: %s\n",
			    (int) fs_inode->gid, perm);
			len = sizeof(ext2fs_pos_acl_entry_sh);
			break;
		    case EXT2_PACL_TAG_OTHER:
			fprintf(hFile, "  other: %s\n", perm);
			len = sizeof(ext2fs_pos_acl_entry_sh);
			break;
		    case EXT2_PACL_TAG_MASK:
			fprintf(hFile, "  mask: %s\n", perm);
			len = sizeof(ext2fs_pos_acl_entry_sh);
			break;


		    case EXT2_PACL_TAG_GRP:
			fprintf(hFile, "  gid: %d: %s\n",
			    getu32(fs, acl_lo->id), perm);
			len = sizeof(ext2fs_pos_acl_entry_lo);
			break;

		    case EXT2_PACL_TAG_USER:
			fprintf(hFile, "  uid: %d: %s\n",
			    getu32(fs, acl_lo->id), perm);

			len = sizeof(ext2fs_pos_acl_entry_lo);
			break;

		    default:
			fprintf(hFile, "Unknown ACL tag: %d\n",
			    getu16(fs, acl_lo->tag));
			len = sizeof(ext2fs_pos_acl_entry_sh);
			break;
		    }
		    acl_lo =
			(ext2fs_pos_acl_entry_lo *) ((uintptr_t) acl_lo +
			len);
		}
	    }
	    else {
		fprintf(hFile, "Unsupported Extended Attr Type: %d\n",
		    ea_entry->nidx);
	    }
	}
      egress_ea:

	free(buf);
    }

    if (sec_skew != 0) {
	fprintf(hFile, "\nAdjusted Inode Times:\n");
	fs_inode->mtime -= sec_skew;
	fs_inode->atime -= sec_skew;
	fs_inode->ctime -= sec_skew;

	fprintf(hFile, "Accessed:\t%s", ctime(&fs_inode->atime));
	fprintf(hFile, "File Modified:\t%s", ctime(&fs_inode->mtime));
	fprintf(hFile, "Inode Modified:\t%s", ctime(&fs_inode->ctime));

	if (fs_inode->dtime) {
	    fs_inode->dtime -= sec_skew;
	    fprintf(hFile, "Deleted:\t%s", ctime(&fs_inode->dtime));
	    fs_inode->dtime += sec_skew;
	}

	fs_inode->mtime += sec_skew;
	fs_inode->atime += sec_skew;
	fs_inode->ctime += sec_skew;

	fprintf(hFile, "\nOriginal Inode Times:\n");
    }
    else {
	fprintf(hFile, "\nInode Times:\n");
    }

    fprintf(hFile, "Accessed:\t%s", ctime(&fs_inode->atime));
    fprintf(hFile, "File Modified:\t%s", ctime(&fs_inode->mtime));
    fprintf(hFile, "Inode Modified:\t%s", ctime(&fs_inode->ctime));

    if (fs_inode->dtime)
	fprintf(hFile, "Deleted:\t%s", ctime(&fs_inode->dtime));

    if (numblock > 0)
	fs_inode->size = numblock * fs->block_size;

    fprintf(hFile, "\nDirect Blocks:\n");

    print.indir_idx = 0;
    print.idx = 0;
    print.hFile = hFile;

    if (fs->file_walk(fs, fs_inode, 0, 0,
	    (FS_FLAG_FILE_AONLY | FS_FLAG_FILE_META |
		FS_FLAG_FILE_NOID), print_addr_act, (void *) &print)) {
	fprintf(hFile, "\nError reading file:  ");
	tsk_error_print(hFile);
	tsk_errno = 0;
	tsk_errstr[0] = '\0';
    }
    else {

	if (print.idx != 0)
	    fprintf(hFile, "\n");

	/* print indirect blocks */
	if (print.indir_idx > 0) {
	    int i, printidx;
	    fprintf(hFile, "\nIndirect Blocks:\n");

	    printidx = 0;

	    for (i = 0; i < print.indir_idx; i++) {
		fprintf(hFile, "%" PRIuDADDR " ", print.indirl[i]);
		if (++printidx == 8) {
		    fprintf(hFile, "\n");
		    printidx = 0;
		}
	    }
	    if (printidx != 0)
		fprintf(hFile, "\n");
	}
    }

    fs_inode_free(fs_inode);
    return 0;
}


/* ext2fs_close - close an ext2fs file system */

static void
ext2fs_close(FS_INFO * fs)
{
    EXT2FS_INFO *ext2fs = (EXT2FS_INFO *) fs;

    free((char *) ext2fs->fs);
    if (ext2fs->dino_buf != NULL)
	free((char *) ext2fs->dino_buf);

    if (ext2fs->grp_buf != NULL)
	free((char *) ext2fs->grp_buf);

    if (ext2fs->bmap_buf != NULL)
	free((char *) ext2fs->bmap_buf);

    if (ext2fs->imap_buf != NULL)
	free((char *) ext2fs->imap_buf);

    free(ext2fs);
}

/* ext2fs_open - open an ext2fs file system 
 *
 * Return NULL on error (or not an Ext2 file system)
 * */

FS_INFO *
ext2fs_open(IMG_INFO * img_info, SSIZE_T offset, uint8_t ftype,
    uint8_t test)
{
    EXT2FS_INFO *ext2fs;
    unsigned int len;
    FS_INFO *fs;
    SSIZE_T cnt;

    if ((ftype & FSMASK) != EXTxFS_TYPE) {
	tsk_errno = TSK_ERR_FS_ARG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "Invalid FS Type in ext2fs_open");
	tsk_errstr2[0] = '\0';
	return NULL;
    }

    if ((ext2fs = (EXT2FS_INFO *) mymalloc(sizeof(*ext2fs))) == NULL)
	return NULL;

    fs = &(ext2fs->fs_info);

    fs->ftype = ftype;
    fs->flags = 0;

    /*
     * Read the superblock.
     */
    fs->img_info = img_info;
    fs->offset = offset;
    len = sizeof(ext2fs_sb);
    if ((ext2fs->fs = (ext2fs_sb *) mymalloc(len)) == NULL) {
	free(ext2fs);
	return NULL;
    }

    cnt =
	fs_read_random(fs, (char *) ext2fs->fs, len, (OFF_T) EXT2FS_SBOFF);
    if (cnt != len) {
	if (cnt != -1) {
	    tsk_errno = TSK_ERR_FS_READ;
	    tsk_errstr[0] = '\0';
	}
	snprintf(tsk_errstr2, TSK_ERRSTR_L, "ext2fs_open: superblock");
	free(ext2fs->fs);
	free(ext2fs);
	return NULL;
    }

    /* 
     * Verify we are looking at an EXTxFS image
     */
    if (fs_guessu16(fs, ext2fs->fs->s_magic, EXT2FS_FS_MAGIC)) {
	free(ext2fs->fs);
	free(ext2fs);
	tsk_errno = TSK_ERR_FS_MAGIC;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "not an EXTxFS file system (magic)");
	tsk_errstr2[0] = '\0';
	return NULL;
    }

    if (verbose) {
	if (getu32(fs, ext2fs->fs->s_feature_ro_compat) &
	    EXT2FS_FEATURE_RO_COMPAT_SPARSE_SUPER)
	    fprintf(stderr, "File system has sparse super blocks\n");

	fprintf(stderr, "First data block is %" PRIu32 "\n",
	    getu32(fs, ext2fs->fs->s_first_data_block));
    }

    /* If autodetect was given, look for the journal */
    if (ftype == EXTAUTO) {
	if (getu32(fs, ext2fs->fs->s_feature_compat) &
	    EXT2FS_FEATURE_COMPAT_HAS_JOURNAL)
	    fs->ftype = EXT3FS;
	else
	    fs->ftype = EXT2FS;
    }


    /* we need to figure out if dentries are v1 or v2 */
    if (getu32(fs, ext2fs->fs->s_feature_incompat) &
	EXT2FS_FEATURE_INCOMPAT_FILETYPE)
	ext2fs->deentry_type = EXT2_DE_V2;
    else
	ext2fs->deentry_type = EXT2_DE_V1;


    /*
     * Translate some filesystem-specific information to generic form.
     */
    fs->inum_count = getu32(fs, ext2fs->fs->s_inodes_count);
    fs->last_inum = fs->inum_count;
    fs->first_inum = EXT2FS_FIRSTINO;
    fs->root_inum = EXT2FS_ROOTINO;

    if (fs->inum_count < 10) {
	free(ext2fs->fs);
	free(ext2fs);
	tsk_errno = TSK_ERR_FS_MAGIC;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "Not an EXTxFS file system (inum count)");
	tsk_errstr2[0] = '\0';
	return NULL;
    }


    /* Set the size of the inode, but default to our data structure
     * size if it is larger */
    ext2fs->inode_size = getu16(fs, ext2fs->fs->s_inode_size);
    if (ext2fs->inode_size < sizeof(ext2fs_inode)) {
	ext2fs->inode_size = sizeof(ext2fs_inode);
	if (verbose)
	    fprintf(stderr, "SB inode size is too small, using default");
    }


    fs->dev_bsize = EXT2FS_DEV_BSIZE;
    fs->block_count = getu32(fs, ext2fs->fs->s_blocks_count);
    fs->first_block = 0;
    fs->last_block = fs->block_count - 1;
    ext2fs->first_data_block = getu32(fs, ext2fs->fs->s_first_data_block);


    if (getu32(fs, ext2fs->fs->s_log_block_size) !=
	getu32(fs, ext2fs->fs->s_log_frag_size)) {
	free(ext2fs->fs);
	free(ext2fs);
	tsk_errno = TSK_ERR_FS_FUNC;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "This file system has fragments that are a different size than blocks, which is not currently supported\nContact brian with details of the system that created this image");
	tsk_errstr2[0] = '\0';
	return NULL;
    }

    fs->block_size =
	EXT2FS_MIN_BLOCK_SIZE << getu32(fs, ext2fs->fs->s_log_block_size);

    /* The group descriptors are located in the block following the 
     * super block
     */

    ext2fs->groups_offset =
	roundup((EXT2FS_SBOFF + sizeof(ext2fs_sb)), fs->block_size);

    ext2fs->groups_count = (getu32(fs, ext2fs->fs->s_blocks_count) -
	ext2fs->first_data_block +
	getu32(fs,
	    ext2fs->fs->s_blocks_per_group) -
	1) / getu32(fs, ext2fs->fs->s_blocks_per_group);

    /* callbacks */
    fs->inode_walk = ext2fs_inode_walk;
    fs->block_walk = ext2fs_block_walk;
    fs->inode_lookup = ext2fs_inode_lookup;
    fs->dent_walk = ext2fs_dent_walk;
    fs->file_walk = ext2fs_file_walk;
    fs->fsstat = ext2fs_fsstat;
    fs->fscheck = ext2fs_fscheck;
    fs->istat = ext2fs_istat;
    fs->close = ext2fs_close;


    /* Journal */
    fs->journ_inum = getu32(fs, ext2fs->fs->s_journal_inum);
    fs->jblk_walk = ext2fs_jblk_walk;
    fs->jentry_walk = ext2fs_jentry_walk;
    fs->jopen = ext2fs_jopen;

    /* allocate buffers */

    /* inode map */
    ext2fs->imap_buf = NULL;
    ext2fs->imap_grp_num = 0xffffffff;

    /* block map */
    ext2fs->bmap_buf = NULL;
    ext2fs->bmap_grp_num = 0xffffffff;

    /* dinode */
    ext2fs->dino_buf = NULL;
    ext2fs->dino_inum = 0xffffffff;

    /* group descriptor */
    ext2fs->grp_buf = NULL;
    ext2fs->grp_num = 0xffffffff;


    /*
     * Print some stats.
     */
    if (verbose)
	fprintf(stderr,
	    "inodes %" PRIu32 " root ino %" PRIuINUM " blocks %" PRIu32
	    " blocks/group %" PRIu32 "\n", getu32(fs,
		ext2fs->fs->
		s_inodes_count),
	    fs->root_inum, getu32(fs, ext2fs->fs->s_blocks_count),
	    getu32(fs, ext2fs->fs->s_blocks_per_group));

    return (fs);
}
