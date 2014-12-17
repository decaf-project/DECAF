/*
** ffind  (file find)
** The Sleuth Kit 
**
** $Date: 2006-08-24 10:45:33 -0400 (Thu, 24 Aug 2006) $
**
** Find the file that uses the specified inode (including deleted files)
** 
** Brian Carrier [carrier@sleuthkit.org]
** Copyright (c) 2006 Brian Carrier, Basis Technology.  All Rights reserved
** Copyright (c) 2003-2005 Brian Carrier.  All rights reserved 
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
#include "libfstools.h"

/* NTFS has an optimized version of this function */
extern uint8_t
ntfs_find_file(FS_INFO *, INUM_T, uint32_t,
    uint16_t, int, FS_DENT_WALK_FN, void *ptr);


static INUM_T inode = 0;
static uint8_t localflags = 0;
static uint8_t found = 0;

char found_path[512];

static uint8_t
find_file_act(FS_INFO * fs, FS_DENT * fs_dent, int flags, void *ptr)
{
    /* We found it! */
    if (fs_dent->inode == inode) {
//		found = 1;
//		if (flags & FS_FLAG_NAME_UNALLOC)
//	    	printf("* ");

//		printf("/%s%s\n", fs_dent->path, fs_dent->name);
//		if (!(localflags & FFIND_ALL)) {
//	    	return WALK_STOP;
//		}
	    sprintf(found_path, "/%s%s", fs_dent->path, fs_dent->name);
		if(strncmp(found_path, "/-ORPHAN_FILE", 13)) {
			found = 1;
			return WALK_STOP;
		}
    }
    return WALK_CONT;
}


/* Return 0 on success and 1 on error */
uint8_t
fs_ffind(FS_INFO * fs, uint8_t lclflags, INUM_T inode_a, uint32_t type,
    uint16_t id, int flags)
{

    found = 0;
    localflags = lclflags;
    inode = inode_a;

    /* Since we start the walk on the root inode, then this will not show
     ** up in the above functions, so do it now
     */
    if (inode == fs->root_inum) {
	if (flags & FS_FLAG_NAME_ALLOC) {
//	    printf("/\n");
            sprintf(found_path, "/");
	    found = 1;

	    if (!(lclflags & FFIND_ALL))
		return 0;
	}
    }

    if ((fs->ftype & FSMASK) == NTFS_TYPE) {
	if (ntfs_find_file(fs, inode, type, id, flags, find_file_act,
		NULL))
	    return 1;
    }
    else {
	if (fs->dent_walk(fs, fs->root_inum, flags, find_file_act, NULL))
	    return 1;
    }

#if 0
    if (found == 0) {

	/* With FAT, we can at least give the name of the file and call
	 * it orphan 
	 */
	if ((fs->ftype & FSMASK) == FATFS_TYPE) {
	    FS_INODE *fs_inode = fs->inode_lookup(fs, inode);
	    if ((fs_inode != NULL) && (fs_inode->name != NULL)) {
		if (fs_inode->flags & FS_FLAG_NAME_UNALLOC)
		    printf("* ");
		printf("%s/%s\n", ORPHAN_STR, fs_inode->name->name);
	    }
	    if (fs_inode)
		fs_inode_free(fs_inode);
	}
	else {
	    printf("inode not currently used\n");
	}
    }
#endif
    return (found == 0);
}
