/*
** icat 
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

 * LICENSE
 *	This software is distributed under the IBM Public License.
 * AUTHOR(S)
 *	Wietse Venema
 *	IBM T.J. Watson Research
 *	P.O. Box 704
 *	Yorktown Heights, NY 10598, USA
 --*/

#include "libfstools.h"


/* usage - explain and terminate */

static void
usage()
{
    fprintf(stderr,
	"usage: %s [-hHsvV] [-f fstype] [-i imgtype] [-o imgoffset] image [images] inum[-typ[-id]]\n",
	progname);
    fprintf(stderr, "\t-h: Do not display holes in sparse files\n");
    fprintf(stderr, "\t-r: Recover deleted file\n");
    fprintf(stderr, "\t-s: Display slack space at end of file\n");
    fprintf(stderr, "\t-i imgtype: The format of the image file\n");
    fprintf(stderr,
	"\t-o imgoffset: The offset of the file system in the image (in sectors)\n");
    fprintf(stderr, "\t-v: verbose to stderr\n");
    fprintf(stderr, "\t-V: Print version\n");
    fprintf(stderr, "\t-f fstype: File system type\n");
    fs_print_types(stderr);
    fprintf(stderr, "Supported image format types:\n");
    img_print_types(stderr);

    exit(1);
}

int
main(int argc, char **argv)
{
    FS_INFO *fs;
    char *cp;
    INUM_T inum;
    int flags = 0;
    int ch;
    char *fstype = NULL;
    char *imgtype = NULL, *dash;
    IMG_INFO *img;
    uint32_t type = 0;
    uint16_t id = 0;
    int id_used = 0;
    int retval;
    SSIZE_T imgoff = 0;

    progname = argv[0];
    setlocale(LC_ALL, "");

    while ((ch = getopt(argc, argv, "f:hi:o:rsvV")) > 0) {
	switch (ch) {
	case '?':
	default:
	    fprintf(stderr, "Invalid argument: %s\n", argv[optind]);
	    usage();
	case 'f':
	    fstype = optarg;
	    break;
	case 'h':
	    flags |= FS_FLAG_FILE_NOSPARSE;
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
	case 'r':
	    flags |= FS_FLAG_FILE_RECOVER;
	    break;
	case 's':
	    flags |= FS_FLAG_FILE_SLACK;
	    break;
	case 'v':
	    verbose++;
	    break;
	case 'V':
	    print_version(stdout);
	    exit(0);
	}
    }

    /* We need at least two more argument */
    if (optind + 1 >= argc) {
	fprintf(stderr, "Missing image name and/or address\n");
	usage();
    }

    /* Get the inode address */
    /* simple inode usage */
    if ((dash = strchr(argv[argc - 1], '-')) == NULL) {
	inum = strtoull(argv[argc - 1], &cp, 0);
	if (*cp || cp == argv[argc - 1]) {
	    fprintf(stderr, "Invalid inode address: %s\n", argv[argc - 1]);
	    usage();
	}
    }

    /* inum-type or inum-type-id format */
    else {
	char *dash2;
	*dash = '\0';
	dash++;

	if ((dash2 = strchr(dash, '-')) == NULL) {
	    id = 0;
	}
	else {
	    *dash2 = '\0';
	    dash2++;

	    id = (uint16_t) strtoul(dash2, &cp, 0);
	    id_used = 1;
	    if (*cp || cp == dash2) {
		fprintf(stderr, "Invalid id in inode\n");
		usage();
	    }
	}

	inum = strtoull(argv[argc - 1], &cp, 0);
	if (*cp || cp == argv[argc - 1]) {
	    fprintf(stderr, "Invalid inode address: %s\n", argv[argc - 1]);
	    usage();
	}

	type = strtoul(dash, &cp, 0);
	if (*cp || cp == dash) {
	    fprintf(stderr, "Invalid type in inode\n");
	    usage();
	}
    }

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

    if (inum > fs->last_inum) {
	fprintf(stderr,
	    "Metadata address too large for image (%" PRIuINUM ")\n",
	    fs->last_inum);
	fs->close(fs);
	img->close(img);
	exit(1);
    }
    if (inum < fs->first_inum) {
	fprintf(stderr,
	    "Metadata address too small for image (%" PRIuINUM ")\n",
	    fs->first_inum);
	fs->close(fs);
	img->close(img);
	exit(1);
    }


    if (id_used)
	retval = fs_icat(fs, 0, inum, type, id, flags);
    /* If the id value was not used, then set the flag accordingly so the '0' value is ignored */
    else
	retval = fs_icat(fs, 0, inum, type, id, flags | FS_FLAG_FILE_NOID);

    if (retval) {
	tsk_error_print(stderr);
	fs->close(fs);
	img->close(img);
	exit(1);

    }
    fs->close(fs);
    img->close(img);

    exit(0);
}
