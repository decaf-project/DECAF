/*
** The Sleuth Kit
**
** This software is subject to the IBM Public License ver. 1.0,
** which was displayed prior to download and is included in the readme.txt
** file accompanying the Sleuth Kit files.  It may also be requested from:
** Crucial Security Inc.
** 14900 Conference Center Drive
** Chantilly, VA 20151
**
** Wyatt Banks [wbanks@crucialsecurity.com]
** Copyright (c) 2005 Crucial Security Inc.  All rights reserved.
**
** Brian Carrier [carrier@sleuthkit.org]
** Copyright (c) 2003-2005 Brian Carrier.  All rights reserved
**
** Copyright (c) 1997,1998,1999, International Business Machines
** Corporation and others. All Rights Reserved.
*/

/* TCT
 * LICENSE
 *      This software is distributed under the IBM Public License.
 * AUTHOR(S)
 *      Wietse Venema
 *      IBM T.J. Watson Research
 *      P.O. Box 704
 *      Yorktown Heights, NY 10598, USA
 --*/

/*
** You may distribute the Sleuth Kit, or other software that incorporates
** part of all of the Sleuth Kit, in object code form under a license agreement,
** provided that:
** a) you comply with the terms and conditions of the IBM Public License
**    ver 1.0; and
** b) the license agreement
**     i) effectively disclaims on behalf of all Contributors all warranties
**        and conditions, express and implied, including warranties or
**        conditions of title and non-infringement, and implied warranties
**        or conditions of merchantability and fitness for a particular
**        purpose.
**    ii) effectively excludes on behalf of all Contributors liability for
**        damages, including direct, indirect, special, incidental and
**        consequential damages such as lost profits.
**   iii) states that any provisions which differ from IBM Public License
**        ver. 1.0 are offered by that Contributor alone and not by any
**        other party; and
**    iv) states that the source code for the program is available from you,
**        and informs licensees how to obtain it in a reasonable manner on or
**        through a medium customarily used for software exchange.
**
** When the Sleuth Kit or other software that incorporates part or all of
** the Sleuth Kit is made available in source code form:
**     a) it must be made available under IBM Public License ver. 1.0; and
**     b) a copy of the IBM Public License ver. 1.0 must be included with
**        each copy of the program.
*/

#include "fs_tools.h"
#include "hfs.h"
#include "fs_data.h"
#include "hfs.h"


#include <ctype.h>		/* for isprint in hfs_uni2ascii() */


#define MAX_DEPTH   64
#define DIR_STRSZ 2048


#define hfs_is_deleted_leaf(b, c) \
        hfs_is_bit_b_alloc(b, c)


#define hfs_is_block_alloc(b, c) \
        hfs_is_bit_b_alloc(b, c)


typedef struct {
    /* recursive path stuff */

    /* how deep in the directory tree are we */
    unsigned int depth;

    /* pointer in dirs string to where '/' is for */
    char *didx[MAX_DEPTH];

    /* The current directory name string */
    char dirs[DIR_STRSZ];
} HFS_DINFO;


static void
 hfs_dent_walk_lcl(FS_INFO *, HFS_DINFO *, INUM_T, int,
    FS_DENT_WALK_FN, void *);


/* convert unicode to ascii */
void
hfs_uni2ascii(FS_INFO * fs, char *uni, int ulen, char *asc, int alen)
{
    int i, l;

    /* find the maximum that we can go
     * we will break when we hit NULLs, but this is the
     * absolute max
     */
    if ((ulen + 1) > alen)
	l = alen - 1;
    else
	l = ulen;

    if (fs->endian & TSK_LIT_ENDIAN) {

	for (i = 0; i < l; i++) {
	    /* If this value is NULL, then stop */
	    if (uni[i * 2] == 0 && uni[i * 2 + 1] == 0)
		break;

	    if (isprint((int) uni[i * 2]))
		asc[i] = uni[i * 2];
	    else
		asc[i] = '?';
	}

	/* handle big endian unicode - TODO: suggest changing unicode.c to Brian */

	/* also, HFS has some filenames that have nulls in their name, so stopping
	 * on the first null can be bad.  one example is the file 
	 * HFS+ Private Data which is preceded by 4 nulls in its name.
	 *
	 * read about that here:
	 * http://developer.apple.com/technotes/tn/tn1150.html#HardLinks
	 */

    }
    else {

	for (i = 0; i < l; i++) {
	    /* If this value is NULL, then stop */
	    if (uni[i * 2] == 0 && uni[i * 2 + 1] == 0)
		break;

	    if (isprint((int) uni[(i * 2) + 1]))
		asc[i] = uni[(i * 2) + 1];
	    else
		asc[i] = '?';
	}
    }

    /* NULL Terminate */
    asc[i] = '\0';
    return;
}


void
hfs_dent_walk(FS_INFO * fs, INUM_T inum, int flags,
    FS_DENT_WALK_FN action, void *ptr)
{
    HFS_DINFO dinfo;
    memset(&dinfo, 0, sizeof(HFS_DINFO));
    hfs_dent_walk_lcl(fs, &dinfo, inum, flags, action, ptr);
}


void
hfs_dent_walk_lcl(FS_INFO * fs, HFS_DINFO * dinfo, INUM_T inum,
    int flags, FS_DENT_WALK_FN action, void *ptr)
{
    FS_DENT *fs_dent = fs_dent_alloc(HFS_MAXNAMLEN, 0);
    HFS_INFO *hfs = (HFS_INFO *) fs;
    uint8_t ucs_fn[HFS_MAXNAMLEN * 2];
    uint16_t filetype;
    FS_INODE *fs_inode;
    DADDR_T offs;
    int keylen;
    uint8_t key_len[2];
    int myflags = 0, j;
    INUM_T newinum;
    hfs_inode_struct *in;

    if (verbose)
	fprintf(stderr, "hfs_dent_walk: Processing directory %lu\n",
	    (ULONG) inum);

    if (inum < fs->first_inum || inum > fs->last_inum)
	error("invalid inode value: %i\n", inum);

    fs_inode = fs->inode_lookup(fs, inum);

    filetype = getu16(fs, hfs->cat->rec_type);

    /*
     * "."
     */
    fs_dent->inode = inum;
    strcpy(fs_dent->name, ".");

    /* copy the path data */
    fs_dent->path = dinfo->dirs;
    fs_dent->pathdepth = dinfo->depth;
    fs_dent->fsi = fs->inode_lookup(fs, fs_dent->inode);

    fs_dent->ent_type = FS_DENT_DIR;

    if (WALK_STOP == action(fs, fs_dent, myflags, ptr)) {
	fs_dent_free(fs_dent);
	return;
    }

    /*
     * ".."
     */

    in = hfs->inodes + inum;

    /* the parent of root is 1, but there is no inode 1 */
    if (inum == HFS_ROOT_INUM)
	newinum = HFS_ROOT_INUM;
    else
	newinum = in->parent;

    fs_dent->inode = newinum;
    strcpy(fs_dent->name, "..");

    /* copy the path data */
    fs_dent->path = dinfo->dirs;
    fs_dent->pathdepth = dinfo->depth;
    fs_dent->fsi = fs->inode_lookup(fs, fs_dent->inode);

    fs_dent->ent_type = FS_DENT_DIR;

    if (WALK_STOP == action(fs, fs_dent, myflags, ptr)) {
	fs_dent_free(fs_dent);
	return;
    }

    /* because hfs->key (the filename offset) is reset with each lookup, this is necessary here... */
    fs_dent->inode = inum;
    fs_dent->path = dinfo->dirs;
    fs_dent->pathdepth = dinfo->depth;
    fs_dent->fsi = fs->inode_lookup(fs, fs_dent->inode);

    if (filetype == HFS_FOLDER_RECORD) {

	for (j = HFS_ROOT_INUM; j <= fs->last_inum; j++) {

	    in = hfs->inodes + j;

	    if (in->parent == inum) {

		newinum = j;

		fs_dent->inode = newinum;
		fs_dent->path = dinfo->dirs;
		fs_dent->pathdepth = dinfo->depth;
		fs_dent->fsi = fs->inode_lookup(fs, fs_dent->inode);

		offs = hfs->key;
		fs_read_random(fs, key_len, 2, offs);

		keylen = getu16(fs, key_len);
		keylen -= 6;
		offs += 8;

		fs_read_random(fs, ucs_fn, keylen, offs);

		hfs_uni2ascii(fs, ucs_fn, keylen / 2, fs_dent->name,
		    HFS_MAXNAMLEN);

		fs_inode = fs->inode_lookup(fs, newinum);

		filetype = getu16(fs, hfs->cat->rec_type);

		if (filetype == HFS_FOLDER_RECORD)
		    fs_dent->ent_type = FS_DENT_DIR;
		else
		    fs_dent->ent_type = FS_DENT_REG;

		myflags |= FS_FLAG_NAME_ALLOC;

		if ((flags & myflags) == myflags) {

		    if (WALK_STOP == action(fs, fs_dent, myflags, ptr)) {
			fs_dent_free(fs_dent);
			return;
		    }
		}

		if (filetype == HFS_FOLDER_RECORD) {
		    if (flags & FS_FLAG_NAME_RECURSE) {
			if (dinfo->depth < MAX_DEPTH) {
			    dinfo->didx[dinfo->depth] =
				&dinfo->dirs[strlen(dinfo->dirs)];
			    strncpy(dinfo->didx[dinfo->depth],
				fs_dent->name,
				DIR_STRSZ - strlen(dinfo->dirs));
			    strncat(dinfo->dirs, "/", DIR_STRSZ);
			}
			dinfo->depth++;

			hfs_dent_walk_lcl(fs, dinfo, newinum, flags,
			    action, ptr);
			dinfo->depth--;

			if (dinfo->depth < MAX_DEPTH)
			    *dinfo->didx[dinfo->depth] = '\0';
		    }
		}



	    }
	}

	/* regular file */
    }
    else {

	fs_dent->inode = inum;
	offs = hfs->key;
	fs_read_random(fs, key_len, 2, offs);
	keylen = getu16(fs, key_len);

	/* skip key length and parent cnid fields */
	keylen -= 6;
	offs += 8;

	fs_read_random(fs, ucs_fn, keylen, offs);

	hfs_uni2ascii(fs, ucs_fn, keylen / 2, fs_dent->name,
	    HFS_MAXNAMLEN);

	fs_dent->path = dinfo->dirs;
	fs_dent->pathdepth = dinfo->depth;
	fs_dent->fsi = fs->inode_lookup(fs, fs_dent->inode);

	fs_dent->ent_type = FS_DENT_REG;

	if (WALK_STOP == action(fs, fs_dent, myflags, ptr)) {
	    fs_dent_free(fs_dent);
	    return;
	}
    }

}
