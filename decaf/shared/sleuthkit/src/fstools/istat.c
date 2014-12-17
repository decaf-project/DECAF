/*
** istat
** The Sleuth Kit 
**
** $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
**
** Display all inode info about a given inode.
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


/* usage - explain and terminate */

static void
usage()
{
    fprintf(stderr,
	"usage: %s [-b num] [-f fstype] [-i imgtype] [-o imgoffset] [-z zone] [-s seconds] [-vV] image inum\n",
	progname);
    fprintf(stderr,
	"\t-b num: force the display of NUM address of block pointers\n");
    fprintf(stderr,
	"\t-z zone: time zone of original machine (i.e. EST5EDT or GMT)\n");
    fprintf(stderr,
	"\t-s seconds: Time skew of original machine (in seconds)\n");
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
    char *cp, *dash;
    char *fstype = NULL;
    FS_INFO *fs;
    int32_t sec_skew = 0;
    char *imgtype = NULL;
    IMG_INFO *img;
    SSIZE_T imgoff = 0;


    /* When > 0 this is the number of blocks to print, used for -b arg */
    DADDR_T numblock = 0;

    progname = argv[0];
    setlocale(LC_ALL, "");


    while ((ch = getopt(argc, argv, "b:f:i:o:s:vVz:")) > 0) {
	switch (ch) {
	case '?':
	default:
	    fprintf(stderr, "Invalid argument: %s\n", argv[optind]);
	    usage();
	case 'b':
	    numblock = strtoull(optarg, &cp, 0);
	    if (*cp || cp == optarg || numblock < 1) {
		fprintf(stderr,
		    "invalid argument: block count must be positive: %s\n",
		    optarg);
		usage();
	    }
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

	case 's':
	    sec_skew = atoi(optarg);
	    break;

	case 'v':
	    verbose++;
	    break;

	case 'V':
	    print_version(stdout);
	    exit(0);
	case 'z':
	    {
		char envstr[32];
		snprintf(envstr, 32, "TZ=%s", optarg);
		if (0 != putenv(envstr)) {
		    fprintf(stderr, "error setting environment");
		    exit(1);
		}

		tzset();
	    }
	    break;

	}
    }

    /* We need at least two more argument */
    if (optind + 1 >= argc) {
	fprintf(stderr, "Missing image name and/or address\n");
	usage();
    }

    /* if we are given the inode in the inode-type-id form, then ignore
     * the other stuff w/out giving an error 
     *
     * This will make scripting easier
     */
    if ((dash = strchr(argv[argc - 1], '-')) != NULL) {
	*dash = '\0';
    }

    inum = strtoull(argv[argc - 1], &cp, 0);
    if (*cp || cp == argv[argc - 1]) {
	fprintf(stderr, "bad inode number: %s", argv[argc - 1]);
	usage();
    }

    /*
     * Open the file system.
     */
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
	    "Metadata address is too large for image (%" PRIuINUM ")\n",
	    fs->last_inum);
	fs->close(fs);
	img->close(img);
	exit(1);
    }

    if (inum < fs->first_inum) {
	fprintf(stderr,
	    "Metadata address is too small for image (%" PRIuINUM ")\n",
	    fs->first_inum);
	fs->close(fs);
	img->close(img);
	exit(1);
    }

    if (fs->istat(fs, stdout, inum, numblock, sec_skew)) {
	tsk_error_print(stderr);
	fs->close(fs);
	img->close(img);
	exit(1);
    }

    fs->close(fs);
    img->close(img);
    exit(0);
}
