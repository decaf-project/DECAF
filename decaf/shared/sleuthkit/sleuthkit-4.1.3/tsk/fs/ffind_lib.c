/*
** ffind  (file find)
** The Sleuth Kit 
**
** Find the file that uses the specified inode (including deleted files)
** 
** Brian Carrier [carrier <at> sleuthkit [dot] org]
** Copyright (c) 2006-2011 Brian Carrier, Basis Technology.  All Rights reserved
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

/**
 * \file ffind_lib.c
 * Contains the library API functions used by the TSK ffind command
 * line tool.
 */
#include "tsk_fs_i.h"
#include "tsk_ntfs.h"           // NTFS has an optimized version of this function



/** \internal 
* Structure to store data for callbacks.
*/
typedef struct {
    TSK_INUM_T inode;
    uint8_t flags;
    uint8_t found;
} FFIND_DATA;


static TSK_WALK_RET_ENUM
find_file_act(TSK_FS_FILE * fs_file, const char *a_path, void *ptr)
{
    FFIND_DATA *data = (FFIND_DATA *) ptr;

    /* We found it! */
    if (fs_file->name->meta_addr == data->inode) {
        data->found = 1;
        if (fs_file->name->flags & TSK_FS_NAME_FLAG_UNALLOC)
            tsk_printf("* ");

        tsk_printf("/%s%s\n", a_path, fs_file->name->name);

        if (!(data->flags & TSK_FS_FFIND_ALL)) {
            return TSK_WALK_STOP;
        }
    }
    return TSK_WALK_CONT;
}


/* Return 0 on success and 1 on error */
uint8_t
tsk_fs_ffind(TSK_FS_INFO * fs, TSK_FS_FFIND_FLAG_ENUM lclflags,
    TSK_INUM_T a_inode,
    TSK_FS_ATTR_TYPE_ENUM type, uint8_t type_used,
    uint16_t id, uint8_t id_used, TSK_FS_DIR_WALK_FLAG_ENUM flags)
{
    FFIND_DATA data;

    data.found = 0;
    data.flags = lclflags;
    data.inode = a_inode;

    /* Since we start the walk on the root inode, then this will not show
     ** up in the above functions, so do it now
     */
    if (data.inode == fs->root_inum) {
        if (flags & TSK_FS_DIR_WALK_FLAG_ALLOC) {
            tsk_printf("/\n");
            data.found = 1;

            if (!(lclflags & TSK_FS_FFIND_ALL))
                return 0;
        }
    }

    if (TSK_FS_TYPE_ISNTFS(fs->ftype)) {
        if (ntfs_find_file(fs, data.inode, type, type_used, id, id_used,
                flags, find_file_act, &data))
            return 1;
    }
    else {
        if (tsk_fs_dir_walk(fs, fs->root_inum, flags, find_file_act,
                &data))
            return 1;
    }

    if (data.found == 0) {

        /* With FAT, we can at least give the name of the file and call
         * it orphan 
         */
        if (TSK_FS_TYPE_ISFAT(fs->ftype)) {
            TSK_FS_FILE *fs_file =
                tsk_fs_file_open_meta(fs, NULL, data.inode);
            if ((fs_file != NULL) && (fs_file->meta != NULL)
                && (fs_file->meta->name2 != NULL)) {
                if (fs_file->meta->flags & TSK_FS_META_FLAG_UNALLOC)
                    tsk_printf("* ");
                tsk_printf("%s/%s\n", TSK_FS_ORPHAN_STR,
                    fs_file->meta->name2->name);
            }
            if (fs_file)
                tsk_fs_file_close(fs_file);
        }
        else {
            tsk_printf("File name not found for inode\n");
        }
    }
    return 0;
}

/* Method to read file information from the provided inode index number 
 * Used in DECAF 
 */
TSK_FS_FILE*
qemu_inode_read(TSK_FS_INFO *fs, unsigned int inode_number)
{

	
	TSK_FS_FILE * file_info_internal=tsk_fs_file_alloc(fs);

	
	if((fs)->file_add_meta((fs), file_info_internal,(TSK_INUM_T) inode_number))
	{	
		//file_info_internal = tsk_fs_file_open_meta( fs,file_info_internal,file_info_internal->meta->addr);
	}
	else
	{
		//monitor_printf(default_mon,"file open error! \n");
	}


	return file_info_internal;
#if 0
	//This is always qemu image
    TSK_IMG_TYPE_ENUM imgtype = QEMU_IMG;
    TSK_IMG_INFO *img;
	
    TSK_FS_TYPE_ENUM fstype = TSK_FS_TYPE_DETECT;
    TSK_FS_INFO *fs;
    int dir_walk_flags = TSK_FS_DIR_WALK_FLAG_RECURSE;

    TSK_FS_ATTR_TYPE_ENUM type;
    uint16_t id;
    uint8_t id_used = 0, type_used = 0;
    uint8_t ffind_flags = 0;
    
    //avb_arg, from input
    TSK_INUM_T inode;
    unsigned int ssize = 0;



    /* if the user did not specify either of the alloc/unalloc dir_walk_flags
     ** then show them all
     */
    if ((!(dir_walk_flags & TSK_FS_DIR_WALK_FLAG_ALLOC))
        && (!(dir_walk_flags & TSK_FS_DIR_WALK_FLAG_UNALLOC)))
        dir_walk_flags |=
            (TSK_FS_DIR_WALK_FLAG_ALLOC | TSK_FS_DIR_WALK_FLAG_UNALLOC);


    /* Get the inode from char */
 	// TODO: AVB, this will be changed to accept unsinged longs, but its ok for now
    if (tsk_fs_parse_inum(inode_input, &inode, &type, &type_used, &id,
            &id_used)) {
        TFPRINTF(stderr, _TSK_T("Invalid inode: %s\n"),inode_input);
      
    }

    /* open image */
    if ((img =
            tsk_img_open(1, &img_path,
                imgtype, ssize)) == NULL) {
        tsk_error_print(stderr);
 		return NULL;
    }

    /* AVB, we will not perform this check for performance */
    #if 0
    if ((imgaddr * img->sector_size) >= img->size) {
        tsk_fprintf(stderr,
            "Sector offset supplied is larger than disk image (maximum: %"
            PRIu64 ")\n", img->size / img->sector_size);
        exit(1);
    }
    #endif


    if ((fs = tsk_fs_open_img(img, 1*512, TSK_FS_TYPE_EXT_DETECT)) == NULL) {
        tsk_error_print(stderr);
        if (tsk_error_get_errno() == TSK_ERR_FS_UNSUPTYPE)
            tsk_fs_type_print(stderr);
        img->close(img);
        return 1;
    }



    if (inode < fs->first_inum) {
        tsk_fprintf(stderr,
            "Inode is too small for image (%" PRIuINUM ")\n",
            fs->first_inum);
        return NULL;
    }
    if (inode > fs->last_inum) {
        tsk_fprintf(stderr,
            "Inode is too large for image (%" PRIuINUM ")\n",
            fs->last_inum);
        return NULL;
    }

    if (tsk_fs_ffind(fs, (TSK_FS_FFIND_FLAG_ENUM) ffind_flags, inode, type,
            type_used, id, id_used,
            (TSK_FS_DIR_WALK_FLAG_ENUM) dir_walk_flags)) {
        tsk_error_print(stderr);
        fs->close(fs);
        img->close(img);
        return NULL;
    }

    //fs->close(fs);
    //img->close(img);

	return fs;

	#endif
}

