/*
** fs_dent
** The Sleuth Kit 
**
** $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
**
** Display and manipulate directory entries 
** This file contains generic functions that call the appropriate function
** depending on the file system type
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
#include "fs_tools.h"
#include "ntfs.h"

extern char *tzname[2];
char fs_dent_str[FS_DENT_MAX_STR][2] =
    { "-", "p", "c", "", "d", "", "b", "", "r", "",
    "l", "", "s", "h", "w"
};
char fs_inode_str[FS_INODE_MAX_STR][2] =
    { "-", "p", "c", "", "d", "", "b", "", "-", "",
    "l", "", "s", "h", "w"
};

/* Allocate a fs_dent structure */
FS_DENT *
fs_dent_alloc(ULONG norm_namelen, ULONG shrt_namelen)
{
    FS_DENT *fs_dent;
    fs_dent = (FS_DENT *) mymalloc(sizeof(*fs_dent));
    if (fs_dent == NULL)
	return NULL;

    fs_dent->name = (char *) mymalloc(norm_namelen + 1);
    if (fs_dent->name == NULL) {
	free(fs_dent);
	return NULL;
    }
    fs_dent->name_max = norm_namelen;

    fs_dent->shrt_name_max = shrt_namelen;
    if (shrt_namelen == 0) {
	fs_dent->shrt_name = NULL;
    }
    else {
	fs_dent->shrt_name = (char *) mymalloc(shrt_namelen + 1);
	if (fs_dent->shrt_name == NULL) {
	    free(fs_dent->name);
	    free(fs_dent);
	    return NULL;
	}
    }

    fs_dent->ent_type = FS_DENT_UNDEF;
    fs_dent->path = NULL;
    fs_dent->pathdepth = 0;
    fs_dent->fsi = NULL;

    return fs_dent;
}

FS_DENT *
fs_dent_realloc(FS_DENT * fs_dent, ULONG namelen)
{
    if (fs_dent->name_max == namelen)
	return fs_dent;

    fs_dent->name = (char *) myrealloc(fs_dent->name, namelen + 1);
    if (fs_dent->name == NULL) {
	if (fs_dent->fsi)
	    fs_inode_free(fs_dent->fsi);

	if (fs_dent->shrt_name)
	    free(fs_dent->shrt_name);

	free(fs_dent);
	return NULL;
    }

    fs_dent->ent_type = FS_DENT_UNDEF;
    fs_dent->name_max = namelen;

    return fs_dent;
}

void
fs_dent_free(FS_DENT * fs_dent)
{
    if (fs_dent->fsi)
	fs_inode_free(fs_dent->fsi);

    free(fs_dent->name);
    if (fs_dent->shrt_name)
	free(fs_dent->shrt_name);

    free(fs_dent);
}


/***********************************************************************
 * Printing functions
 ***********************************************************************/

/*
 * make the ls -l output from the mode 
 *
 * ls must be 12 bytes or more!
 */
void
make_ls(mode_t mode, char *ls)
{
    int typ;

    /* put the default values in */
    strcpy(ls, "----------");

    typ = (mode & FS_INODE_FMT) >> FS_INODE_SHIFT;
    if ((typ & FS_INODE_MASK) < FS_INODE_MAX_STR)
	ls[0] = fs_inode_str[typ & FS_INODE_MASK][0];


    /* user perms */
    if (mode & MODE_IRUSR)
	ls[1] = 'r';
    if (mode & MODE_IWUSR)
	ls[2] = 'w';
    /* set uid */
    if (mode & MODE_ISUID) {
	if (mode & MODE_IXUSR)
	    ls[3] = 's';
	else
	    ls[3] = 'S';
    }
    else if (mode & MODE_IXUSR)
	ls[3] = 'x';

    /* group perms */
    if (mode & MODE_IRGRP)
	ls[4] = 'r';
    if (mode & MODE_IWGRP)
	ls[5] = 'w';
    /* set gid */
    if (mode & MODE_ISGID) {
	if (mode & MODE_IXGRP)
	    ls[6] = 's';
	else
	    ls[6] = 'S';
    }
    else if (mode & MODE_IXGRP)
	ls[6] = 'x';

    /* other perms */
    if (mode & MODE_IROTH)
	ls[7] = 'r';
    if (mode & MODE_IWOTH)
	ls[8] = 'w';

    /* sticky bit */
    if (mode & MODE_ISVTX) {
	if (mode & MODE_IXOTH)
	    ls[9] = 't';
	else
	    ls[9] = 'T';
    }
    else if (mode & MODE_IXOTH)
	ls[9] = 'x';
}

void
fs_print_time(FILE * hFile, time_t time)
{
    if (time <= 0) {
	fprintf(hFile, "0000.00.00 00:00:00 (UTC)");
    }
    else {
	struct tm *tmTime = localtime(&time);

	fprintf(hFile, "%.4d.%.2d.%.2d %.2d:%.2d:%.2d (%s)",
	    (int) tmTime->tm_year + 1900,
	    (int) tmTime->tm_mon + 1, (int) tmTime->tm_mday,
	    tmTime->tm_hour,
	    (int) tmTime->tm_min, (int) tmTime->tm_sec,
	    tzname[(tmTime->tm_isdst == 0) ? 0 : 1]);
    }
}


/* The only difference with this one is that the time is always
 * 00:00:00, which is applicable for the A-Time in FAT, which does
 * not have a time and if we do it normally it gets messed up because
 * of the timezone conversion
 */
void
fs_print_day(FILE * hFile, time_t time)
{
    if (time <= 0) {
	fprintf(hFile, "0000.00.00 00:00:00 (UTC)");
    }
    else {
	struct tm *tmTime = localtime(&time);

	fprintf(hFile, "%.4d.%.2d.%.2d 00:00:00 (%s)",
	    (int) tmTime->tm_year + 1900,
	    (int) tmTime->tm_mon + 1, (int) tmTime->tm_mday,
	    tzname[(tmTime->tm_isdst == 0) ? 0 : 1]);
    }
}


/* simple print of dentry type / inode type, deleted, inode, and
 * name
 *
 * fs_data is used for alternate data streams in NTFS, set to NULL
 * for all other file systems
 *
 * A newline is not printed at the end
 *
 * If path is NULL, then skip else use. it has the full directory name
 *  It needs to end with "/"
 */
void
fs_dent_print(FILE * hFile, FS_DENT * fs_dent, int flags, FS_INFO * fs,
    FS_DATA * fs_data)
{
    FS_INODE *fs_inode = fs_dent->fsi;

    /* type of file - based on dentry type */
    if ((fs_dent->ent_type & FS_DENT_MASK) < FS_DENT_MAX_STR)
	fprintf(hFile, "%s/",
	    fs_dent_str[fs_dent->ent_type & FS_DENT_MASK]);
    else
	fprintf(hFile, "-/");

    /* type of file - based on inode type: we want letters though for
     * regular files so we use the dent_str though */
    if (fs_inode) {
	int typ = (fs_inode->mode & FS_INODE_FMT) >> FS_INODE_SHIFT;
	if ((typ & FS_INODE_MASK) < FS_DENT_MAX_STR)
	    fprintf(hFile, "%s ", fs_dent_str[typ & FS_INODE_MASK]);
	else
	    fprintf(hFile, "- ");
    }
    else {
	fprintf(hFile, "- ");
    }


    /* print a * if it is deleted */
    if (flags & FS_FLAG_NAME_UNALLOC)
	fprintf(hFile, "* ");

    fprintf(hFile, "%" PRIuINUM "", fs_dent->inode);

    /* print the id and type if we have fs_data (NTFS) */
    if (fs_data)
	fprintf(hFile, "-%lu-%lu", (ULONG) fs_data->type,
	    (ULONG) fs_data->id);

    fprintf(hFile, "%s:\t",
	((fs_inode) && (fs_inode->flags & FS_FLAG_META_ALLOC) &&
	    (flags & FS_FLAG_NAME_UNALLOC)) ? "(realloc)" : "");

    if (fs_dent->path != NULL)
	fprintf(hFile, "%s", fs_dent->path);

    fprintf(hFile, "%s", fs_dent->name);

/*  This will add the short name in parentheses
    if (fs_dent->shrt_name != NULL && fs_dent->shrt_name[0] != '\0')
	fprintf(hFile, " (%s)", fs_dent->shrt_name);
*/

    /* print the data stream name if we the non-data NTFS stream */
    if (fs_data) {
	if (((fs_data->type == NTFS_ATYPE_DATA) &&
		(strcmp(fs_data->name, "$Data") != 0)) ||
	    ((fs_data->type == NTFS_ATYPE_IDXROOT) &&
		(strcmp(fs_data->name, "$I30") != 0)))
	    fprintf(hFile, ":%s", fs_data->name);
    }

    return;
}

/* Print contents of  fs_dent entry format like ls -l
**
** All elements are tab delimited 
**
** If path is NULL, then skip else use. it has the full directory name
**  It needs to end with "/"
*/
void
fs_dent_print_long(FILE * hFile, FS_DENT * fs_dent, int flags,
    FS_INFO * fs, FS_DATA * fs_data)
{
    FS_INODE *fs_inode = fs_dent->fsi;

    fs_dent_print(hFile, fs_dent, flags, fs, fs_data);

    if ((fs == NULL) || (fs_inode == NULL)) {

	fprintf(hFile, "\t0000.00.00 00:00:00 (GMT)");
	fprintf(hFile, "\t0000.00.00 00:00:00 (GMT)");
	fprintf(hFile, "\t0000.00.00 00:00:00 (GMT)");

	fprintf(hFile, "\t0\t0\t0\n");
    }
    else {

	/* MAC times */
	fprintf(hFile, "\t");
	fs_print_time(hFile, fs_inode->mtime);

	fprintf(hFile, "\t");
	/* FAT only gives the day of last access */
	if ((fs->ftype & FSMASK) != FATFS_TYPE)
	    fs_print_time(hFile, fs_inode->atime);
	else
	    fs_print_day(hFile, fs_inode->atime);

	fprintf(hFile, "\t");
	fs_print_time(hFile, fs_inode->ctime);


	/* use the stream size if one was given */
	if (fs_data)
	    fprintf(hFile, "\t%llu", (ULLONG) fs_data->size);
	else
	    fprintf(hFile, "\t%llu", (ULLONG) fs_inode->size);

	fprintf(hFile, "\t%lu\t%lu\n",
	    (ULONG) fs_inode->gid, (ULONG) fs_inode->uid);
    }

    return;
}


/*
** Print output in the format that mactime reads.
** This allows the deleted files to be inserted to get a better
** picture of what happened
**
** Prepend fs_dent->path when printing full file name
**  dir needs to end with "/" 
**
** prepend *prefix to path as the mounting point that the original
** grave-robber was run on
**
** flags is used to check if the UNALLOC flag is set
**
** If the flags in the fs_inode structure are set to FS_FLAG_ALLOC
** then it is assumed that the inode has been reallocated and the
** contents are not displayed
**
** fs is not required (only used for block size).  
*/
void
fs_dent_print_mac(FILE * hFile, FS_DENT * fs_dent, int flags,
    FS_INFO * fs, FS_DATA * fs_data, char *prefix)
{
    FS_INODE *fs_inode;
    char ls[12];

    if ((!hFile) || (!fs_dent))
	return;

    fs_inode = fs_dent->fsi;

    /* md5 */
    fprintf(hFile, "0|");

    /* file name */
    fprintf(hFile, "%s%s%s", prefix, fs_dent->path, fs_dent->name);

    /* print the data stream name if it exists and is not the default NTFS */
    if ((fs_data) && (((fs_data->type == NTFS_ATYPE_DATA) &&
		(strcmp(fs_data->name, "$Data") != 0)) ||
	    ((fs_data->type == NTFS_ATYPE_IDXROOT) &&
		(strcmp(fs_data->name, "$I30") != 0))))
	fprintf(hFile, ":%s", fs_data->name);

    if ((fs_inode) && ((fs_inode->mode & FS_INODE_FMT) == FS_INODE_LNK) &&
	(fs_inode->link)) {
	fprintf(hFile, " -> %s", fs_inode->link);
    }

    /* if filename is deleted add a comment and if the inode is now
     * allocated, then add realloc comment */
    if (flags & FS_FLAG_NAME_UNALLOC)
	fprintf(hFile, " (deleted%s)", ((fs_inode)
		&& (fs_inode->
		    flags & FS_FLAG_META_ALLOC)) ? "-realloc" : "");

    /* device, inode */
    fprintf(hFile, "|0|%" PRIuINUM "", fs_dent->inode);
    if (fs_data)
	fprintf(hFile, "-%lu-%lu", (ULONG) fs_data->type,
	    (ULONG) fs_data->id);

    /* mode val */
    fprintf(hFile, "|%lu|", (ULONG) ((fs_inode) ? fs_inode->mode : 0));

    /* TYPE as specified in the directory entry 
     * we want '-' for a regular file, so use the inode_str array
     */
    if ((fs_dent->ent_type & FS_DENT_MASK) < FS_INODE_MAX_STR)
	fprintf(hFile, "%s/",
	    fs_inode_str[fs_dent->ent_type & FS_DENT_MASK]);
    else
	fprintf(hFile, "-/");

    if (!fs_inode) {
	fprintf(hFile, "----------|0|0|0|0|0|0|0|0|");
    }
    else {

	/* mode as string */
	make_ls(fs_inode->mode, ls);
	fprintf(hFile, "%s|", ls);

	/* num link, uid, gid, rdev */
	fprintf(hFile, "%d|%d|%d|0|", (int) fs_inode->nlink,
	    (int) fs_inode->uid, (int) fs_inode->gid);

	/* size - use data stream if we have it */
	if (fs_data)
	    fprintf(hFile, "%" PRIuOFF "|", fs_data->size);
	else
	    fprintf(hFile, "%" PRIuOFF "|", fs_inode->size);

	/* atime, mtime, ctime */
	fprintf(hFile, "%lu|%lu|%lu|",
	    (ULONG) fs_inode->atime, (ULONG) fs_inode->mtime,
	    (ULONG) fs_inode->ctime);
    }

    /* block size and num of blocks */
    fprintf(hFile, "%u|0\n", (fs) ? fs->block_size : 0);

}
