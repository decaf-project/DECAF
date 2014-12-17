/*
** fls
** The Sleuth Kit 
**
** $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
**
** Given an image and directory inode, display the file names and 
** directories that exist (both active and deleted)
**
** Brian Carrier [carrier@sleuthkit.org]
** Copyright (c) 2006 Brian Carrier, Basis Technology.  All Rights reserved
** Copyright (c) 2003-2005 Brian Carier.  All rights reserved
**
** TASK
** Copyright (c) 2002 @stake Inc.  All rights reserved
**
** TCTUTILs
** Copyright (c) 2001 Brian Carrier.  All rights reserved
**
**
** This software is distributed under the Common Public License 1.0
**
*/

#include "fs_tools.h"
#include "libfstools.h"
#include "ntfs.h"

/* Time skew of the system in seconds */
static int32_t sec_skew = 0;


/*directory prefix for printing mactime output */
static char *macpre = NULL;

static int localflags;



/* this is a wrapper type function that takes care of the runtime
 * flags
 * 
 * fs_data should be set to NULL for all NTFS file systems
 */
static void
printit(FS_INFO * fs, FS_DENT * fs_dent, int flags, FS_DATA * fs_data)
{
    unsigned int i;

    if (!(localflags & FLS_FULL)) {
	for (i = 0; i < fs_dent->pathdepth; i++)
	    fprintf(stdout, "+");

	if (fs_dent->pathdepth)
	    fprintf(stdout, " ");
    }


    if (localflags & FLS_MAC) {
	if ((sec_skew != 0) && (fs_dent->fsi)) {
	    fs_dent->fsi->mtime -= sec_skew;
	    fs_dent->fsi->atime -= sec_skew;
	    fs_dent->fsi->ctime -= sec_skew;
	}

	fs_dent_print_mac(stdout, fs_dent, flags, fs, fs_data, macpre);

	if ((sec_skew != 0) && (fs_dent->fsi)) {
	    fs_dent->fsi->mtime += sec_skew;
	    fs_dent->fsi->atime += sec_skew;
	    fs_dent->fsi->ctime += sec_skew;
	}
    }

    else if (localflags & FLS_LONG) {
	if ((sec_skew != 0) && (fs_dent->fsi)) {
	    fs_dent->fsi->mtime -= sec_skew;
	    fs_dent->fsi->atime -= sec_skew;
	    fs_dent->fsi->ctime -= sec_skew;
	}

	if (FLS_FULL & localflags)
	    fs_dent_print_long(stdout, fs_dent, flags, fs, fs_data);
	else {
	    char *tmpptr = fs_dent->path;
	    fs_dent->path = NULL;
	    fs_dent_print_long(stdout, fs_dent, flags, fs, fs_data);
	    fs_dent->path = tmpptr;
	}

	if ((sec_skew != 0) && (fs_dent->fsi)) {
	    fs_dent->fsi->mtime += sec_skew;
	    fs_dent->fsi->atime += sec_skew;
	    fs_dent->fsi->ctime += sec_skew;
	}
    }
    else {
	if (FLS_FULL & localflags)
	    fs_dent_print(stdout, fs_dent, flags, fs, fs_data);
	else {
	    char *tmpptr = fs_dent->path;
	    fs_dent->path = NULL;
	    fs_dent_print(stdout, fs_dent, flags, fs, fs_data);
	    fs_dent->path = tmpptr;
	}
	printf("\n");
    }
}


/* 
 * call back action function for dent_walk
 */
static uint8_t
print_dent_act(FS_INFO * fs, FS_DENT * fs_dent, int flags, void *ptr)
{

    /* only print dirs if FLS_DIR is set and only print everything
     ** else if FLS_FILE is set (or we aren't sure what it is)
     */
    if (((localflags & FLS_DIR) &&
	    ((fs_dent->fsi) &&
		((fs_dent->fsi->mode & FS_INODE_FMT) == FS_INODE_DIR))) ||
	((localflags & FLS_FILE) &&
	    (((fs_dent->fsi) &&
		    ((fs_dent->fsi->mode & FS_INODE_FMT) != FS_INODE_DIR))
		|| (!fs_dent->fsi)))) {


	/* Make a special case for NTFS so we can identify all of the
	 * alternate data streams!
	 */
	if (((fs->ftype & FSMASK) == NTFS_TYPE) && (fs_dent->fsi)) {

	    FS_DATA *fs_data = fs_dent->fsi->attr;
	    uint8_t printed = 0;

	    while ((fs_data) && (fs_data->flags & FS_DATA_INUSE)) {

		if (fs_data->type == NTFS_ATYPE_DATA) {
		    mode_t mode = fs_dent->fsi->mode;
		    uint8_t ent_type = fs_dent->ent_type;

		    printed = 1;


		    /* 
		     * A directory can have a Data stream, in which
		     * case it would be printed with modes of a
		     * directory, although it is really a file
		     * So, to avoid confusion we will set the modes
		     * to a file so it is printed that way.  The
		     * entry for the directory itself will still be
		     * printed as a directory
		     */

		    if ((fs_dent->fsi->mode & FS_INODE_FMT) ==
			FS_INODE_DIR) {


			/* we don't want to print the ..:blah stream if
			 * the -a flag was not given
			 */
			if ((fs_dent->name[0] == '.') && (fs_dent->name[1])
			    && (fs_dent->name[2] == '\0') &&
			    ((localflags & FLS_DOT) == 0)) {
			    fs_data = fs_data->next;
			    continue;
			}

			fs_dent->fsi->mode &= ~FS_INODE_FMT;
			fs_dent->fsi->mode |= FS_INODE_REG;
			fs_dent->ent_type = FS_DENT_REG;
		    }

		    printit(fs, fs_dent, flags, fs_data);

		    fs_dent->fsi->mode = mode;
		    fs_dent->ent_type = ent_type;
		}
		else if (fs_data->type == NTFS_ATYPE_IDXROOT) {
		    printed = 1;

		    /* If it is . or .. only print it if the flags say so,
		     * we continue with other streams though in case the 
		     * directory has a data stream 
		     */
		    if (!((ISDOT(fs_dent->name)) &&
			    ((localflags & FLS_DOT) == 0)))
			printit(fs, fs_dent, flags, fs_data);
		}

		fs_data = fs_data->next;
	    }

	    /* A user reported that an allocated file had the standard
	     * attributes, but no $Data.  We should print something */
	    if (printed == 0) {
		printit(fs, fs_dent, flags, NULL);
	    }

	}
	else {
	    /* skip it if it is . or .. and we don't want them */
	    if (!((ISDOT(fs_dent->name)) && ((localflags & FLS_DOT) == 0)))
		printit(fs, fs_dent, flags, NULL);
	}
    }
    return WALK_CONT;
}


/* Returns 0 on success and 1 on error */
uint8_t
fs_fls(FS_INFO * fs, uint8_t lclflags, INUM_T inode, int flags, char *pre,
    int32_t skew)
{
    localflags = lclflags;
    macpre = pre;
    sec_skew = skew;

    return fs->dent_walk(fs, inode, flags, print_dent_act, NULL);
}
