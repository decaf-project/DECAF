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
#include "fs_types.h"
#include "iso9660.h"
#include "mymalloc.h"
#include "fs_data.h"
#include <ctype.h>
#include <netinet/in.h>
#include "fs_unicode.h"

/* free all memory used by inode linked list */
void
iso9660_inode_list_free(FS_INFO * fs)
{
    ISO_INFO *iso = (ISO_INFO *) fs;
    in_node *tmp;

    while (iso->in) {
	tmp = iso->in;
	iso->in = iso->in->next;
	free(tmp);
    }
    iso->in = NULL;
}

/* rockridge extensions are passed in 'buf'.  buf is 'count' bytes long 
 *
 * Returns NULL on error 
 */
rockridge_ext *
parse_rockridge(FS_INFO * fs, char *buf, int count)
{
    rr_px_entry *rr_px;
    rr_pn_entry *rr_pn;
    rr_sl_entry *rr_sl;
    rr_nm_entry *rr_nm;
    rr_cl_entry *rr_cl;
    rr_re_entry *rr_re;
    rr_tf_entry *rr_tf;
    rr_sf_entry *rr_sf;
    rockridge_ext *rr;
    ISO_INFO *iso = (ISO_INFO *) fs;

    char *end = buf + count - 1;

    if (verbose)
	fprintf(stderr, "parse_rockridge: count is: %" PRIu32 " ", count);

    rr = (rockridge_ext *) mymalloc(sizeof(rockridge_ext));
    if (rr == NULL) {
	return NULL;
    }

    while (buf < end) {
	/* SP is a system use field, not part of RockRidge */
	if ((buf[0] == 'S') && (buf[1] == 'P')) {
	    buf += sizeof(sp_sys_use);
	}
	/* RR is a system use field indicating RockRidge, but not part of RockRidge */
	else if ((buf[0] == 'R') && (buf[1] == 'R')) {
	    iso->rr_found = 1;
	    buf += sizeof(rr_sys_use);
	}
	/* POSIX file attributes */
	else if ((buf[0] == 'P') && (buf[1] == 'X')) {
	    rr_px = (rr_px_entry *) buf;
	    rr->uid = parseu32(fs, rr_px->uid);
	    rr->gid = parseu32(fs, rr_px->gid);
	    rr->mode = parseu16(fs, rr_px->mode);
	    rr->nlink = parseu32(fs, rr_px->links);
	    buf += sizeof(rr_px_entry);
	}
	else if ((buf[0] == 'P') && (buf[1] == 'N')) {
	    rr_pn = (rr_pn_entry *) buf;
	    buf += sizeof(rr_pn_entry);
	}
	else if ((buf[0] == 'S') && (buf[1] == 'L')) {
	    rr_sl = (rr_sl_entry *) buf;
	    buf += sizeof(rr_sl_entry);
	}
	else if ((buf[0] == 'N') && (buf[1] == 'M')) {
	    rr_nm = (rr_nm_entry *) buf;
	    strncpy(rr->fn, &buf[5], (int) (*rr_nm->len) - 5);
	    rr->fn[(int) (*rr_nm->len) - 5] = '\0';
	    buf += (int) buf[2];
	}
	else if ((buf[0] == 'C') && (buf[1] == 'L')) {
	    rr_cl = (rr_cl_entry *) buf;
	    buf += sizeof(rr_cl_entry);
	}
	else if ((buf[0] == 'R') && (buf[1] == 'E')) {
	    rr_re = (rr_re_entry *) buf;
	    buf += sizeof(rr_re_entry);
	}
	else if ((buf[0] == 'T') && (buf[1] == 'F')) {
	    rr_tf = (rr_tf_entry *) buf;
	    buf += (int) buf[2];
	}
	else if ((buf[0] == 'S') && (buf[1] == 'F')) {
	    rr_sf = (rr_sf_entry *) buf;
	    buf += sizeof(rr_sf_entry);
	}
	else if ((buf[0] == 'C') && (buf[1] == 'E')) {
	    buf += (int) buf[2];
	}
	else
	    buf = end;
    }

    return rr;
}

/* get directory entries from current directory and add them to the inode list.
 * Type: ISO9660_TYPE_PVD for primary volume descriptor, ISO9660_TYPE_SVD for
 * supplementary volume descriptor (do Joliet utf-8 conversion).
 *
 * Return number of inodes or -1 on error
 */

int
iso9660_get_dentrys(FS_INFO * fs, OFF_T offs, int count, int type,
    char *fn)
{
    ISO_INFO *iso = (ISO_INFO *) fs;
    iso9660_dentry dd;		/* directory descriptor */
    in_node *in;		/* inode we'll build */
    uint16_t uni_buf[ISO9660_MAXNAMLEN];	/* temp hold UCS-2 chars */
    char *a, *b;		/* handles directory holes */
    in_node *tmp;
    int dentry_bytes;		/* bytes left this dentry */
    int dir_bytes;		/* bytes left this directory */
    int fn_skip;		/* filename skip size */
    UTF16 *name16;
    UTF8 *name8;
    int retVal;
    SSIZE_T cnt;
    char *file_ver;

    if (verbose)
	fprintf(stderr,
	    "iso9660_get_dentrys: fs: %lu offs: %lu"
	    " count: %" PRIu32 " type: %" PRIu32 " fn: %lu\n",
	    (ULONG) fs, (ULONG) offs, count, type, (ULONG) fn);

    in = (in_node *) mymalloc(sizeof(in_node));
    if (in == NULL) {
	return -1;
    }

    cnt =
	fs_read_random(fs, (char *) &(in->inode.dr),
	sizeof(iso9660_dentry), offs);
    if (cnt != sizeof(iso9660_dentry)) {
	if (cnt != -1) {
	    tsk_errno = TSK_ERR_FS_READ;
	    tsk_errstr[0] = '\0';
	}
	snprintf(tsk_errstr2, TSK_ERRSTR_L, "iso_get_dentries");
	return -1;
    }

    offs += sizeof(iso9660_dentry);

    /* find how many bytes in this directory total */
    dir_bytes =
	parseu32(fs, in->inode.dr.data_len) - (int) sizeof(iso9660_dentry);

    /* figure how many bytes are left including file name, rockridge, etc */
    dentry_bytes = (int) (in->inode.dr.length - sizeof(iso9660_dentry));

    /* skip file name and padding byte */
    fn_skip = in->inode.dr.len + (in->inode.dr.len % 2) ? 1 : 0;
    offs += fn_skip;
    dentry_bytes -= fn_skip;
    dir_bytes -= fn_skip;

    /* add file name for "." taken from path table */
    strcpy(in->inode.fn, fn);

    if (dentry_bytes > 0) {
	char *buf;		/* used to hold rockridge info */
	if ((buf = mymalloc(dentry_bytes)) == NULL) {
	    return -1;
	}
	cnt = fs_read_random(fs, buf, dentry_bytes, offs);
	if (cnt != dentry_bytes) {
	    if (cnt != -1) {
		tsk_errno = TSK_ERR_FS_READ;
		tsk_errstr[0] = '\0';
	    }
	    snprintf(tsk_errstr2, TSK_ERRSTR_L, "iso_get_dentries");
	    return -1;
	}
	in->inode.rr = parse_rockridge(fs, buf, dentry_bytes);
	if (in->inode.rr == NULL) {
	    return -1;
	}
	dir_bytes -= dentry_bytes;
	free(buf);
	offs += dentry_bytes;
    }

    in->inum = count;
    in->offset = parseu32(fs, in->inode.dr.ext_loc) * fs->block_size;
    in->size = parseu32(fs, dd.data_len);

    /* add inode to the list */

    tmp = iso->in;

    /* inode list not empty */
    if (iso->in) {

	while ((in->offset != tmp->offset) && (tmp->next))
	    tmp = tmp->next;

	/* directory is already in list, but not its filename */
	if (in->offset == tmp->offset) {

	    strcpy(tmp->inode.fn, in->inode.fn);

	    file_ver = strchr(in->inode.fn, ';');
	    if (file_ver) {
		in->inode.version = atoi(file_ver + 1);
		*file_ver = '\0';
		file_ver = NULL;
	    }

	    if ((in->inode.rr) && (!tmp->inode.rr)) {
		tmp->inode.rr = in->inode.rr;
		in->inode.rr = NULL;
	    }

	    if (in->inode.rr)
		free(in->inode.rr);
	    free(in);

	    /* directory not in list */
	}
	else {
	    tmp->next = in;
	    in->next = NULL;
	    count++;
	}
    }
    /* inode list was empty */
    else {
	iso->in = in;
	in->next = NULL;
	count++;
    }

    /* process rest of directory */
    while (dir_bytes > (int) sizeof(iso9660_dentry)) {
	cnt =
	    fs_read_random(fs, (char *) &dd, sizeof(iso9660_dentry), offs);
	if (cnt != sizeof(iso9660_dentry)) {
	    if (cnt != -1) {
		tsk_errno = TSK_ERR_FS_READ;
		tsk_errstr[0] = '\0';
	    }
	    snprintf(tsk_errstr2, TSK_ERRSTR_L, "iso_get_dentries");
	    return -1;
	}

	dentry_bytes = dd.length - sizeof(iso9660_dentry);
	dir_bytes -= sizeof(iso9660_dentry);

	if (dd.length > 0) {
	    offs += sizeof(iso9660_dentry);
	    in = (in_node *) mymalloc(sizeof(in_node));
	    if (in == NULL) {
		return -1;
	    }

	    /* do unicode to utf-8 conversion */
	    if (type == ISO9660_TYPE_SVD) {
		file_ver = NULL;
		/* get UCS-2 filename */
		cnt = fs_read_random(fs, (char *) uni_buf, dd.len, offs);
		if (cnt != dd.len) {
		    if (cnt != -1) {
			tsk_errno = TSK_ERR_FS_READ;
			tsk_errstr[0] = '\0';
		    }
		    snprintf(tsk_errstr2, TSK_ERRSTR_L,
			"iso_get_dentries");
		    return -1;
		}
		offs += dd.len;

		if (fs->endian & TSK_LIT_ENDIAN) {
		    int i = 0;

		    /* ISO9660 uses big endian UCS-2 chars */
		    while (i < dd.len) {
			uni_buf[i] = ((uni_buf[i] & 0xff) << 8) +
			    ((uni_buf[i] & 0xff00) >> 8);
		    }
		}
		name16 = (UTF16 *) uni_buf;
		name8 = (UTF8 *) iso->dinode->fn;

		retVal = fs_UTF16toUTF8(fs, (const UTF16 **) &name16,
		    (UTF16 *) ((uintptr_t) name16 +
			dd.len), &name8,
		    (UTF8 *) ((uintptr_t) name8 + dd.len / 2),
		    lenientConversion);
		if (retVal != conversionOK) {
		    if (verbose)
			fprintf(stderr,
			    "fsstat: Error converting Joliet name to UTF8: %d",
			    retVal);
		    iso->dinode->fn[0] = '\0';
		}
		file_ver = strchr(iso->dinode->fn, ';');
		if (file_ver) {
		    iso->dinode->version = atoi(file_ver + 1);
		    *file_ver = '\0';
		    file_ver = NULL;
		}
	    }
	    else {
		cnt = fs_read_random(fs, in->inode.fn, dd.len, offs);
		if (cnt != dd.len) {
		    if (cnt != -1) {
			tsk_errno = TSK_ERR_FS_READ;
			tsk_errstr[0] = '\0';
		    }
		    snprintf(tsk_errstr2, TSK_ERRSTR_L,
			"iso_get_dentries");
		    return -1;
		}
		offs += dd.len;
		in->inode.fn[dd.len] = '\0';
		file_ver = strchr(in->inode.fn, ';');
		if (file_ver) {
		    in->inode.version = atoi(file_ver + 1);
		    *file_ver = '\0';
		    file_ver = NULL;
		}
	    }

	    dentry_bytes -= dd.len;

	    /* skip past padding byte */
	    if (!(dd.len % 2)) {
		offs++;
		dir_bytes--;
		dentry_bytes--;
	    }

	    memcpy(&(in->inode.dr), &dd, sizeof(iso9660_dentry));

	    in->inode.ea = NULL;
	    in->offset = parseu32(fs, dd.ext_loc) * fs->block_size;

	    /* Found data after dentry, possibly RockRidge */
	    if (dentry_bytes) {
		char *buf = mymalloc(dentry_bytes);
		if (buf == NULL) {
		    return -1;
		}
		cnt = fs_read_random(fs, buf, dentry_bytes, offs);
		if (cnt != dentry_bytes) {
		    if (cnt != -1) {
			tsk_errno = TSK_ERR_FS_READ;
			tsk_errstr[0] = '\0';
		    }
		    snprintf(tsk_errstr2, TSK_ERRSTR_L,
			"iso_get_dentries");
		    return -1;
		}
		offs += dentry_bytes;
		in->inode.rr = parse_rockridge(fs, buf, dentry_bytes);
		if (in->inode.rr == NULL) {
		    return -1;
		}
		dir_bytes -= dentry_bytes;
		free(buf);
	    }

	    /* record size to make sure fifos show up as unique files */
	    in->size = parseu32(fs, in->inode.dr.data_len);
	    in->inum = count;

	    /* add inode to the list */

	    /* list not empty */
	    if (iso->in) {
		tmp = iso->in;
		while ((tmp->next)
		    && ((in->offset != tmp->offset) || (!in->size)
			|| (!tmp->size)))
		    tmp = tmp->next;

		/* file already in list */
		if ((in->offset == tmp->offset) && (in->size)
		    && (tmp->size)) {
		    if ((in->inode.rr) && (!tmp->inode.rr)) {
			tmp->inode.rr = in->inode.rr;
			in->inode.rr = NULL;
		    }
		    if (in->inode.rr)
			free(in->inode.rr);

		    free(in);
		    /* file wasn't in list, add it */
		}
		else {
		    tmp->next = in;
		    in->next = NULL;
		    count++;
		}

		/* list is empty */
	    }
	    else {
		iso->in = in;
		in->next = NULL;
	    }

	    dir_bytes -= dd.len;

	    /* oddity:
	     * We might be inside the directory's block slack if we found
	     * a 0, or we might be between directory entries (found
	     * a directory with a hole in it).
	     */
	}
	else {
	    a = (char *) &dd;
	    b = a + sizeof(dd);
	    while ((*a == 0) && (a != b))
		a++;
	    if (a != b)
		dir_bytes += sizeof(dd) - (int) (b - a);
	    offs += (int) (a - (char *) &dd);
	}
    }

    return count;
}

/* search through all volume descriptors and find files, adding them to a linked list where
 * each item is unique if its extent location is unique.
 *
 * Return -1 on error or count of XXX
 */
int
iso9660_find_inodes(ISO_INFO * iso)
{
    int count = 0;
    int pt_bytes;		/* bytes left in path table */
    path_table_rec dir;		/* directory we are working on */
    FS_INFO *fs = (FS_INFO *) & iso->fs_info;
    svd_node *s = iso->svd;
    pvd_node *p = iso->pvd;
    char fn[ISO9660_MAXNAMLEN];	/* store current directory name */
    uint16_t uni_buf[ISO9660_MAXNAMLEN];	/* hold UCS-2 chars for processing */
    off_t offs;			/* offset of where we are in path table */
    off_t extent;		/* offset of extent for current directory */
    UTF16 *name16;
    UTF8 *name8;
    int retVal;
    SSIZE_T cnt;
    char *file_ver;

    if (verbose)
	fprintf(stderr, "iso9660_find_inodes: iso: %lu\n", (ULONG) iso);

    /* initialize in case repeatedly called */
    iso9660_inode_list_free(fs);
    iso->in = NULL;

    if (s)
	fs->block_size = parseu16(fs, s->svd.blk_sz);
    else
	fs->block_size = parseu16(fs, p->pvd.blk_sz);

    /* search all supplementary volume descriptors for unique files */
    while (s) {

	if (fs->endian & TSK_LIT_ENDIAN)
	    offs = (off_t) (getu32(fs, s->svd.loc_l) * fs->block_size);
	else
	    offs = (off_t) (getu32(fs, s->svd.loc_m) * fs->block_size);

	pt_bytes = parseu32(fs, s->svd.path_size);

	while (pt_bytes > 0) {
	    file_ver = NULL;
	    /* get next dir... */
	    cnt =
		fs_read_random(fs, (char *) &dir, (int) sizeof(dir), offs);
	    if (cnt != sizeof(dir)) {
		if (cnt != -1) {
		    tsk_errno = TSK_ERR_FS_READ;
		    tsk_errstr[0] = '\0';
		}
		snprintf(tsk_errstr2, TSK_ERRSTR_L, "iso_find_inodes");
		return -1;
	    }
	    pt_bytes -= sizeof(path_table_rec);
	    offs += (int) sizeof(path_table_rec);

	    /* get UCS-2 filename */
	    cnt = fs_read_random(fs, (char *) uni_buf, dir.len_di, offs);
	    if (cnt != sizeof(dir)) {
		if (cnt != -1) {
		    tsk_errno = TSK_ERR_FS_READ;
		    tsk_errstr[0] = '\0';
		}
		snprintf(tsk_errstr2, TSK_ERRSTR_L, "iso_find_inodes");
		return -1;
	    }
	    pt_bytes -= dir.len_di;
	    offs += dir.len_di;

	    /* do unicode to utf-8 conversion */
	    if (fs->endian & TSK_LIT_ENDIAN) {
		int i = 0;

		while (i < dir.len_di) {
		    uni_buf[i] = ((uni_buf[i] & 0xff) << 8) +
			((uni_buf[i] & 0xff00) >> 8);
		}
	    }

	    name16 = (UTF16 *) uni_buf;
	    name8 = (UTF8 *) iso->dinode->fn;

	    retVal = fs_UTF16toUTF8(fs, (const UTF16 **) &name16,
		(UTF16 *) ((uintptr_t) name16 +
		    dir.len_di), &name8,
		(UTF8 *) ((uintptr_t) name8 + dir.len_di / 2),
		lenientConversion);
	    if (retVal != conversionOK) {
		if (verbose)
		    fprintf(stderr,
			"fsstat: Error converting Joliet name to UTF8: %d",
			retVal);
		iso->dinode->fn[0] = '\0';
	    }

	    file_ver = strchr(iso->dinode->fn, ';');
	    if (file_ver) {
		iso->dinode->version = atoi(file_ver + 1);
		*file_ver = '\0';
		file_ver = NULL;
	    }

	    /* padding byte is there if strlen(file name) is odd */
	    if (dir.len_di % 2) {
		pt_bytes--;
		offs++;
	    }

	    extent = (off_t) (parseu16(fs, dir.ext_loc) * fs->block_size);

	    count =
		iso9660_get_dentrys(fs, extent, count, ISO9660_TYPE_SVD,
		fn);

	    if (count == -1) {
		return -1;
	    }
	}
	s = s->next;
    }

    /* search all primary volume descriptors for unique files */
    while (p) {

	if (fs->endian & TSK_LIT_ENDIAN)
	    offs = (off_t) (getu32(fs, p->pvd.loc_l) * fs->block_size);
	else
	    offs = (off_t) (getu32(fs, p->pvd.loc_m) * fs->block_size);

	pt_bytes = parseu32(fs, p->pvd.path_size);

	while (pt_bytes > 0) {

	    /* get next dir... */
	    cnt = fs_read_random(fs, (char *) &dir, sizeof(dir), offs);
	    if (cnt != sizeof(dir)) {
		if (cnt != -1) {
		    tsk_errno = TSK_ERR_FS_READ;
		    tsk_errstr[0] = '\0';
		}
		snprintf(tsk_errstr2, TSK_ERRSTR_L, "iso_find_inodes");
		return -1;
	    }
	    pt_bytes -= sizeof(path_table_rec);
	    offs += sizeof(path_table_rec);

	    /* get directory name, this is the only chance */
	    cnt = fs_read_random(fs, fn, dir.len_di, offs);
	    if (cnt != dir.len_di) {
		if (cnt != -1) {
		    tsk_errno = TSK_ERR_FS_READ;
		    tsk_errstr[0] = '\0';
		}
		snprintf(tsk_errstr2, TSK_ERRSTR_L, "iso_find_inodes");
		return -1;
	    }
	    fn[dir.len_di] = '\0';

	    pt_bytes -= dir.len_di;
	    offs += dir.len_di;

	    /* padding byte is there if strlen(file name) is odd */
	    if (dir.len_di % 2) {
		pt_bytes--;
		offs++;
	    }

	    extent = (off_t) (parseu16(fs, dir.ext_loc) * fs->block_size);

	    count =
		iso9660_get_dentrys(fs, extent, count, ISO9660_TYPE_PVD,
		fn);

	    if (count == -1) {
		return -1;
	    }
	}
	p = p->next;
    }
    return count;
}

/*
 * Load the raw "inode" into the cached buffer (iso->dinode)
 *
 * dinode_load (for now) does not check for extended attribute records...
 * my issue is I dont have an iso9660 image with extended attr recs, so I
 * can't test/debug, etc
 */
uint8_t
iso9660_dinode_load(ISO_INFO * iso, INUM_T inum)
{
    in_node *n;

    if (verbose)
	fprintf(stderr, "iso9660_dinode_load: iso: %lu"
	    " inum: %" PRIuINUM "\n", (ULONG) iso, inum);

    n = iso->in;
    while (n && (n->inum != inum))
	n = n->next;

    iso->dinum = inum;

    if (n)
	memcpy(iso->dinode, &n->inode, sizeof(iso9660_inode));
    else {
	return 1;
    }
    return 0;
}

/* copy cached disk inode into generic structure */
static void
iso9660_dinode_copy(ISO_INFO * iso, FS_INODE * fs_inode)
{
    FS_INFO *fs = (FS_INFO *) & iso->fs_info;
    struct tm t;

    if (verbose)
	fprintf(stderr, "iso9660_dinode_copy: iso: %lu"
	    " inode: %lu\n", (ULONG) iso, (ULONG) fs_inode);

    fs_inode->addr = iso->dinum;
    fs_inode->size = parseu32(fs, iso->dinode->dr.data_len);

    t.tm_sec = iso->dinode->dr.rec.sec;
    t.tm_min = iso->dinode->dr.rec.min;
    t.tm_hour = iso->dinode->dr.rec.hour;
    t.tm_mday = iso->dinode->dr.rec.day;
    t.tm_mon = iso->dinode->dr.rec.month - 1;
    t.tm_year = iso->dinode->dr.rec.year;

    fs_inode->mtime = fs_inode->atime = fs_inode->ctime = mktime(&t);

    if (iso->dinode->ea) {
	fs_inode->uid = getu32(fs, iso->dinode->ea->uid);
	fs_inode->gid = getu32(fs, iso->dinode->ea->gid);
	fs_inode->mode = getu16(fs, iso->dinode->ea->mode);
	fs_inode->nlink = 1;
    }
    else {
	if (iso->dinode->dr.flags & ISO9660_FLAG_DIR)
	    fs_inode->mode = FS_INODE_DIR;
	else
	    fs_inode->mode = FS_INODE_REG;

	fs_inode->nlink = 1;
	fs_inode->uid = 0;
	fs_inode->gid = 0;
    }


    fs_inode->direct_addr[0] =
	(DADDR_T) parseu32(fs, iso->dinode->dr.ext_loc);

    fs_inode->flags = 0;
}

static FS_INODE *
iso9660_inode_lookup(FS_INFO * fs, INUM_T inum)
{
    ISO_INFO *iso = (ISO_INFO *) fs;
    FS_INODE *fs_inode;

    if ((fs_inode =
	    fs_inode_alloc(ISO9660_NDADDR, ISO9660_NIADDR)) == NULL)
	return NULL;

    if (verbose)
	fprintf(stderr, "iso9660_inode_lookup: iso: %lu"
	    " inum: %" PRIuINUM "\n", (ULONG) iso, inum);

    if (iso9660_dinode_load(iso, inum)) {
	fs_inode_free(fs_inode);
	return NULL;
    }

    iso9660_dinode_copy(iso, fs_inode);

    return fs_inode;
}

uint8_t
iso9660_inode_walk(FS_INFO * fs, INUM_T start, INUM_T last, int flags,
    FS_INODE_WALK_FN action, void *ptr)
{
    char *myname = "iso9660_inode_walk";
    ISO_INFO *iso = (ISO_INFO *) fs;
    INUM_T inum;
    FS_INODE *fs_inode;
    int myflags;

    if ((fs_inode =
	    fs_inode_alloc(ISO9660_NDADDR, ISO9660_NIADDR)) == NULL)
	return 1;

    if (verbose)
	fprintf(stderr, "iso9660_inode_walk: iso: %lu"
	    " start: %" PRIuINUM " last: %" PRIuINUM " flags: %d"
	    " action: %lu ptr: %lu\n",
	    (ULONG) fs, start, last, flags, (ULONG) action, (ULONG) ptr);

    myflags = FS_FLAG_META_LINK | FS_FLAG_META_USED | FS_FLAG_META_ALLOC;

    /*
     * Sanity checks.
     */
    if (start < fs->first_inum || start > fs->last_inum) {
	tsk_errno = TSK_ERR_FS_WALK_RNG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "%s: Start inode:  %" PRIuINUM "", myname, start);
	tsk_errstr2[0] = '\0';
	return 1;
    }
    if (last < fs->first_inum || last > fs->last_inum || last < start) {
	tsk_errno = TSK_ERR_FS_WALK_RNG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "%s: End inode: %" PRIuINUM "", myname, last);
	tsk_errstr2[0] = '\0';
	return 1;
    }
    /*
     * Iterate.
     */
    for (inum = start; inum <= last; inum++) {
	int retval;
	if (iso9660_dinode_load(iso, inum)) {
	    fs_inode_free(fs_inode);
	    return 1;
	}

	if ((flags & myflags) != myflags)
	    continue;

	iso9660_dinode_copy(iso, fs_inode);

	retval = action(fs, fs_inode, myflags, ptr);
	if (retval == WALK_ERROR) {
	    fs_inode_free(fs_inode);
	    return 1;
	}
	else if (retval == WALK_STOP) {
	    break;
	}
    }

    /*
     * Cleanup.
     */
    fs_inode_free(fs_inode);
    return 0;
}

/* return 1 if block is allocated in a file's extent, return 0 otherwise */
static int
iso9660_is_block_alloc(FS_INFO * fs, DADDR_T blk_num)
{
    ISO_INFO *iso = (ISO_INFO *) fs;
    in_node *in = iso->in;
    DADDR_T first_block = 0;
    DADDR_T last_block = 0;
    DADDR_T file_size = 0;

    if (verbose)
	fprintf(stderr, "iso9660_is_block_alloc: fs: %lu"
	    " blk_num: %" PRIuDADDR "\n", (ULONG) fs, blk_num);

    while (in) {
	first_block = in->offset / fs->block_size;
	file_size = parseu32(fs, in->inode.dr.data_len);
	last_block = first_block + (file_size / fs->block_size);
	if (file_size % fs->block_size)
	    last_block++;

	if ((blk_num >= first_block) && (blk_num <= last_block))
	    return 1;

	in = in->next;
    }

    return 0;
}

/* flags: FS_FLAG_DATA_ALLOC and FS_FLAG_UNALLOC
 * ISO9660 has a LOT of very sparse meta, so in this function a block is only
 * checked to see if it is part of an inode's extent
 */
uint8_t
iso9660_block_walk(FS_INFO * fs, DADDR_T start, DADDR_T last, int flags,
    FS_BLOCK_WALK_FN action, void *ptr)
{
    char *myname = "iso9660_block_walk";
    DATA_BUF *data_buf;
    DADDR_T addr;
    int myflags = 0;
    SSIZE_T cnt;

    if (verbose)
	fprintf(stderr, "iso9660_block_walk: fs: %lu"
	    " start: %" PRIuDADDR " last: %" PRIuDADDR " flags: %d"
	    " action: %lu ptr: %lu\n",
	    (ULONG) fs, start, last, flags, (ULONG) action, (ULONG) ptr);

    /*
     * Sanity checks.
     */
    if (start < fs->first_block || start > fs->last_block) {
	tsk_errno = TSK_ERR_FS_WALK_RNG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "%s: Start block: %" PRIuDADDR "", myname, start);
	tsk_errstr2[0] = '\0';
	return 1;
    }
    if (last < fs->first_block || last > fs->last_block) {
	tsk_errno = TSK_ERR_FS_WALK_RNG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "%s: End block: %" PRIuDADDR "", myname, last);
	tsk_errstr2[0] = '\0';
	return 1;
    }

    if ((data_buf = data_buf_alloc(fs->block_size)) == NULL) {
	return 1;
    }

    if (verbose)
	fprintf(stderr,
	    "isofs_block_walk: Block Walking %lu to %lu\n",
	    (ULONG) start, (ULONG) last);

    /* cycle through block addresses */
    for (addr = start; addr <= last; addr++) {
	myflags = (iso9660_is_block_alloc(fs, addr)) ?
	    FS_FLAG_DATA_ALLOC : FS_FLAG_DATA_UNALLOC;

	if ((flags & myflags) == myflags) {
	    int retval;
	    cnt = fs_read_block(fs, data_buf, fs->block_size, addr);
	    if (cnt != fs->block_size) {
		if (cnt != -1) {
		    tsk_errno = TSK_ERR_FS_READ;
		    tsk_errstr[0] = '\0';
		}
		snprintf(tsk_errstr2, TSK_ERRSTR_L, "iso_block_walk");
		return 1;
	    }

	    retval = action(fs, addr, data_buf->data, myflags, ptr);
	    if (retval == WALK_ERROR) {
		data_buf_free(data_buf);
		return 1;
	    }
	    else if (retval == WALK_STOP) {
		break;
	    }
	}
    }

    data_buf_free(data_buf);
    return 0;
}

/**************************************************************************
 *
 * FILE WALKING
 *
 *************************************************************************/

uint8_t
iso9660_file_walk(FS_INFO * fs, FS_INODE * inode, uint32_t type,
    uint16_t id, int flags, FS_FILE_WALK_FN action, void *ptr)
{
    char *data_buf;
    size_t length, size;
    int myflags;
    OFF_T offs;
    int bytes_read;
    DADDR_T addr;

    if (verbose)
	fprintf(stderr,
	    "iso9660_file_walk: inode: %" PRIuINUM " type: %" PRIu32
	    " id: %" PRIu16 " flags: %X\n", inode->addr, type, id, flags);

    myflags = FS_FLAG_DATA_CONT;

    data_buf = mymalloc(fs->block_size);
    if (data_buf == NULL) {
	return 1;
    };

    /* examine data at end of last file sector */
    if (flags & FS_FLAG_FILE_SLACK)
	length = roundup(inode->size, 2048);
    else
	length = inode->size;

    /* Get start of extent */
    addr = inode->direct_addr[0];
    offs = fs->block_size * addr;

    while (length > 0) {
	int retval;

	if (length >= fs->block_size)
	    size = fs->block_size;
	else
	    size = length;

	if ((flags & FS_FLAG_FILE_AONLY) == 0) {
	    bytes_read = fs_read_random(fs, data_buf, size, offs);
	    if (bytes_read != size) {
		if (bytes_read != -1) {
		    tsk_errno = TSK_ERR_FS_READ;
		    tsk_errstr[0] = '\0';
		}
		snprintf(tsk_errstr2, TSK_ERRSTR_L,
		    "iso9660_file_walk: Error reading block: %"
		    PRIuDADDR, offs);
		return 1;
	    }
	}
	else {
	    bytes_read = size;
	}
	offs += bytes_read;

	retval = action(fs, addr, data_buf, size, myflags, ptr);
	if (retval == WALK_ERROR) {
	    free(data_buf);
	    return 1;
	}
	else if (retval == WALK_STOP) {
	    break;
	}
	addr++;
	length -= bytes_read;
    }
    free(data_buf);
    return 0;
}


static uint8_t
iso9660_fscheck(FS_INFO * fs, FILE * hFile)
{
    tsk_errno = TSK_ERR_FS_FUNC;
    snprintf(tsk_errstr, TSK_ERRSTR_L,
	"fscheck not implemented for iso9660 yet");
    tsk_errstr2[0] = '\0';
    return 1;
}

/* fsstat - 	print info on file system as seen by each unique primary
 * 		and supplementary volume descriptor.
 */
static uint8_t
iso9660_fsstat(FS_INFO * fs, FILE * hFile)
{
    char str[129];		/* store name of publisher/preparer/etc */
    ISO_INFO *iso = (ISO_INFO *) fs;
    char *cp;
    int i;

    pvd_node *p = iso->pvd;
    svd_node *s = iso->svd;

    if (verbose)
	fprintf(stderr, "iso9660_fsstat: fs: %lu \
			hFile: %lu\n", (ULONG) fs, (ULONG) hFile);

    i = 0;

    while (p != NULL) {
	i++;
	fprintf(hFile, "\nPRIMARY VOLUME DESCRIPTOR %d\n", i);
	fprintf(hFile, "\nFILE SYSTEM INFORMATION\n");
	fprintf(hFile, "--------------------------------------------\n");
	fprintf(hFile, "Read from Primary Volume Descriptor\n");
	fprintf(hFile, "File System Type: ISO9660\n");
	fprintf(hFile, "Volume Name: %s\n", p->pvd.vol_id);
	fprintf(hFile, "Volume Set Size: %d\n",
	    parseu16(fs, p->pvd.vol_set));

	/* print publisher */
	if (p->pvd.pub_id[0] == 0x5f)
	    /* publisher is in a file.  TODO: handle this properly */
	    snprintf(str, 8, "In file\n");
	else
	    snprintf(str, 128, "%s", p->pvd.pub_id);

	cp = &str[127];
	/* find last printable non space character */
	while ((!isprint(*cp) || isspace(*cp)) && (cp != str))
	    cp--;
	*++cp = '\0';
	fprintf(hFile, "Publisher: %s\n", str);
	memset(str, ' ', 128);


	/* print data preparer */
	if (p->pvd.prep_id[0] == 0x5f)
	    /* preparer is in a file.  TODO: handle this properly */
	    snprintf(str, 8, "In file\n");
	else
	    snprintf(str, 128, "%s", p->pvd.prep_id);

	cp = &str[127];
	while ((!isprint(*cp) || isspace(*cp)) && (cp != str))
	    cp--;
	*++cp = '\0';
	fprintf(hFile, "Data Preparer: %s\n", str);
	memset(str, ' ', 128);


	/* print recording application */
	if (p->pvd.app_id[0] == 0x5f)
	    /* application is in a file.  TODO: handle this properly */
	    snprintf(str, 8, "In file\n");
	else
	    snprintf(str, 128, "%s", p->pvd.app_id);
	cp = &str[127];
	while ((!isprint(*cp) || isspace(*cp)) && (cp != str))
	    cp--;
	*++cp = '\0';
	fprintf(hFile, "Recording Application: %s\n", str);
	memset(str, ' ', 128);


	/* print copyright */
	if (p->pvd.copy_id[0] == 0x5f)
	    /* copyright is in a file.  TODO: handle this properly */
	    snprintf(str, 8, "In file\n");
	else
	    snprintf(str, 37, "%s", p->pvd.copy_id);
	cp = &str[36];
	while ((!isprint(*cp) || isspace(*cp)) && (cp != str))
	    cp--;
	*++cp = '\0';
	fprintf(hFile, "Copyright: %s\n", str);
	memset(str, ' ', 37);

	fprintf(hFile, "\nMETADATA INFORMATION\n");
	fprintf(hFile, "--------------------------------------------\n");
	fprintf(hFile, "Path Table is at block: %d\n",
	    (fs->endian & TSK_LIT_ENDIAN) ?
	    getu32(fs, p->pvd.loc_l) : getu32(fs, p->pvd.loc_m));

	fprintf(hFile, "Range: %" PRIuINUM " - %" PRIuINUM "\n",
	    fs->first_inum, fs->last_inum);

	fprintf(hFile, "\nCONTENT INFORMATION\n");
	fprintf(hFile, "--------------------------------------------\n");
	fprintf(hFile, "Sector Size: %d\n", ISO9660_SSIZE_B);
	fprintf(hFile, "Block Size: %d\n", parseu16(fs, p->pvd.blk_sz));

	fprintf(hFile, "Total Sector Range: 0 - %d\n",
	    (int) ((fs->block_size / ISO9660_SSIZE_B) *
		(fs->block_count - 1)));
	/* get image slack, ignore how big the image claims itself to be */
	fprintf(hFile, "Total Block Range: 0 - %d\n",
	    (int) fs->block_count - 1);

	p = p->next;
    }

    i = 0;

    while (s != NULL) {
	i++;
	fprintf(hFile, "\nSUPPLEMENTARY VOLUME DESCRIPTOR %d\n", i);
	fprintf(hFile, "\nFILE SYSTEM INFORMATION\n");
	fprintf(hFile, "--------------------------------------------\n");
	fprintf(hFile, "Read from Supplementary Volume Descriptor\n");
	fprintf(hFile, "File System Type: ISO9660\n");
	fprintf(hFile, "Volume Name: %s\n", s->svd.vol_id);
	fprintf(hFile, "Volume Set Size: %d\n",
	    parseu16(fs, s->svd.vol_set));


	/* print publisher */
	if (s->svd.pub_id[0] == 0x5f)
	    /* publisher is in a file.  TODO: handle this properly */
	    snprintf(str, 8, "In file\n");
	else
	    snprintf(str, 128, "%s", s->svd.pub_id);

	cp = &str[127];
	/* find last printable non space character */
	while ((!isprint(*cp) || isspace(*cp)) && (cp != str))
	    cp--;
	*++cp = '\0';
	fprintf(hFile, "Publisher: %s\n", str);
	memset(str, ' ', 128);


	/* print data preparer */
	if (s->svd.prep_id[0] == 0x5f)
	    /* preparer is in a file.  TODO: handle this properly */
	    snprintf(str, 8, "In file\n");
	else
	    snprintf(str, 128, "%s", s->svd.prep_id);

	cp = &str[127];
	while ((!isprint(*cp) || isspace(*cp)) && (cp != str))
	    cp--;
	*++cp = '\0';
	fprintf(hFile, "Data Preparer: %s\n", str);
	memset(str, ' ', 128);


	/* print recording application */
	if (s->svd.app_id[0] == 0x5f)
	    /* application is in a file.  TODO: handle this properly */
	    snprintf(str, 8, "In file\n");
	else
	    snprintf(str, 128, "%s", s->svd.app_id);
	cp = &str[127];
	while ((!isprint(*cp) || isspace(*cp)) && (cp != str))
	    cp--;
	*++cp = '\0';
	fprintf(hFile, "Recording Application: %s\n", str);
	memset(str, ' ', 128);


	/* print copyright */
	if (s->svd.copy_id[0] == 0x5f)
	    /* copyright is in a file.  TODO: handle this properly */
	    snprintf(str, 8, "In file\n");
	else
	    snprintf(str, 37, "%s\n", s->svd.copy_id);
	cp = &str[36];
	while ((!isprint(*cp) || isspace(*cp)) && (cp != str))
	    cp--;
	*++cp = '\0';
	fprintf(hFile, "Copyright: %s\n", str);
	memset(str, ' ', 37);

	fprintf(hFile, "\nMETADATA INFORMATION\n");
	fprintf(hFile, "--------------------------------------------\n");
	fprintf(hFile, "Path Table is at block: %d\n",
	    (fs->endian & TSK_LIT_ENDIAN) ?
	    getu32(fs, s->svd.loc_l) : getu32(fs, s->svd.loc_m));

	/* learn joliet level (1-3) */
	if (!strncmp((char *) s->svd.esc_seq, "%/E", 3))
	    fprintf(hFile, "Joliet Name Encoding: UCS-2 Level 3\n");
	if (!strncmp((char *) s->svd.esc_seq, "%/C", 3))
	    fprintf(hFile, "Joliet Name Encoding: UCS-2 Level 2\n");
	if (!strncmp((char *) s->svd.esc_seq, "%/@", 3))
	    fprintf(hFile, "Joliet Name Encoding: UCS-2 Level 1\n");
	if (iso->rr_found)
	    fprintf(hFile, "RockRidge Extensions present\n");


	fprintf(hFile, "\nCONTENT INFORMATION\n");
	fprintf(hFile, "--------------------------------------------\n");
	fprintf(hFile, "Sector Size: %d\n", ISO9660_SSIZE_B);
	fprintf(hFile, "Block Size: %d\n", fs->block_size);

	fprintf(hFile, "Total Sector Range: 0 - %d\n",
	    (int) ((fs->block_size / ISO9660_SSIZE_B) *
		(fs->block_count - 1)));
	/* get image slack, ignore how big the image claims itself to be */
	fprintf(hFile, "Total Block Range: 0 - %d\n",
	    (int) fs->block_count - 1);

	s = s->next;
    }

    return 0;
}

char *
make_unix_perm(FS_INFO * fs, iso9660_dentry * dd)
{
    static char perm[11];
    ISO_INFO *iso = (ISO_INFO *) fs;

    if (verbose)
	fprintf(stderr, "make_unix_perm: fs: %lu"
	    " dd: %lu\n", (ULONG) fs, (ULONG) dd);

    perm[10] = '\0';

    memset(perm, '-', 11);

    if (dd->flags & ISO9660_FLAG_DIR)
	perm[0] = 'd';

    if (iso->dinode->ea) {
	if (getu16(fs, iso->dinode->ea->mode) & ISO9660_BIT_UR)
	    perm[1] = 'r';

	if (getu16(fs, iso->dinode->ea->mode) & ISO9660_BIT_UX)
	    perm[3] = 'x';

	if (getu16(fs, iso->dinode->ea->mode) & ISO9660_BIT_GR)
	    perm[4] = 'r';

	if (getu16(fs, iso->dinode->ea->mode) & ISO9660_BIT_GX)
	    perm[6] = 'x';

	if (getu16(fs, iso->dinode->ea->mode) & ISO9660_BIT_AR)
	    perm[7] = 'r';

	if (getu16(fs, iso->dinode->ea->mode) & ISO9660_BIT_AX)
	    perm[9] = 'x';
    }
    else {
	strcpy(&perm[1], "r-xr-xr-x");
    }

    return perm;
}

static void
iso9660_print_rockridge(FILE * hFile, rockridge_ext * rr)
{
    char mode_buf[11];

    fprintf(hFile, "\nROCKRIDGE EXTENSIONS\n");

    fprintf(hFile, "Owner-ID: ");
    fprintf(hFile, "%d\t", (int) rr->uid);

    fprintf(hFile, "Group-ID: ");
    fprintf(hFile, "%d\n", (int) rr->gid);

    fprintf(hFile, "Mode: ");
    memset(mode_buf, '-', 11);
    mode_buf[10] = '\0';

    /* file type */
    /* note: socket and symbolic link are multi bit fields */
    if ((rr->mode & MODE_IFSOCK) == MODE_IFSOCK)
	mode_buf[0] = 's';
    else if ((rr->mode & MODE_IFLNK) == MODE_IFLNK)
	mode_buf[0] = 'l';
    else if (rr->mode & MODE_IFDIR)
	mode_buf[0] = 'd';
    else if (rr->mode & MODE_IFIFO)
	mode_buf[0] = 'p';
    else if (rr->mode & MODE_IFBLK)
	mode_buf[0] = 'b';
    else if (rr->mode & MODE_IFCHR)
	mode_buf[0] = 'c';

    /* owner permissions */
    if (rr->mode & MODE_IRUSR)
	mode_buf[1] = 'r';
    if (rr->mode & MODE_IWUSR)
	mode_buf[2] = 'w';

    if ((rr->mode & MODE_IXUSR) && (rr->mode & MODE_ISUID))
	mode_buf[3] = 's';
    else if (rr->mode & MODE_IXUSR)
	mode_buf[3] = 'x';
    else if (rr->mode & MODE_ISUID)
	mode_buf[3] = 'S';

    /* group permissions */
    if (rr->mode & MODE_IRGRP)
	mode_buf[4] = 'r';
    if (rr->mode & MODE_IWGRP)
	mode_buf[5] = 'w';

    if ((rr->mode & MODE_IXGRP) && (rr->mode & MODE_ISGID))
	mode_buf[6] = 's';
    else if (rr->mode & MODE_IXGRP)
	mode_buf[6] = 'x';
    else if (rr->mode & MODE_ISGID)
	mode_buf[6] = 'S';

    /* other permissions */
    if (rr->mode & MODE_IROTH)
	mode_buf[7] = 'r';
    if (rr->mode & MODE_IWOTH)
	mode_buf[8] = 'w';

    if ((rr->mode & MODE_IXOTH) && (rr->mode & MODE_ISVTX))
	mode_buf[9] = 't';
    else if (rr->mode & MODE_IXOTH)
	mode_buf[9] = 'x';
    else if (rr->mode & MODE_ISVTX)
	mode_buf[9] = 'T';

    fprintf(hFile, "%s\n", mode_buf);
    fprintf(hFile, "Number links: %d\n", rr->nlink);

    fprintf(hFile, "Alternate name: %s\n", rr->fn);
    fprintf(hFile, "\n");
}

static uint8_t
iso9660_istat(FS_INFO * fs, FILE * hFile, INUM_T inum, int numblock,
    int32_t sec_skew)
{
    ISO_INFO *iso = (ISO_INFO *) fs;
    FS_INODE *fs_inode;
    iso9660_dentry dd;

    fs_inode = iso9660_inode_lookup(fs, inum);

    fprintf(hFile, "Entry: %lu\n", (ULONG) inum);

    iso9660_dinode_load(iso, inum);
    memcpy(&dd, &iso->dinode->dr, sizeof(iso9660_dentry));

    fprintf(hFile, "Type: ");
    if (dd.flags & ISO9660_FLAG_DIR)
	fprintf(hFile, "Directory\n");
    else
	fprintf(hFile, "File\n");

    fprintf(hFile, "Links: %lu\n", (ULONG) fs_inode->nlink);

    fprintf(hFile, "Flags: ");

    if (dd.flags & ISO9660_FLAG_HIDE)
	fprintf(hFile, "Hidden, ");

    if (dd.flags & ISO9660_FLAG_ASSOC)
	fprintf(hFile, "Associated, ");

    if (dd.flags & ISO9660_FLAG_RECORD)
	fprintf(hFile, "Record Format, ");

    if (dd.flags & ISO9660_FLAG_PROT)
	fprintf(hFile, "Protected,  ");

    /* check if reserved bits are set, be suspicious */
    if (dd.flags & ISO9660_FLAG_RES1)
	fprintf(hFile, "Reserved1, ");

    if (dd.flags & ISO9660_FLAG_RES2)
	fprintf(hFile, "Reserved2, ");

    if (dd.flags & ISO9660_FLAG_MULT)
	fprintf(hFile, "Non-final multi-extent entry");
    putchar('\n');

    fprintf(hFile, "Name: %s\n", iso->dinode->fn);
    fprintf(hFile, "Size: %d\n", parseu32(fs, iso->dinode->dr.data_len));

    if (iso->dinode->ea) {
	fprintf(hFile, "\nEXTENDED ATTRIBUTE INFO\n");
	fprintf(hFile, "Owner-ID: %d\n", getu32(fs, iso->dinode->ea->uid));
	fprintf(hFile, "Group-ID: %d\n", getu32(fs, iso->dinode->ea->gid));
	fprintf(hFile, "Mode: %s\n", make_unix_perm(fs, &dd));
    }
    else if (iso->dinode->rr) {
	iso9660_print_rockridge(hFile, iso->dinode->rr);
    }
    else {
	fprintf(hFile, "Owner-ID: 0\n");
	fprintf(hFile, "Group-ID: 0\n");
	fprintf(hFile, "Mode: %s\n", make_unix_perm(fs, &dd));
    }

    if (sec_skew != 0) {
	fprintf(hFile, "\nAdjusted File Times:\n");
	fs_inode->mtime -= sec_skew;
	fs_inode->atime -= sec_skew;
	fs_inode->ctime -= sec_skew;

	fprintf(hFile, "Written:\t%s", ctime(&fs_inode->mtime));
	fprintf(hFile, "Accessed:\t%s", ctime(&fs_inode->atime));
	fprintf(hFile, "Created:\t%s", ctime(&fs_inode->ctime));

	fs_inode->mtime += sec_skew;
	fs_inode->atime += sec_skew;
	fs_inode->ctime += sec_skew;

	fprintf(hFile, "\nOriginal File Times:\n");
    }
    else {
	fprintf(hFile, "\nFile Times:\n");
    }

    fprintf(hFile, "Created:\t%s", ctime(&fs_inode->ctime));
    fprintf(hFile, "File Modified:\t%s", ctime(&fs_inode->mtime));
    fprintf(hFile, "Accessed:\t%s", ctime(&fs_inode->atime));


    fprintf(hFile, "\nSectors:\n");
    /* since blocks are all contiguous, print them here to simplify file_walk */
    {
	int block = parseu32(fs, iso->dinode->dr.ext_loc);
	int size = fs_inode->size;
	int rowcount = 0;

	while (size > 0) {
	    fprintf(hFile, "%d ", block++);
	    size -= fs->block_size;
	    rowcount++;
	    if (rowcount == 8) {
		rowcount = 0;
		fprintf(hFile, "\n");
	    }
	}
	fprintf(hFile, "\n");
    }
    return 0;
}




uint8_t
iso9660_jopen(FS_INFO * fs, INUM_T inum)
{
    tsk_errno = TSK_ERR_FS_FUNC;
    snprintf(tsk_errstr, TSK_ERRSTR_L, "ISO9660 does not have a journal");
    tsk_errstr2[0] = '\0';
    return 1;
}

uint8_t
iso9660_jentry_walk(FS_INFO * fs, int flags, FS_JENTRY_WALK_FN action,
    void *ptr)
{
    tsk_errno = TSK_ERR_FS_FUNC;
    snprintf(tsk_errstr, TSK_ERRSTR_L, "ISO9660 does not have a journal");
    tsk_errstr2[0] = '\0';
    return 1;
}

uint8_t
iso9660_jblk_walk(FS_INFO * fs, DADDR_T start, DADDR_T end, int flags,
    FS_JBLK_WALK_FN action, void *ptr)
{
    tsk_errno = TSK_ERR_FS_FUNC;
    snprintf(tsk_errstr, TSK_ERRSTR_L, "ISO9660 does not have a journal");
    tsk_errstr2[0] = '\0';
    return 1;
}

static void
iso9660_close(FS_INFO * fs)
{
    ISO_INFO *iso = (ISO_INFO *) fs;
    pvd_node *p;
    svd_node *s;

    while (iso->pvd != NULL) {
	p = iso->pvd;
	iso->pvd = iso->pvd->next;
	free(p);
    }

    while (iso->svd != NULL) {
	s = iso->svd;
	iso->svd = iso->svd->next;
	free(s);
    }

    free((char *) iso->dinode);

    free(fs);
}

/* get_vol_desc - 	get volume descriptors from image.
 * This is useful for discs which may have 2 volumes on them (no, not
 * multisession CD-R/CD-RW).
 * Design note: If path table address is the same, then you have the same image.
 * Only store unique image info.
 * Uses a linked list even though Ecma-119 says there is only 1 primary vol
 * desc, consider possibility of more.
 *
 * Returns -1 on error, or the number of volumes
 */
int
get_vol_desc(FS_INFO * fs)
{
    int count = 0;
    iso_vd vd;
    ISO_INFO *iso = (ISO_INFO *) fs;
    pvd_node *p, *ptmp;
    svd_node *s, *stmp;
    iso_bootrec *b;
    off_t offs;
    char *myname = "get_vol_desc";
    SSIZE_T cnt;

    b = (iso_bootrec *) mymalloc(sizeof(iso_bootrec));
    if (b == NULL) {
	return -1;
    }

    iso->pvd = NULL;
    iso->svd = NULL;

    offs = ISO9660_SBOFF;

    cnt = fs_read_random(fs, (char *) &vd, sizeof(iso_vd), ISO9660_SBOFF);
    if (cnt != sizeof(iso_vd)) {
	if (cnt != -1) {
	    tsk_errno = TSK_ERR_FS_READ;
	    tsk_errstr[0] = '\0';
	}
	snprintf(tsk_errstr2, TSK_ERRSTR_L,
	    "iso_get_vol_desc: Error reading");
	free(b);
	return -1;
    }

    if (strncmp(vd.magic, ISO9660_MAGIC, 5)) {
	if (verbose)
	    fprintf(stderr, "%s Bad volume descriptor: \
			         Magic number is not CD001", myname);
	free(b);
	return -1;
    }

    while (vd.type != ISO9660_VOL_DESC_SET_TERM) {

	offs += sizeof(iso_vd);

	switch (vd.type) {

	case ISO9660_PRIM_VOL_DESC:

	    p = (pvd_node *) mymalloc(sizeof(pvd_node));
	    if (p == NULL) {
		free(b);
		return -1;
	    }

	    cnt =
		fs_read_random(fs, (char *) &(p->pvd), sizeof(iso_pvd),
		offs);
	    if (cnt != sizeof(iso_pvd)) {
		if (cnt != -1) {
		    tsk_errno = TSK_ERR_FS_READ;
		    tsk_errstr[0] = '\0';
		}
		snprintf(tsk_errstr2, TSK_ERRSTR_L,
		    "iso_get_vol_desc: Error reading");
		free(b);
		return -1;
	    }

	    offs += sizeof(iso_pvd);

	    /* list not empty */
	    if (iso->pvd) {
		ptmp = iso->pvd;
		/* append to list if path table address not found in list */
		while ((p->pvd.loc_l != ptmp->pvd.loc_l) && (ptmp->next))
		    ptmp = ptmp->next;

		if (p->pvd.loc_l == ptmp->pvd.loc_l) {
		    free(p);
		    p = NULL;
		}
		else {
		    ptmp->next = p;
		    p->next = NULL;
		    count++;
		}
	    }

	    /* list empty, insert */
	    else {
		iso->pvd = p;
		p->next = NULL;
		count++;
	    }
	    break;

	case ISO9660_SUPP_VOL_DESC:

	    s = (svd_node *) mymalloc(sizeof(svd_node));
	    if (s == NULL) {
		free(b);
		return -1;
	    }

	    cnt =
		fs_read_random(fs, (char *) &(s->svd), sizeof(iso_svd),
		offs);
	    if (cnt != sizeof(iso_svd)) {
		if (cnt != -1) {
		    tsk_errno = TSK_ERR_FS_READ;
		    tsk_errstr[0] = '\0';
		}
		snprintf(tsk_errstr2, TSK_ERRSTR_L,
		    "iso_get_vol_desc: Error reading");
		free(b);
		return -1;
	    }

	    offs += sizeof(iso_svd);

	    /* list not empty */
	    if (iso->svd) {
		stmp = iso->svd;
		/* append to list if path table address not found in list */
		while ((s->svd.loc_l != stmp->svd.loc_l) && (stmp->next))
		    stmp = stmp->next;

		if (s->svd.loc_l == stmp->svd.loc_l) {
		    free(s);
		    s = NULL;
		}
		else {
		    stmp->next = s;
		    s->next = NULL;
		    count++;
		}
	    }

	    /* list empty, insert */
	    else {
		iso->svd = s;
		s->next = NULL;
		count++;
	    }
	    break;

	    /* boot records are just read and discarded for now... */
	case ISO9660_BOOT_RECORD:

	    cnt =
		fs_read_random(fs, (char *) b, sizeof(iso_bootrec), offs);
	    if (cnt != sizeof(iso_bootrec)) {
		if (cnt != -1) {
		    tsk_errno = TSK_ERR_FS_READ;
		    tsk_errstr[0] = '\0';
		}
		snprintf(tsk_errstr2, TSK_ERRSTR_L,
		    "iso_get_vol_desc: Error reading");
		free(b);
		return -1;
	    }

	    offs += sizeof(iso_bootrec);
	    break;
	}

	cnt = fs_read_random(fs, (char *) &vd, sizeof(iso_vd), offs);
	if (cnt != sizeof(iso_vd)) {
	    if (cnt != -1) {
		tsk_errno = TSK_ERR_FS_READ;
		tsk_errstr[0] = '\0';
	    }
	    snprintf(tsk_errstr2, TSK_ERRSTR_L,
		"iso_get_vol_desc: Error reading");
	    free(b);
	    return -1;
	}

	if (strncmp(vd.magic, ISO9660_MAGIC, 5)) {
	    if (verbose)
		fprintf(stderr, "%s Bad volume descriptor: \
				         Magic number is not CD001", myname);
	    free(b);
	    return -1;
	}
    }

    free(b);

    /* now that we have all primary and supplementary volume descs, we should cull the list of */
    /* primary that match up with supplems, since supplem has all info primary has plus more. */
    /* this will make jobs such as searching all volumes easier later */
    stmp = iso->svd;

    while (stmp) {
	ptmp = iso->pvd;
	while (ptmp) {
	    if (ptmp->pvd.loc_l == stmp->svd.loc_l) {
		/* Start of primary list? */
		if (ptmp == iso->pvd) {
		    iso->pvd = ptmp->next;
		}
		else {
		    p = iso->pvd;
		    while (p->next != ptmp)
			p = p->next;
		    p->next = ptmp->next;
		}
		ptmp->next = NULL;
		free(ptmp);
		ptmp = NULL;
		count--;
	    }
	    ptmp = ptmp->next;
	}

	stmp = stmp->next;
    }

    if ((iso->pvd == NULL) && (iso->svd == NULL)) {
	tsk_errno = TSK_ERR_FS_MAGIC;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "get_vol_desc: primary and secondary volume descriptors null");
	tsk_errstr2[0] = '\0';
	return -1;
    }

    return count;
}

/* find_block_sizes - 	compare block sizes in other volumes to the one we
 * chose in iso9660_open.  Report discrepancies if verbose is on
 */
int
find_block_sizes(FS_INFO * fs)
{
    ISO_INFO *iso = (ISO_INFO *) fs;
    pvd_node *p;
    svd_node *s;
    int size = 0;

    p = iso->pvd;

    while (p) {
	if (!size) {
	    size = parseu16(fs, p->pvd.blk_sz);
	}
	else if (size != parseu16(fs, p->pvd.blk_sz)) {
	    if (verbose)
		fprintf(stderr,
		    "Warning: Expected ISO9660 block size: %d got %d\n",
		    size, parseu16(fs, p->pvd.blk_sz));
	}
	p = p->next;
    }

    s = iso->svd;

    while (s) {
	if (!size) {
	    size = parseu16(fs, s->svd.blk_sz);
	}
	else if (size != parseu16(fs, s->svd.blk_sz)) {
	    if (verbose)
		fprintf(stderr,
		    "Warning: Expected ISO9660 block size: %d got %d\n",
		    size, parseu16(fs, s->svd.blk_sz));
	}
	s = s->next;
    }

    return size;
}

/* iso9660_open -
 * opens an iso9660 filesystem.
 * Design note: This function doesn't read a superblock, since iso9660 doesnt
 * really have one.  Volume info is read in with a call to get_vol_descs().
 */
FS_INFO *
iso9660_open(IMG_INFO * img_info, SSIZE_T offset, unsigned char ftype,
    uint8_t test)
{
    ISO_INFO *iso;
    FS_INFO *fs;

    int len;

    if (verbose) {
	fprintf(stderr, "iso9660_open img_info: %lu"
	    " ftype: %" PRIu8 " test: %" PRIu8 "\n", (ULONG) img_info,
	    ftype, test);
    }

    iso = (ISO_INFO *) mymalloc(sizeof(ISO_INFO));
    if (iso == NULL) {
	return NULL;
    };
    fs = &(iso->fs_info);

    iso->rr_found = 0;

    if ((ftype & FSMASK) != ISO9660_TYPE) {
        free(iso);
	tsk_errno = TSK_ERR_FS_ARG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "Invalid FS type in iso9660_open");
	return NULL;
    }

    fs->ftype = ftype;
    fs->flags = 0;
    fs->img_info = img_info;
    fs->offset = offset;

    iso->in = NULL;

    /* following two lines use setup TSK memory manger for local byte ordering
     * since we never check magic as a number, because it is not a number
     * and ISO9660 has no concept of byte order.
     */
    len = 1;
    fs_guessu32(fs, (uint8_t *) & len, 1);

    /* get_vol_descs checks magic value */
    if (get_vol_desc(fs) == -1) {
	free(iso);
	if (test)
	    return NULL;
	else {
	    tsk_errno = TSK_ERR_FS_MAGIC;
	    snprintf(tsk_errstr, TSK_ERRSTR_L,
		"Invalid FS type in iso9660_open");
	    return NULL;
	}
    }

    fs->dev_bsize = 512;
    fs->block_size = find_block_sizes(fs);
    if (fs->block_size == 0) {
        free(iso);
	tsk_errno = TSK_ERR_FS_MAGIC;
	snprintf(tsk_errstr, TSK_ERRSTR_L, "Block size not found");
	return NULL;
    }

    fs->inum_count = iso9660_find_inodes(iso);
    if ((int) fs->inum_count == -1) {
        free(iso);
	return NULL;
    }

    fs->last_inum = fs->inum_count - 1;
    fs->first_inum = ISO9660_FIRSTINO;
    fs->root_inum = ISO9660_ROOTINO;

    if (iso->pvd)
	fs->block_count = parseu32(fs, iso->pvd->pvd.vol_spc);
    else
	fs->block_count = parseu32(fs, iso->svd->svd.vol_spc);

    fs->first_block = 0;
    fs->last_block = fs->block_count - 1;

    fs->inode_walk = iso9660_inode_walk;
    fs->block_walk = iso9660_block_walk;
    fs->file_walk = iso9660_file_walk;
    fs->inode_lookup = iso9660_inode_lookup;
    fs->dent_walk = iso9660_dent_walk;
    fs->fsstat = iso9660_fsstat;
    fs->fscheck = iso9660_fscheck;
    fs->istat = iso9660_istat;
    fs->close = iso9660_close;

    fs->jblk_walk = iso9660_jblk_walk;
    fs->jentry_walk = iso9660_jentry_walk;
    fs->jopen = iso9660_jopen;


    /* allocate buffers */

    /* dinode */
    iso->dinode = (iso9660_inode *) mymalloc(sizeof(iso9660_inode));
    if (iso->dinode == NULL) {
        free(iso);
	return NULL;
    }
    iso->dinum = -1;

    return fs;
}
