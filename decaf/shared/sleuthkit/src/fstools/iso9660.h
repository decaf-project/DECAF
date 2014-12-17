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

/* refernece documents used:
 * IEEE P1281 - System Use Sharing Protocol, version 1.12
 * IEEE P1282 - Rock Ridge Interchange Protocol, version 1.12
 * ECMA-119 - Volume and File Structure of CDROM for Information Interchange,
 * 2nd Edition
 */

#ifndef _iso9660_h
#define _iso9660_h

/* This part borrowed from the bsd386 isofs */
#define ISODCL(from, to) (to - from + 1)

/*
 * Constants
 */
#define ISO9660_FIRSTINO	0
#define ISO9660_ROOTINO		0
#define ISO9660_NIADDR		0	/* iso9660 doesnt have indirect blocks */
#define ISO9660_NDADDR		1	/* just a single data "block" */
#define ISO9660_SBOFF		32768
#define ISO9660_SSIZE_B		2048
#define ISO9660_MIN_BLOCK_SIZE	512
#define ISO9660_MAX_BLOCK_SIZE	2048
#define ISO9660_MAGIC		"CD001"

/* values used in volume descriptor type */
#define ISO9660_BOOT_RECORD		0	/* boot record */
#define ISO9660_PRIM_VOL_DESC		1	/* primary volume descriptor */
#define ISO9660_SUPP_VOL_DESC		2	/* supplementary volume descriptor */
#define ISO9660_VOL_PART_DESC		3	/* volume partition descriptor */
#define ISO9660_RESERVE_FLOOR		4	/* 4-254 are reserved */
#define ISO9660_RESERVE_CEIL		254
#define ISO9660_VOL_DESC_SET_TERM	255	/* volume descriptor set terminator */

#define ISO9660_MAXNAMLEN	103	/* joliet max name length */

/* Bits in permissions used in extended attribute records.  */
#define ISO9660_BIT_UR	0x0010
#define ISO9660_BIT_UX	0x0040
#define ISO9660_BIT_GR	0x0100
#define ISO9660_BIT_GX	0x0400
#define ISO9660_BIT_AR	0x1000
#define ISO9660_BIT_AX	0x4000

/* directory descriptor flags */
#define ISO9660_FLAG_HIDE	0x01	/* Hide file -- called EXISTENCE */
#define ISO9660_FLAG_DIR	0x02	/* Directory */
#define ISO9660_FLAG_ASSOC	0x04	/* File is associated */
#define ISO9660_FLAG_RECORD	0X08	/* Record format in extended attr */
#define ISO9660_FLAG_PROT	0X10	/* No read / exec perm in ext attr */
#define ISO9660_FLAG_RES1	0X20	/* reserved */
#define ISO9660_FLAG_RES2	0x40	/* reserved */
#define ISO9660_FLAG_MULT	0X80	/* not final entry of mult ext file */

/* POSIX modes used in ISO9660 not already defined */
#define MODE_IFSOCK 0140000	/* socket */
#define MODE_IFLNK  0120000	/* symbolic link */
#define MODE_IFDIR  0040000	/* directory */
#define MODE_IFIFO  0010000	/* pipe or fifo */
#define MODE_IFBLK  0060000	/* block special */
#define MODE_IFCHR  0020000	/* character special */

/* used to determine if get directory entry function needs to handle Joliet */
#define ISO9660_TYPE_PVD	0
#define ISO9660_TYPE_SVD	1

/* macros to get numbers.  numbers in ISO9660 are stored in both byte orders.
 * These will grab the one we are using.
 */
#define parseu16(foo, x) \
    (uint16_t)( (foo->endian & TSK_LIT_ENDIAN)  ? \
    ((uint16_t) \
        ((uint16_t)((uint8_t *)(x))[0] << 0) + \
        ((uint16_t)((uint8_t *)(x))[1] << 8)) \
        : \
    ((uint16_t) \
        ((uint16_t)((uint8_t *)(x))[2] << 8) + \
        ((uint16_t)((uint8_t *)(x))[3] << 0)) )

#define parseu32(foo, x) \
    (uint32_t)( (foo->endian & TSK_LIT_ENDIAN)  ? \
    ((uint32_t) \
        ((uint32_t)((uint8_t *)(x))[0] << 0) + \
        ((uint32_t)((uint8_t *)(x))[1] << 8) + \
        ((uint32_t)((uint8_t *)(x))[2] << 16) + \
        ((uint32_t)((uint8_t *)(x))[3] << 24)) \
        : \
    ((uint32_t) \
        ((uint32_t)((uint8_t *)(x))[7] << 0) + \
        ((uint32_t)((uint8_t *)(x))[6] << 8) + \
        ((uint32_t)((uint8_t *)(x))[5] << 16) + \
        ((uint32_t)((uint8_t *)(x))[4] << 24)) )


/* recording date and time */
typedef struct {
    uint8_t year;		/* years since 1900 */
    uint8_t month;		/* 1-12 */
    uint8_t day;		/* 1-31 */
    uint8_t hour;		/* 0-23 */
    uint8_t min;		/* 0-59 */
    uint8_t sec;		/* 0-59 */
    uint8_t gmt_off;		/* greenwich mean time offset */
} record_data;

/* data and time format
 * all are stored as "digits" according to specifications for iso9660
 */
typedef struct {
    uint8_t year[4];		/* 1 to 9999 */
    uint8_t month[2];		/* 1 to 12 */
    uint8_t day[2];		/* 1 to 31 */
    uint8_t hour[2];		/* 0 to 23 */
    uint8_t min[2];		/* 0 to 59 */
    uint8_t sec[2];		/* 0 to 59 */
    uint8_t hun[2];		/* hundredths of a second */
    uint8_t gmt_off;		/* GMT offset */
} date_time;

/* iso 9660 directory record */
typedef struct {
    uint8_t length;		/* length of directory record */
    uint8_t ext_len;		/* extended attribute record length */
    uint8_t ext_loc[8];		/* location of extent (2|32) */
    uint8_t data_len[8];	/* data length (2|32) */
    record_data rec;		/* recording date and time */
    int8_t flags;		/* file flags */
    uint8_t unit_sz;		/* file unit size */
    uint8_t gap_sz;		/* interleave gap size */
    uint8_t seq[4];		/* volume sequence number (2|16) */
    uint8_t len;		/* length of file identifier */
} iso9660_dentry;

/* This is a dummy struct used to make reading an entire PVD easier,
 * due to the fact that the root directory has a 1 byte name that
 * wouldn't be worth adding to the regular struct.
 */
typedef struct {
    uint8_t length;		/* length of directory record */
    uint8_t ext_len;		/* extended attribute record length */
    uint8_t ext_loc[8];		/* location of extent (2|32) */
    uint8_t data_len[8];	/* data length (2|32) */
    record_data rec;		/* recording date and time */
    int8_t flags;		/* file flags */
    uint8_t unit_sz;		/* file unit size */
    uint8_t gap_sz;		/* interleave gap size */
    uint8_t seq[4];		/* volume sequence number (2|16) */
    uint8_t len;		/* length of file identifier */
    char name;
} iso9660_root_dentry;

/* generic volume descriptor */
typedef struct {
    uint8_t type;		/* volume descriptor type */
    char magic[ISODCL(2, 6)];	/* magic number. CD001 */
    char ver[ISODCL(7, 7)];	/* volume descriptor version */
} iso_vd;

/* primary volume descriptor */
typedef struct {
    char unused1[ISODCL(8, 8)];	/* should be 0.  unused. */
    char sys_id[ISODCL(9, 40)];	/* system identifier */
    char vol_id[ISODCL(41, 72)];	/* volume identifier */
    char unused2[ISODCL(73, 80)];	/* should be 0.  unused. */
    uint8_t vol_spc[ISODCL(81, 88)];	/* volume space size (2|32) */
    char unused3[ISODCL(89, 120)];	/* should be 0.  unused. */
    uint8_t vol_set[ISODCL(121, 124)];	/* volume set size (2|16) */
    uint8_t vol_seq[ISODCL(125, 128)];	/* volume sequence number (2|16) */
    uint8_t blk_sz[ISODCL(129, 132)];	/* logical block size (2|16) */
    uint8_t path_size[ISODCL(133, 140)];	/* path table size (2|32) */
    uint8_t loc_l[ISODCL(141, 144)];	/* locat of occur of type L path tbl. */
    uint8_t opt_loc_l[ISODCL(145, 148)];	/* locat of optional occurence */
    uint8_t loc_m[ISODCL(149, 152)];	/* locat of occur of type M path tbl. */
    uint8_t opt_loc_m[ISODCL(153, 156)];	/* locat of optional occurence */
    iso9660_root_dentry dir_rec;	/* directory record for root dir */
    char vol_setid[ISODCL(191, 318)];	/* volume set identifier */
    unsigned char pub_id[ISODCL(319, 446)];	/* publisher identifier */
    unsigned char prep_id[ISODCL(447, 574)];	/* data preparer identifier */
    unsigned char app_id[ISODCL(575, 702)];	/* application identifier */
    unsigned char copy_id[ISODCL(703, 739)];	/* copyright file identifier */
    unsigned char abs_id[ISODCL(740, 776)];	/* abstract file identifier */
    unsigned char bib_id[ISODCL(777, 813)];	/* bibliographic file identifier */
    date_time make_date;	/* volume creation date/time */
    date_time mod_date;		/* volume modification date/time */
    date_time exp_date;		/* volume expiration date/time */
    date_time ef_date;		/* volume effective date/time */
    uint8_t fs_ver;		/* file structure version */
    char res[ISODCL(883, 883)];	/* reserved */
    char app_use[ISODCL(884, 1395)];	/* application use */
    char reserv[ISODCL(1396, 2048)];	/* reserved */
} iso_pvd;

/* supplementary volume descriptor */
typedef struct {
    uint8_t flags[ISODCL(8, 8)];	/* volume flags */
    char sys_id[ISODCL(9, 40)];	/* system identifier */
    char vol_id[ISODCL(41, 72)];	/* volume identifier */
    char unused2[ISODCL(73, 80)];	/* should be 0.  unused. */
    uint8_t vol_spc[ISODCL(81, 88)];	/* volume space size (2|32) */
    uint8_t esc_seq[ISODCL(89, 120)];	/* escape sequences */
    uint8_t vol_set[ISODCL(121, 124)];	/* volume set size (2|16) */
    uint8_t vol_seq[ISODCL(125, 128)];	/* volume sequence number (2|16) */
    uint8_t blk_sz[ISODCL(129, 132)];	/* logical block size (2|16) */
    uint8_t path_size[ISODCL(133, 140)];	/* path table size (2|32) */
    uint8_t loc_l[ISODCL(141, 144)];	/* locat of occur of type L path tbl. */
    uint8_t opt_loc_l[ISODCL(145, 148)];	/* locat of optional occurence */
    uint8_t loc_m[ISODCL(149, 152)];	/* locat of occur of type M path tbl. */
    uint8_t opt_loc_m[ISODCL(153, 156)];	/* locat of optional occurence */
    iso9660_root_dentry dir_rec;	/* directory record for root dir */
    char vol_setid[ISODCL(191, 318)];	/* volume set identifier */
    unsigned char pub_id[ISODCL(319, 446)];	/* publisher identifier */
    unsigned char prep_id[ISODCL(447, 574)];	/* data preparer identifier */
    unsigned char app_id[ISODCL(575, 702)];	/* application identifier */
    unsigned char copy_id[ISODCL(703, 739)];	/* copyright file identifier */
    unsigned char abs_id[ISODCL(740, 776)];	/* abstract file identifier */
    unsigned char bib_id[ISODCL(777, 813)];	/* bibliographic file identifier */
    date_time make_date;	/* volume creation date/time */
    date_time mod_date;		/* volume modification date/time */
    date_time exp_date;		/* volume expiration date/time */
    date_time ef_date;		/* volume effective date/time */
    char fs_ver[ISODCL(882, 882)];	/* file structure version */
    char res[ISODCL(883, 883)];	/* reserved */
    char app_use[ISODCL(884, 1395)];	/* application use */
    char reserv[ISODCL(1396, 2048)];	/* reserved */
} iso_svd;

/* iso 9660 boot record */
typedef struct {
    char boot_sys_id[ISODCL(8, 39)];	/* boot system identifier */
    char boot_id[ISODCL(40, 71)];	/* boot identifier */
    char system_use[ISODCL(72, 2048)];	/* system use */
} iso_bootrec;

/* path table record */
typedef struct {
    uint8_t len_di;		/* length of directory identifier */
    uint8_t attr_len;		/* extended attribute record length */
    uint8_t ext_loc[4];		/* location of extent */
    uint8_t par_dir[2];		/* parent directory number */
} path_table_rec;

/* extended attribute record */
typedef struct {
    uint8_t uid[ISODCL(1, 4)];	/* owner identification */
    uint8_t gid[ISODCL(5, 8)];	/* group identification */
    uint8_t mode[ISODCL(9, 10)];	/* permissions */
    uint8_t cre[ISODCL(11, 27)];	/* file creation date/time */
    uint8_t mod[ISODCL(28, 44)];	/* file modification d/t */
    uint8_t exp[ISODCL(45, 61)];	/* file expiration d/t */
    uint8_t eff[ISODCL(62, 78)];	/* file effective d/t */
    uint8_t fmt[ISODCL(79, 79)];	/* record format */
    uint8_t attr[ISODCL(80, 80)];	/* record attributes */
    uint8_t len[ISODCL(81, 84)];	/* record length */
    uint8_t sys_id[ISODCL(85, 116)];	/* system identifier */
    uint8_t uns[ISODCL(117, 180)];	/* system use, not specified */
    uint8_t e_ver[ISODCL(181, 181)];	/* extended attribute record version */
    uint8_t len_esc[ISODCL(182, 182)];	/* length of escape sequences */
} ext_attr_rec;

/* primary volume descriptor linked list node */
typedef struct pvd_node {
    iso_pvd pvd;
    struct pvd_node *next;
} pvd_node;

/* supplementary volume descriptor linked list node */
typedef struct svd_node {
    iso_svd svd;
    struct svd_node *next;
} svd_node;

/* RockRidge extension info */
typedef struct {
    uid_t uid /* owner */ ;
    gid_t gid;			/* group */
    uint16_t mode;		/* posix file mode */
    uint32_t nlink;		/* number of links */
    char fn[ISO9660_MAXNAMLEN];	/* alternate filename */
} rockridge_ext;

/*
 * Inode
 */
typedef struct {
    iso9660_dentry dr;		/* directory record */
    ext_attr_rec *ea;		/* extended attribute record */
    char fn[ISO9660_MAXNAMLEN];	/* file name */
    rockridge_ext *rr;		/* RockRidge Extensions */
    int version;
} iso9660_inode;

/* inode linked list node */
typedef struct in_node {
    iso9660_inode inode;
    OFF_T offset;		/* byte offset of inode into disk */
    INUM_T inum;		/* identifier of inode */
    int size;			/* kludge: used to flag fifos, etc */
    struct in_node *next;
} in_node;

/* The all important ISO_INFO struct */
typedef struct {
    FS_INFO fs_info;		/* SUPER CLASS */
    INUM_T dinum;		/* cached inode number */
    iso9660_inode *dinode;	/* cached disk inode */
    uint32_t path_tab_addr;	/* address of path table */
    uint32_t root_addr;		/* address of root dir extent */
    pvd_node *pvd;		/* primary volume descriptors */
    svd_node *svd;		/* secondary volume descriptors */
    in_node *in;		/* list of inodes */
    uint8_t rr_found;		/* 1 if rockridge found */
} ISO_INFO;

extern uint8_t iso9660_dent_walk(FS_INFO * fs, INUM_T inode, int flags,
    FS_DENT_WALK_FN action, void *ptr);

extern uint8_t iso9660_dinode_load(ISO_INFO * iso, INUM_T inum);

/**********************************************************
 *
 * RockRidge Extensions
 *
 **********************************************************/

/* System use sharing protocol CE entry */
typedef struct {
    char sig[ISODCL(1, 2)];	/* signature, should be "CE" */
    uint8_t len[ISODCL(3, 3)];	/* length of system use entry */
    uint8_t ver[ISODCL(4, 4)];	/* system use entry version */
    uint8_t blk_loc[ISODCL(5, 12)];	/* block location */
    uint8_t offs[ISODCL(13, 20)];	/* offset */
    uint8_t len_c[ISODCL(21, 28)];	/* length of cont area */

} ce_sys_use;

/* System use  sharing protocol SP entry */
typedef struct {
    char sig[ISODCL(1, 2)];	/* signature, should be "SP" */
    uint8_t len[ISODCL(3, 3)];	/* length of system use entry */
    uint8_t ver[ISODCL(4, 4)];	/* system use entry version */
    uint8_t check[ISODCL(5, 6)];	/* check bytes 0xbeef */
    uint8_t skip[ISODCL(7, 7)];	/* bytes skipped */
} sp_sys_use;

/* Rockridge ISO9660 system use field entry */
typedef struct {
    char sig[ISODCL(1, 2)];	/* signature, should be "RR" */
    uint8_t len[ISODCL(3, 3)];	/* length of system use entry */
    uint8_t ver[ISODCL(4, 4)];	/* system use entry version */
    uint8_t foo[ISODCL(5, 5)];	/* foo */
} rr_sys_use;

/* Rockridge PX entry */
typedef struct {
    char sig[ISODCL(1, 2)];	/* signature, should be "PX" */
    uint8_t len[ISODCL(3, 3)];	/* length, should be 44 */
    uint8_t ver[ISODCL(4, 4)];	/* system use entry version (1) */
    uint8_t mode[ISODCL(5, 12)];	/* POSIX file mode */
    uint8_t links[ISODCL(13, 20)];	/* POSIX file links */
    uint8_t uid[ISODCL(21, 28)];	/* POSIX user id */
    uint8_t gid[ISODCL(29, 36)];	/* POSIX group id */
    /* rockridge docs say this is here, k3b disagrees... hmmmm */
    //      uint8_t serial[ISODCL(37,44)];  /* POSIX file serial number */
} rr_px_entry;

/* Rockridge PN entry */
typedef struct {
    char sig[ISODCL(1, 2)];	/* signature, should be "PN" */
    uint8_t len[ISODCL(3, 3)];	/* length, should be 20 */
    uint8_t ver[ISODCL(4, 4)];	/* system use entry version (1) */
    uint8_t devt_h[ISODCL(5, 12)];	/* top 32 bits of device # */
    uint8_t devt_l[ISODCL(13, 20)];	/* low 32 bits of device # */
} rr_pn_entry;

/* Rockridge SL entry */
typedef struct {
    char sig[ISODCL(1, 2)];	/* signature, should be "SL" */
    uint8_t len[ISODCL(3, 3)];	/* length */
    uint8_t ver[ISODCL(4, 4)];	/* system use entry version (1) */
    uint8_t flags[ISODCL(5, 5)];	/* flags */
} rr_sl_entry;

/* Rockridge NM entry */
typedef struct {
    char sig[ISODCL(1, 2)];	/* signature, should be "NM" */
    uint8_t len[ISODCL(3, 3)];	/* length of alternate name */
    uint8_t ver[ISODCL(4, 4)];	/* system use entry version (1) */
    uint8_t flags[ISODCL(5, 5)];	/* flags */
} rr_nm_entry;

/* Rockridge CL entry */
typedef struct {
    char sig[ISODCL(1, 2)];	/* signature, should be "CL" */
    uint8_t len[ISODCL(3, 3)];	/* length, should be 12 */
    uint8_t ver[ISODCL(4, 4)];	/* system use entry version (1) */
    uint8_t par_loc[ISODCL(5, 12)];	/* location of parent directory */
} rr_cl_entry;

/* Rockridge RE entry */
typedef struct {
    char sig[ISODCL(1, 2)];	/* signature, should be "RE" */
    uint8_t len[ISODCL(3, 3)];	/* length, should be 4 */
    uint8_t ver[ISODCL(4, 4)];	/* system use entry version (1) */
} rr_re_entry;

/* Rockridge TF entry */
typedef struct {
    char sig[ISODCL(1, 2)];	/* signature, should be "TF" */
    uint8_t len[ISODCL(3, 3)];	/* length of TF entry */
    uint8_t ver[ISODCL(4, 4)];	/* system use entry version (1) */
    uint8_t flags[ISODCL(5, 5)];	/* flags */
} rr_tf_entry;

/* Rockridge SF entry */
typedef struct {
    char sig[ISODCL(1, 2)];	/* signature, should be "SF" */
    uint8_t len[ISODCL(3, 3)];	/* length, should be 21 */
    uint8_t ver[ISODCL(4, 4)];	/* system use entry version (1) */
    uint8_t vfs_h[ISODCL(5, 12)];	/* virtual file size high */
    uint8_t vfs_l[ISODCL(13, 20)];	/* virtual file size low */
    uint8_t depth[ISODCL(21, 21)];	/* table depth */
} rr_sf_entry;

#endif
