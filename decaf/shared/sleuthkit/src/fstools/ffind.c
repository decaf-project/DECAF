/*
** ffind  (file find)
** The Sleuth Kit 
**
** $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
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

void
usage()
{
    fprintf(stderr,
	"usage: %s [-aduvV] [-f fstype] [-i imgtype] [-o imgoffset] image [images] inode\n",
	progname);
    fprintf(stderr, "\t-a: Find all occurrences\n");
    fprintf(stderr, "\t-d: Find deleted entries ONLY\n");
    fprintf(stderr, "\t-u: Find undeleted entries ONLY\n");
    fprintf(stderr, "\t-i imgtype: The format of the image file\n");
    fprintf(stderr,
	"\t-o imgoffset: The offset of the file system in the image (in sectors)\n");
    fprintf(stderr, "\t-v: Verbose output to stderr\n");
    fprintf(stderr, "\t-V: Print version\n");
    fprintf(stderr, "\t-f fstype: Image file system type\n");
    fs_print_types(stderr);
    fprintf(stderr, "Supported image format types:\n");
    img_print_types(stderr);

    exit(1);
}


int
main(int argc, char **argv)
{
    char *fstype = NULL;
    int flags = FS_FLAG_NAME_RECURSE;
    int ch;
    char *cp;
    FS_INFO *fs;
    extern int optind;
    uint32_t type;
    uint16_t id;
    char *imgtype = NULL, *dash;
    IMG_INFO *img;
    uint8_t localflags = 0;
    INUM_T inode;
    SSIZE_T imgoff = 0;

    progname = argv[0];
    setlocale(LC_ALL, "");

    while ((ch = getopt(argc, argv, "adf:i:o:uvV")) > 0) {
	switch (ch) {
	case 'a':
	    localflags |= FFIND_ALL;
	    break;
	case 'd':
	    flags |= FS_FLAG_NAME_UNALLOC;
	    break;
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
	case 'u':
	    flags |= FS_FLAG_NAME_ALLOC;
	    break;
	case 'v':
	    verbose++;
	    break;
	case 'V':
	    print_version(stdout);
	    exit(0);
	case '?':
	default:
	    fprintf(stderr, "Invalid argument: %s\n", argv[optind]);
	    usage();
	}
    }

    /* if the user did not specify either of the alloc/unalloc flags
     ** then show them all
     */
    if ((!(flags & FS_FLAG_NAME_ALLOC))
	&& (!(flags & FS_FLAG_NAME_UNALLOC)))
	flags |= (FS_FLAG_NAME_ALLOC | FS_FLAG_NAME_UNALLOC);


    if (optind + 1 >= argc) {
	fprintf(stderr, "Missing image name and/or address\n");
	usage();
    }


    /* we have the inum-type or inum-type-id format */
    type = 0;
    id = 0;
    if ((dash = strchr(argv[argc - 1], '-')) != NULL) {
	char *dash2;

	*dash = '\0';
	dash++;

	/* We have an id */
	if ((dash2 = strchr(dash, '-')) != NULL) {
	    *dash2 = '\0';
	    dash2++;

	    id = (uint16_t) strtoul(dash2, &cp, 0);
	    if (*cp || cp == dash2) {
		fprintf(stderr, "Invalid id in inode: %s\n", dash2);
		usage();
	    }
	}
	type = strtoul(dash, &cp, 0);
	if (*cp || cp == dash) {
	    fprintf(stderr, "Invalid type in inode: %s\n", dash);
	    usage();
	}
    }
    inode = strtoull(argv[argc - 1], &cp, 0);
    if (*cp || cp == argv[argc - 1]) {
	fprintf(stderr, "Invalid inode address: %s\n", argv[argc - 1]);
	usage();
    }


    /* open image */
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


    if (inode < fs->first_inum) {
	fprintf(stderr, "Inode is too small for image (%" PRIuINUM ")\n",
	    fs->first_inum);
	exit(1);
    }
    if (inode > fs->last_inum) {
	fprintf(stderr, "Inode is too large for image (%" PRIuINUM ")\n",
	    fs->last_inum);
	exit(1);
    }

    if (fs_ffind(fs, localflags, inode, type, id, flags)) {
	tsk_error_print(stderr);
	fs->close(fs);
	img->close(img);
	exit(1);
    }

    fs->close(fs);
    img->close(img);

    exit(0);
}
