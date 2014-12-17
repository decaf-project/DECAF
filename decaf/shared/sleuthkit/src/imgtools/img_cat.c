/*
 * img_cat
 * The Sleuth Kit 
 *
 * $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
 *
 * Brian Carrier [carrier@sleuthkit.org]
 * Copyright (c) 2006 Brian Carrier, Basis Technology.  All rights reserved 
 *
 * This software is distributed under the Common Public License 1.0
 *
 */
#include "img_tools.h"

static void
usage()
{
    fprintf(stderr, "usage: %s [-vV] [-i imgtype] image\n", progname);
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
    SSIZE_T cnt, done;

    progname = argv[0];

    while ((ch = getopt(argc, argv, "i:vV")) > 0) {
	switch (ch) {
	case '?':
	default:
	    fprintf(stderr, "Invalid argument: %s\n", argv[optind]);
	    usage();

	case 'i':
	    imgtype = optarg;
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

    for (done = 0; done < img->size; done += cnt) {
	char buf[16 * 1024];

	cnt = img->read_random(img, 0, buf, sizeof(buf), done);
	if (cnt == -1) {
	    tsk_error_print(stderr);
	    exit(1);
	}

	if (fwrite(buf, cnt, 1, stdout) != 1) {
	    tsk_errno = TSK_ERR_IMG_WRITE;
	    snprintf(tsk_errstr, TSK_ERRSTR_L,
		"img_cat: Error writing to stdout:  %s", strerror(errno));
	    exit(1);
	}
    }

    img->close(img);
    exit(0);
}
