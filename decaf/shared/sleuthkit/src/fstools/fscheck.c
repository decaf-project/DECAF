/*
** fscheck
** The Sleuth Kit 
**
** $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
**
** Brian Carrier [carrier@sleuthkit.org]
** Copyright (c) 2004-2005 Brian Carrier.  All rights reserved 
**
**
** This software is distributed under the Common Public License 1.0
**
*/
#include "fs_tools.h"


static void
usage()
{
    fprintf(stderr,
	"usage: %s [-vV] [-f fstype] [-i imgtype] [-o imgoffset] image [images]\n",
	progname);
    fprintf(stderr, "\t-i imgtype: The format of the image file\n");
    fprintf(stderr,
	"\t-o imgoffset: The offset of the file system in the image (in sectors)\n");
    fprintf(stderr, "\t-v: verbose output to stderr\n");
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
    char *fstype = NULL;
    int ch;
    FS_INFO *fs;
    char *imgtype = NULL, *imgoff = NULL;
    IMG_INFO *img;

    progname = argv[0];

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
	    imgoff = optarg;
	    break;

	case 'v':
	    verbose++;
	    break;

	case 'V':
	    print_version(stdout);
	    exit(0);
	}
    }

    /* We need at least one more argument */
    if (optind >= argc) {
	fprintf(stderr, "Missing image name\n");
	usage();
    }

    img =
	img_open(imgtype, imgoff, argc - optind,
	(const char **) &argv[optind]);
    if (img == NULL) {
	tsk_error_print(stderr);
	exit(1);
    }

    if (fs = fs_open(img, fstype)) {
	if (tsk_errno == TSK_ERR_FS_UNSUPTYPE)
	    tsk_print_types(stderr);

	tsk_error_print(stderr);
	img->close(img);
	exit(1);

    }

    if (fs->fscheck(fs, stdout)) {
	tsk_error_print(stderr);
	fs->close(fs);
	img->close(img);
	exit(1);
    }

    fs->close(fs);
    img->close(img);

    exit(0);
}
