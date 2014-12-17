/*
** fs_open
** The Sleuth Kit 
**
** $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
**
** Brian Carrier [carrier@sleuthkit.org]
** Copyright (c) 2006 Brian Carrier, Basis Technology.  All Rights reserved
** Copyright (c) 2003-2005 Brian Carrier.  All rights reserved 
**
** TASK
** Copyright (c) 2002 Brian Carrier, @stake Inc.  All rights reserved
** 
** Copyright (c) 1997,1998,1999, International Business Machines          
** Corporation and others. All Rights Reserved.
*/

/* TCT */
/*++
 * LICENSE
 *	This software is distributed under the IBM Public License.
 * AUTHOR(S)
 *	Wietse Venema
 *	IBM T.J. Watson Research
 *	P.O. Box 704
 *	Yorktown Heights, NY 10598, USA
 --*/

#include "fs_tools.h"

/* fs_open - open a file system */

FS_INFO *
fs_open(IMG_INFO * img_info, SSIZE_T offset, const char *type)
{

    /* We will try different file systems ... 
     * We need to try all of them in case more than one matches
     */
    if (type == NULL) {
	FS_INFO *fs_info, *fs_set = NULL;
	char *set = NULL;

	if ((fs_info = ntfs_open(img_info, offset, NTFS, 1)) != NULL) {
	    set = "NTFS";
	    fs_set = fs_info;
	}
	else {
	    tsk_errno = 0;
	    tsk_errstr[0] = '\0';
	    tsk_errstr2[0] = '\0';
	}

	if ((fs_info = fatfs_open(img_info, offset, FATAUTO, 1)) != NULL) {
	    if (set == NULL) {
		set = "FAT";
		fs_set = fs_info;
	    }
	    else {
		fs_set->close(fs_set);
		fs_info->close(fs_info);
		tsk_errno = TSK_ERR_FS_UNKTYPE;
		snprintf(tsk_errstr, TSK_ERRSTR_L, "FAT or %s", set);
		tsk_errstr2[0] = '\0';
		return NULL;
	    }
	}
	else {
	    tsk_errno = 0;
	    tsk_errstr[0] = '\0';
	    tsk_errstr2[0] = '\0';
	}

	if ((fs_info = ext2fs_open(img_info, offset, EXTAUTO, 1)) != NULL) {
	    if (set == NULL) {
		set = "EXT2/3";
		fs_set = fs_info;
	    }
	    else {
		fs_set->close(fs_set);
		fs_info->close(fs_info);
		tsk_errno = TSK_ERR_FS_UNKTYPE;
		snprintf(tsk_errstr, TSK_ERRSTR_L, "EXT2/3 or %s", set);
		tsk_errstr2[0] = '\0';
		return NULL;
	    }
	}
	else {
	    tsk_errno = 0;
	    tsk_errstr[0] = '\0';
	    tsk_errstr2[0] = '\0';
	}

	if ((fs_info = ffs_open(img_info, offset, FFSAUTO)) != NULL) {
	    if (set == NULL) {
		set = "UFS";
		fs_set = fs_info;
	    }
	    else {
		fs_set->close(fs_set);
		fs_info->close(fs_info);
		tsk_errno = TSK_ERR_FS_UNKTYPE;
		snprintf(tsk_errstr, TSK_ERRSTR_L, "UFS or %s", set);
		tsk_errstr2[0] = '\0';
		return NULL;
	    }
	}
	else {
	    tsk_errno = 0;
	    tsk_errstr[0] = '\0';
	    tsk_errstr2[0] = '\0';
	}

	/*
	   if ((fs_info = hfs_open(img_info, offset, HFS, 1)) != NULL) {
	   if (set == NULL) {
	   set = "HFS";
	   fs_set = fs_info;
	   }
	   else {
	   fs_set->close(fs_set);
	   fs_info->close(fs_info);
	   tsk_errno = TSK_ERR_FS_UNKTYPE;
	   snprintf(tsk_errstr, TSK_ERRSTR_L,
	   "HFS or %s", set);
	   tsk_errstr2[0] = '\0';
	   return NULL;
	   }
	   }
	 */
	if ((fs_info = iso9660_open(img_info, offset, ISO9660, 1)) != NULL) {
	    if (set != NULL) {
		fs_set->close(fs_set);
		fs_info->close(fs_info);
		tsk_errno = TSK_ERR_FS_UNKTYPE;
		snprintf(tsk_errstr, TSK_ERRSTR_L, "ISO9660 or %s", set);
		tsk_errstr2[0] = '\0';
		return NULL;
	    }
	    fs_set = fs_info;
	}
	else {
	    tsk_errno = 0;
	    tsk_errstr[0] = '\0';
	    tsk_errstr2[0] = '\0';
	}


	if (fs_set == NULL) {
	    tsk_errno = TSK_ERR_FS_UNKTYPE;
	    tsk_errstr[0] = '\0';
	    tsk_errstr2[0] = '\0';
	    return NULL;
	}
	return fs_set;
    }
    else {
	uint8_t ftype;
	ftype = fs_parse_type(type);

	switch (ftype & FSMASK) {
	case FFS_TYPE:
	    return ffs_open(img_info, offset, ftype);
	case EXTxFS_TYPE:
	    return ext2fs_open(img_info, offset, ftype, 0);
	case FATFS_TYPE:
	    return fatfs_open(img_info, offset, ftype, 0);
	case NTFS_TYPE:
	    return ntfs_open(img_info, offset, ftype, 0);
	case ISO9660_TYPE:
	    return iso9660_open(img_info, offset, ftype, 0);
	    /*
	       case HFS_TYPE:
	       return hfs_open(img_info, offset, ftype, 0);
	     */
	case RAWFS_TYPE:
	    return rawfs_open(img_info, offset);
	case SWAPFS_TYPE:
	    return swapfs_open(img_info, offset);
	case UNSUPP_FS:
	default:
	    tsk_errno = TSK_ERR_FS_UNSUPTYPE;
	    snprintf(tsk_errstr, TSK_ERRSTR_L, "%s", type);
	    tsk_errstr2[0] = '\0';
	    return NULL;
	}
    }
}
