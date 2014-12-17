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

#include "libfstools.h"


/* atoinum - convert string to inode number */

INUM_T
atoinum(const char *str)
{
    char *cp;
    INUM_T inum;

    if (*str == 0)
	return (0);

    inum = strtoull(str, &cp, 0);
    if (*cp || cp == str) {
	fprintf(stderr, "bad inode number: %s", str);
	exit(1);
    }

    return (inum);
}

/* usage - explain and terminate */

static void
usage()
{
    fprintf(stderr,
	"usage: %s [-eOmrvV] [-aAlLzZ] [-f fstype] [-i imgtype] [-o imgoffset] [-s seconds] image [images] [inum[-end]]\n",
	progname);

    fprintf(stderr, "\t-e: Display all inodes\n");
    fprintf(stderr,
	"\t-O: Display inodes that are removed, but sill open (was -o)\n");
    fprintf(stderr,
	"\t-m: Display output in the mactime format (replaces ils2mac from TCT)\n");
    fprintf(stderr, "\t-r: Display removed inodes (default)\n");
    fprintf(stderr,
	"\t-s seconds: Time skew of original machine (in seconds)\n");
    fprintf(stderr, "\t-a: Allocated files\n");
    fprintf(stderr, "\t-A: Un-Allocated files\n");
    fprintf(stderr, "\t-l: Linked files\n");
    fprintf(stderr, "\t-L: Un-Linked files\n");
    fprintf(stderr, "\t-z: Un-Used files (ctime is 0)\n");
    fprintf(stderr, "\t-Z: Used files (ctime is not 0)\n");
    fprintf(stderr, "\t-i imgtype: The format of the image file\n");
    fprintf(stderr,
	"\t-o imgoffset: The offset of the file system in the image (in sectors)\n");
    fprintf(stderr, "\t-v: verbose output to stderr\n");
    fprintf(stderr, "\t-V: Display version number\n");
    fprintf(stderr, "\t-f fstype: File system type\n");
    fs_print_types(stderr);
    fprintf(stderr, "Supported image format types:\n");
    img_print_types(stderr);
    exit(1);
}



/* main - open file system, list inode info */

int
main(int argc, char **argv)
{
    INUM_T istart = 0, ilast = 0;
    int ch;
    int flags = 0;

    int argflags = 0;
    char *fstype = NULL;
    FS_INFO *fs;
    char *imgtype = NULL, *cp, *dash;
    SSIZE_T imgoff = 0;
    IMG_INFO *img;
    int set_range = 1;
    char *image = NULL;
    int32_t sec_skew = 0;

    progname = argv[0];
    setlocale(LC_ALL, "");

    /*
     * Provide convenience options for the most commonly selected feature
     * combinations.
     */
    while ((ch = getopt(argc, argv, "aAef:i:lLo:Omrs:vVzZ")) > 0) {
	switch (ch) {
	case '?':
	default:
	    fprintf(stderr, "Invalid argument: %s\n", argv[optind]);
	    usage();
	case 'e':
	    flags |= ~0;
	    break;
	case 'f':
	    fstype = optarg;
	    break;
	case 'i':
	    imgtype = optarg;
	    break;
	case 'm':
	    argflags |= ILS_MAC;
	    break;
	case 'o':
	    if ((imgoff = parse_offset(optarg)) == -1) {
		tsk_error_print(stderr);
		exit(1);
	    }
	    break;
	case 'O':
	    flags |= (FS_FLAG_META_ALLOC | FS_FLAG_META_UNLINK);
	    argflags |= ILS_OPEN;
	    break;
	case 'r':
	    argflags |= ILS_REM;
	    break;
	case 's':
	    sec_skew = atoi(optarg);
	    break;
	case 'v':
	    verbose++;
	    break;
	case 'V':
	    print_version(stdout);
	    exit(0);

	    /*
	     * Provide fine controls to tweak one feature at a time.
	     */
	case 'a':
	    flags |= FS_FLAG_META_ALLOC;
	    flags &= ~FS_FLAG_META_UNALLOC;
	    break;
	case 'A':
	    flags |= FS_FLAG_META_UNALLOC;
	    flags &= ~FS_FLAG_META_ALLOC;
	    break;
	case 'l':
	    flags |= FS_FLAG_META_LINK;
	    flags &= ~FS_FLAG_META_UNLINK;
	    break;
	case 'L':
	    flags |= FS_FLAG_META_UNLINK;
	    flags &= ~FS_FLAG_META_LINK;
	    break;
	case 'z':
	    flags |= FS_FLAG_META_UNUSED;
	    flags &= ~FS_FLAG_META_USED;
	    break;
	case 'Z':
	    flags |= FS_FLAG_META_USED;
	    flags &= ~FS_FLAG_META_UNUSED;
	    break;
	}
    }

    if (optind >= argc) {
	fprintf(stderr, "Missing image name\n");
	usage();
    }


    /* We need to determine if an inode or inode range was given */
    if ((dash = strchr(argv[argc - 1], '-')) == NULL) {
	/* Check if is a single number */
	istart = strtoull(argv[argc - 1], &cp, 0);
	if (*cp || cp == argv[argc - 1]) {
	    /* Not a number - consider it a file name */
	    image = argv[optind];
	    if ((img =
		    img_open(imgtype, argc - optind,
			(const char **) &argv[optind])) == NULL) {
		tsk_error_print(stderr);
		exit(1);
	    }

	}
	else {
	    /* Single address set end addr to start */
	    ilast = istart;
	    set_range = 0;
	    image = argv[optind];
	    if ((img =
		    img_open(imgtype, argc - optind - 1,
			(const char **) &argv[optind])) == NULL) {
		tsk_error_print(stderr);
		exit(1);
	    }

	}
    }
    else {
	/* We have a dash, but it could be part of the file name */
	*dash = '\0';

	istart = strtoull(argv[argc - 1], &cp, 0);
	if (*cp || cp == argv[argc - 1]) {
	    /* Not a number - consider it a file name */
	    *dash = '-';
	    image = argv[optind];
	    if ((img =
		    img_open(imgtype, argc - optind,
			(const char **) &argv[optind])) == NULL) {
		tsk_error_print(stderr);
		exit(1);
	    }

	}
	else {
	    dash++;
	    ilast = strtoull(dash, &cp, 0);
	    if (*cp || cp == dash) {
		/* Not a number - consider it a file name */
		dash--;
		*dash = '-';
		image = argv[optind];
		if ((img =
			img_open(imgtype, argc - optind,
			    (const char **) &argv[optind])) == NULL) {
		    tsk_error_print(stderr);
		    exit(1);
		}

	    }
	    else {

		set_range = 0;
		/* It was a block range, so do not include it in the open */
		image = argv[optind];
		if ((img =
			img_open(imgtype, argc - optind - 1,
			    (const char **) &argv[optind])) == NULL) {
		    tsk_error_print(stderr);
		    exit(1);
		}

	    }
	}
    }

    if ((fs = fs_open(img, imgoff, fstype)) == NULL) {
	tsk_error_print(stderr);
	if (tsk_errno == TSK_ERR_FS_UNSUPTYPE)
	    fs_print_types(stderr);
	img->close(img);
	exit(1);
    }


    /* do we need to set the range or just check them? */
    if (set_range) {
	istart = fs->first_inum;
	ilast = fs->last_inum;
    }
    else {
	if (istart < fs->first_inum)
	    istart = fs->first_inum;

	if (ilast > fs->last_inum)
	    ilast = fs->last_inum;
    }

    /* NTFS uses alloc and link different than UNIX so change
     * the default behavior
     *
     * The link value can be > 0 on deleted files (even when closed)
     */

    /* NTFS and FAT have no notion of deleted but still open */
    if ((argflags & ILS_OPEN) &&
	(((fs->ftype & FSMASK) == NTFS_TYPE) ||
	    ((fs->ftype & FSMASK) == FATFS_TYPE))) {
	fprintf
	    (stderr,
	    "Error: '-o' argument does not work with NTFS and FAT images\n");
	exit(1);
    }

    /* removed inodes (default behavior) */
    if ((argflags & ILS_REM) || (flags == 0)) {
	if (((fs->ftype & FSMASK) == NTFS_TYPE) ||
	    ((fs->ftype & FSMASK) == FATFS_TYPE))
	    flags |= (FS_FLAG_META_USED | FS_FLAG_META_UNALLOC);
	else
	    flags |= (FS_FLAG_META_USED | FS_FLAG_META_UNLINK);
    }

    /* If neither of the flags in a family are set, then set both 
     *
     * Apply rules for default settings. Assume a "don't care" condition when
     * nothing is explicitly selected from a specific feature category.
     */
    if ((flags & (FS_FLAG_META_ALLOC | FS_FLAG_META_UNALLOC)) == 0)
	flags |= FS_FLAG_META_ALLOC | FS_FLAG_META_UNALLOC;

    if ((flags & (FS_FLAG_META_LINK | FS_FLAG_META_UNLINK)) == 0)
	flags |= FS_FLAG_META_LINK | FS_FLAG_META_UNLINK;

    if ((flags & (FS_FLAG_META_USED | FS_FLAG_META_UNUSED)) == 0)
	flags |= FS_FLAG_META_USED | FS_FLAG_META_UNUSED;


    if (fs_ils(fs, argflags, istart, ilast, flags, sec_skew, image)) {
	tsk_error_print(stderr);
	fs->close(fs);
	img->close(img);
	exit(1);
    }

    fs->close(fs);
    img->close(img);
    exit(0);
}
