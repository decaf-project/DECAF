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

#ifndef _HFS_H
#define _HFS_H

/*
 * All structures created using technote 1150 from Apple.com
 * http://developer.apple.com/technotes/tn/tn1150.html
 */

/*
 * Constants
 */

#define HFS_MAGIC	0x4244	/* HX in big endian */
#define HFSPLUS_MAGIC	0x482b	/* H+ in big endian */

#define HFS_SBOFF	1024
#define HFS_NDADDR	0001
#define HFS_NIADDR	0001

#define HFS_FIRST_USER_CNID	16

/* b-tree kind types */
#define HFS_BTREE_LEAF_NODE	-1
#define HFS_BTREE_INDEX_NODE	 0
#define HFS_BTREE_HEADER_NODE	 1
#define HFS_BTREE_MAP_NODE	 2

#define HFS_MAXNAMLEN		255

#define HFS_ROOT_INUM		2

/* catalog file data types */
#define HFS_FOLDER_RECORD	0x0001
#define HFS_FILE_RECORD		0X0002
#define HFS_FOLDER_THREAD	0x0003
#define HFS_FILE_THREAD		0x0004

/*
 * HFS uses its own time system, which is seconds since Jan 1 1904
 * instead of the typical Jan 1 1970.  This number is the seconds between
 * 1 Jan 1904 and 1 Jan 1970 which will make ctime(3) work instead of
 * re-writing the Apple library function to convert this time.
 */
#define NSEC_BTWN_1904_1970	(uint32_t) 2082844800U

#define HFS_BIT_VOLUME_UNMOUNTED	(uint32_t)(1 << 8)
#define HFS_BIT_VOLUME_INCONSISTENT	(uint32_t)(1 << 11)
#define HFS_BIT_VOLUME_JOURNALED	(uint32_t)(1 << 13)

/*
 * HFS structures
 */

/* File and Folder name struct */
typedef struct {
    uint16_t length[2];
    uint16_t unicode[255];
} hfs_uni_str;

/* access permissions */
typedef struct {
    uint8_t owner[4];		/* file owner */
    uint8_t group[4];		/* file group */
    uint8_t a_flags;		/* admin flags */
    uint8_t o_flags;		/* owner flags */
    uint8_t mode[2];		/* file mode */
    union {
	uint8_t inum[4];	/* inode number */
	uint8_t nlink[4];	/* link count */
	uint8_t raw[4];		/* raw device */
    } special;
} hfs_access_perm;

typedef struct {
    uint32_t uid;		/* owner id */
    uint32_t gid;		/* group id */
    uint32_t mode;		/* permissions */
    uint32_t dev;		/* special device */
} hfs_file_perm;

/* HFS extent descriptor */
typedef struct {
    uint8_t start_blk[4];	/* start block */
    uint8_t blk_cnt[4];		/* block count */
} hfs_ext_desc;

/* fork data structure */
typedef struct {
    uint8_t logic_sz[8];	/* logical size */
    uint8_t clmp_sz[4];		/* clump size */
    uint8_t total_blk[4];	/* total blocks */
    hfs_ext_desc extents[8];
} hfs_fork;

/*
** Super Block
*/
typedef struct {
    uint8_t signature[2];	/* "H+" for HFS+, "HX" for HFSX */
    uint8_t version[2];		/* 4 for HFS+, 5 for HFSX */
    uint8_t attr[4];		/* volume attributes */
    uint8_t last_mnt_ver[4];	/* last mounted version */
    uint8_t jinfo_blk[4];	/* journal info block */
    uint8_t c_date[4];		/* volume creation date */
    uint8_t m_date[4];		/* volume last modified date */
    uint8_t bkup_date[4];	/* volume last backup date */
    uint8_t chk_date[4];	/* date of last consistency check */
    uint8_t file_cnt[4];	/* number of files on volume */
    uint8_t fldr_cnt[4];	/* number of folders on volume */
    uint8_t blk_sz[4];		/* allocation block size */
    uint8_t blk_cnt[4];		/* number of blocks on disk */
    uint8_t free_blks[4];	/* unused block count */
    uint8_t next_alloc[4];	/* start of next allocation search */
    uint8_t rsrc_clmp_sz[4];	/* default clump size for resource forks */
    uint8_t data_clmp_sz[4];	/* default clump size for data forks */
    uint8_t next_cat_id[4];	/* next catalog id */
    uint8_t write_cnt[4];	/* write count */
    uint8_t enc_bmp[8];		/* encoding bitmap */
    uint8_t finder_info[32];
    hfs_fork alloc_file;	/* location and size of allocation file */
    hfs_fork ext_file;		/* location and size of extents file */
    hfs_fork cat_file;		/* location and size of catalog file */
    hfs_fork attr_file;		/* location and size of attributes file */
    hfs_fork start_file;	/* location and size of startup file */
} hfs_sb;

typedef struct {
    uint8_t key_len[2];
    uint8_t parent_cnid[4];
    uint8_t name[510];
} hfs_cat_key;

typedef struct {
    uint32_t inum;		/* inode number */
    uint32_t parent;		/* parent directoy number */
    uint32_t node;		/* btree leaf node */
    DADDR_T offs;		/* offset of beginning of inode */
} hfs_inode_struct;

typedef struct {
    uint8_t flink[4];		/* next node number */
    uint8_t blink[4];		/* previous node number */
    int8_t kind;		/* type of node */
    uint8_t height;		/* level in B-tree */
    uint8_t num_rec[2];		/* number of records this node */
    uint8_t res[2];		/* reserved */
} hfs_btree_node;

typedef struct {
    uint8_t depth[2];		/* current depth of btree */
    uint8_t root[4];		/* node number of root node */
    uint8_t leaf[4];		/* number of records in leaf nodes */
    uint8_t firstleaf[4];	/* number of first leaf node */
    uint8_t lastleaf[4];	/* number of last leaf node */
    uint8_t size[2];		/* byte size of leaf node (512..32768) */
    uint8_t max_len[2];		/* max key length in an index or leaf node */
    uint8_t total[4];		/* number of nodes in btree (free or in use) */
    uint8_t free[4];		/* unused nodes in btree */
    uint8_t res[2];		/* reserved */
    uint8_t clmp_sz[4];		/* clump size */
    uint8_t bt_type;		/* btree type */
    uint8_t k_type;		/* key compare type */
    uint8_t attr[4];		/* attributes */
    uint8_t res2[64];		/* reserved */
} hfs_btree_header_record;

typedef struct {
    int8_t v[2];
    int8_t h[2];
} hfs_point;

typedef struct {
    uint8_t file_type[4];	/* file type */
    uint8_t file_cr[4];		/* file creator */
    uint8_t flags[2];		/* finder flags */
    hfs_point loc;		/* location in the folder */
    uint8_t res[2];		/* reserved */
} hfs_fileinfo;

typedef struct {
    uint8_t res1[8];		/* reserved 1 */
    uint8_t extflags[2];	/* extended finder flags */
    uint8_t res2[2];		/* reserved 2 */
    uint8_t folderid[4];	/* putaway folder id */
} hfs_extendedfileinfo;

typedef struct {
    uint8_t rec_type[2];	/* record type */
    uint8_t flags[2];		/* flags - reserved */
    uint8_t valence[4];		/* valence - items in this folder */
    uint8_t cnid[4];		/* catalog node id */
    uint8_t ctime[4];		/* create date */
    uint8_t cmtime[4];		/* content mod date */
    uint8_t amtime[4];		/* attribute mod date */
    uint8_t atime[4];		/* access date */
    uint8_t bkup_time[4];	/* backup time */
    hfs_access_perm perm;	/* HFS permissions */
    hfs_fileinfo u_info;	/* user info */
    hfs_extendedfileinfo f_info;	/* finder info */
    uint8_t txt_enc[4];		/* text encoding */
    uint8_t res[4];		/* reserved */
} hfs_folder;

typedef struct {
    uint8_t rec_type[2];	/* record type */
    uint8_t flags[2];
    uint8_t res[4];		/* reserved */
    uint8_t cnid[4];		/* catalog node id */
    uint8_t ctime[4];		/* create date */
    uint8_t cmtime[4];		/* content modification date */
    uint8_t attr_mtime[4];	/* attribute mod date */
    uint8_t atime[4];		/* access date */
    uint8_t bkup_date[4];	/* backup date */
    hfs_access_perm perm;	/* permissions */
    hfs_fileinfo u_info;	/* user info */
    hfs_extendedfileinfo f_info;	/* finder info */
    uint8_t text_enc[4];	/* text encoding */
    uint8_t res2[4];		/* reserved 2 */
    hfs_fork data;		/* data fork */
    hfs_fork resource;		/* resource fork */
} hfs_file;

typedef struct {
    int16_t type;
    int16_t res;
    uint8_t cnid[4];
    char name[255];
} hfs_thread;

typedef struct {
    FS_INFO fs_info;		/* SUPER CLASS */
    hfs_sb *fs;			/* cached superblock */
    FS_INODE *cat_inode;	/* contains the data entry for the cat */

    hfs_inode_struct *inodes;
    uint8_t *block_map;		/* cached block allocation bitmap */
    uint8_t *leaf_map;		/* bitmap of btree leaf nodes */
    uint8_t *del_map;		/* bitmap of btree deleted leaf nodes */
    hfs_file *cat;		/* cache for on-disk inode */
    int flags;			/* flags for on-disk inode */
    INUM_T inum;		/* number of above cached cat */

    hfs_btree_header_record *hdr;	/* stored btree header node */

    OFF_T key;			/* offset of key for cached inode */

} HFS_INFO;

/************** JOURNAL ******************/

/* HFS Journal Info Block */
typedef struct {
    uint8_t flags[4];
    uint8_t dev_sig[32];
    uint8_t offs[8];
    uint8_t size[8];
    uint8_t res[128];
} hfs_journ_sb;

/* 
 * Prototypes
 */
extern void hfs_inode_walk(FS_INFO *, INUM_T, INUM_T, int,
    FS_INODE_WALK_FN, void *);
extern void hfs_file_walk(FS_INFO *, FS_INODE *, uint32_t,
    uint16_t, int, FS_FILE_WALK_FN, void *);
extern void hfs_dinode_lookup(HFS_INFO *, INUM_T);
extern void hfs_dent_walk(FS_INFO *, INUM_T, int, FS_DENT_WALK_FN, void *);
extern void hfs_jopen(FS_INFO *, INUM_T);
extern void hfs_jentry_walk(FS_INFO *, int, FS_JENTRY_WALK_FN, void *);
extern void hfs_jblk_walk(FS_INFO *, DADDR_T, DADDR_T, int,
    FS_JBLK_WALK_FN, void *);
extern int hfs_is_block_alloc(uint32_t, uint8_t *);
extern OFF_T hfs_cat_find_node_offset(HFS_INFO *, int);
extern int hfs_is_bit_b_alloc(uint32_t, uint8_t *);
extern int hfs_btree_find_node(FS_INFO *, uint32_t);
#endif
