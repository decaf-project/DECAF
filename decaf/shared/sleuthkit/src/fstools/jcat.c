/*
** jcat
** The Sleuth Kit 
**
** $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
**
** Brian Carrier [carrier@sleuthkit.org]
** Copyright (c) 2006 Brian Carrier, Basis Technology.  All Rights reserved
** Copyright (c) 2004-2005 Brian Carrier.  All rights reserved
**
**
** This software is distributed under the Common Public License 1.0
**
*/

#include "fs_tools.h"



/* atoinum - convert string to inode number */
INUM_T
atoinum(const char *str)
{
    char *cp, *dash;
    INUM_T inum;

    if (*str == 0)
	return (0);

    /* if we are given the inode in the inode-type-id form, then ignore
     * the other stuff w/out giving an error 
     *
     * This will make scripting easier
     */
    if ((dash = strchr(str, '-')) != NULL) {
	*dash = '\0';
    }
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
	"usage: %s [-f fstype] [-i imgtype] [-o imgoffset] [-vV] image [images] [inode] blk\n",
	progname);
    fprintf(stderr, "\tblk: The journal block to view\n");
    fprintf(stderr,
	"\tinode: The file system inode where the journal is located\n");
    fprintf(stderr, "\t-i imgtype: The format of the image file\n");
    fprintf(stderr,
	"\t-o imgoffset: The offset of the file system in the image (in sectors)\n");
    fprintf(stderr, "\t-v: verbose output to stderr\n");
    fprintf(stderr, "\t-V: print version\n");
    fprintf(stderr, "\t-f fstype: File system type\n");
    fs_print_types(stderr);
    fprintf(stderr, "Supported image format types:\n");
    img_print_types(stderr);
    exit(1);
}


int
main(int argc, char **argv)
{
    INUM_T inum;
    int ch;
    char *fstype = NULL;
    FS_INFO *fs;
    DADDR_T blk;
    char *cp;
    char *imgtype = NULL;
    IMG_INFO *img;
    SSIZE_T imgoff = 0;

    progname = argv[0];
    setlocale(LC_ALL, "");

    while ((ch = getopt(argc, argv, "f:i:o:vV")) > 0) {
	switch (ch) {
	case '?':
	default:
	    fprintf(stderr, "Invalid argument: %s\n", argv[optind]);
	    usage();
	case 'f':
	    fstype = optarg;
	    break;
	case 'i':
	    imgtype = optarg;
	    break;

	case 'o':
	    if ((imgoff = parse_offset(optarg)) == -1) {
		tsk_error_print(stderr);
		exit(1);
	    }
	    break;
	case 'v':
	    verbose++;
	    break;
	case 'V':
	    print_version(stdout);
	    exit(0);

	}
    }

    /* We need at least two more arguments */
    if (optind + 1 >= argc) {
	fprintf(stderr, "Missing image name and/or block address\n");
	usage();
    }

    blk = strtoull(argv[argc - 1], &cp, 0);
    if (*cp || cp == argv[argc - 1]) {
	fprintf(stderr, "bad block number: %s", argv[argc - 1]);
	exit(1);
    }

    /* Do we have an inode as well? */
    inum = strtoull(argv[argc - 2], &cp, 0);
    if (*cp || cp == argv[argc - 2]) {
	/* Not a number therefore an image */

	if ((img =
		img_open(imgtype, argc - optind - 1,
		    (const char **) &argv[optind])) == NULL) {
	    tsk_error_print(stderr);
	    exit(1);
	}

	if ((fs = fs_open(img, imgoff, fstype)) == NULL) {
	    tsk_error_print(stderr);
	    if (tsk_errno == TSK_ERR_FS_UNSUPTYPE)
		fs_print_types(stderr);
	    img->close(img);
	    exit(1);
	}

	inum = fs->journ_inum;
    }
    else {
	if ((img =
		img_open(imgtype, argc - optind - 2,
		    (const char **) &argv[optind])) == NULL) {
	    tsk_error_print(stderr);
	    exit(1);
	}

	if ((fs = fs_open(img, imgoff, fstype)) == NULL) {
	    tsk_error_print(stderr);
	    if (tsk_errno == TSK_ERR_FS_UNSUPTYPE)
		fs_print_types(stderr);
	    img->close(img);
	    exit(1);
	}

    }

    if (inum > fs->last_inum) {
	fprintf(stderr,
	    "Inode value is too large for image (%" PRIuINUM ")\n",
	    fs->last_inum);
	fs->close(fs);
	img->close(img);
	exit(1);
    }

    if (inum < fs->first_inum) {
	fprintf(stderr,
	    "Inode value is too small for image (%" PRIuINUM ")\n",
	    fs->first_inum);
	fs->close(fs);
	img->close(img);
	exit(1);
    }

    if (fs->jopen == NULL) {
	fprintf(stderr,
	    "Journal support does not exist for this file system\n");
	fs->close(fs);
	img->close(img);
	return 1;
    }

    if (fs->jopen(fs, inum)) {
	tsk_error_print(stderr);
	fs->close(fs);
	img->close(img);
	exit(1);
    }
    if (fs->jblk_walk(fs, blk, blk, 0, 0, NULL)) {
	tsk_error_print(stderr);
	fs->close(fs);
	img->close(img);
	exit(1);
    }

    fs->close(fs);
    img->close(img);
    exit(0);
}
