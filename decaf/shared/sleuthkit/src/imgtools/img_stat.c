/*
 * imgstat
 * The Sleuth Kit 
 *
 * $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
 *
 * Brian Carrier [carrier@sleuthkit.org]
 * Copyright (c) 2005 Brian Carrier.  All rights reserved 
 *
 *
 * This software is distributed under the Common Public License 1.0
 *
 */
#include "img_tools.h"

static void
usage()
{
    fprintf(stderr, "usage: %s [-tvV] [-i imgtype] image\n", progname);
    fprintf(stderr, "\t-t: display type only\n");
    fprintf(stderr, "\t-i imgtype: The format of the image file\n");
    fprintf(stderr, "\t-v: verbose output to stderr\n");
    fprintf(stderr, "\t-V: Print version\n");
    fprintf(stderr, "Supported image format types:\n");
    img_print_types(stderr);

    exit(1);
}


int
main(int argc, char **argv)
{
    IMG_INFO *img;
    char *imgtype = NULL;
    int ch;
    uint8_t type = 0;

    progname = argv[0];

    while ((ch = getopt(argc, argv, "i:tvV")) > 0) {
	switch (ch) {
	case '?':
	default:
	    fprintf(stderr, "Invalid argument: %s\n", argv[optind]);
	    usage();

	case 'i':
	    imgtype = optarg;
	    break;

	case 't':
	    type = 1;
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

    if ((img =
	    img_open(imgtype, argc - optind,
		(const char **) &argv[optind])) == NULL) {
	tsk_error_print(stderr);
	exit(1);
    }

    if (type) {
	char *str = img_get_type(img->itype);
	printf("%s\n", str);
    }
    else {
	img->imgstat(img, stdout);
    }

    img->close(img);
    exit(0);
}
