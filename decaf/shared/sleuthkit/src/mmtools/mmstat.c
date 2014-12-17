/*
 * The Sleuth Kit
 *
 * $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
 *
 * Brian Carrier [carrier@sleuthkit.org]
 * Copyright (c) 2005 Brian Carrier.  All rights reserved
 *
 * mmstat - Get stats on the volume system / media management
 *
 *
 * This software is distributed under the Common Public License 1.0
 *
 */

#include "mm_tools.h"


extern char *progname;


void
usage()
{
    fprintf(stderr,
	"%s [-i imgtype] [-o imgoffset] [-vV] [-t mmtype] image [images]\n",
	progname);
    fprintf(stderr, "\t-t mmtype: The type of partition system\n");
    fprintf(stderr, "\t-i imgtype: The format of the image file\n");
    fprintf(stderr,
	"\t-o imgoffset: Offset to the start of the volume that contains the partition system (in sectors)\n");
    fprintf(stderr, "\t-v: verbose output\n");
    fprintf(stderr, "\t-V: print the version\n");
    mm_print_types(stderr);
    fprintf(stderr, "Supported image formats:\n");
    img_print_types(stderr);
    exit(1);
}

static void
print_stats(MM_INFO * mm)
{
    printf("%s\n", mm_get_type(mm->mmtype));
    //printf("Type: %s\n", mm->str_type);
    return;
}

int
main(int argc, char **argv)
{
    MM_INFO *mm;
    char *mmtype = NULL;
    int ch;
    SSIZE_T imgoff = 0;
    char *imgtype = NULL;
    IMG_INFO *img;

    progname = argv[0];

    while ((ch = getopt(argc, argv, "i:o:t:vV")) > 0) {
	switch (ch) {
	case 'i':
	    imgtype = optarg;
	    break;

	case 'o':
	    if ((imgoff = parse_offset(optarg)) == -1) {
		tsk_error_print(stderr);
		exit(1);
	    }

	    break;
	case 't':
	    mmtype = optarg;
	    break;
	case 'v':
	    verbose++;
	    break;
	case 'V':
	    print_version(stdout);
	    exit(0);
	case '?':
	default:
	    fprintf(stderr, "Unknown argument\n");
	    usage();
	}
    }

    /* We need at least one more argument */
    if (optind >= argc) {
	fprintf(stderr, "Missing image name\n");
	usage();
    }

    /* open the image */
    if ((img =
	    img_open(imgtype, argc - optind,
		(const char **) &argv[optind])) == NULL) {
	tsk_error_print(stderr);
	exit(1);
    }


    /* process the partition tables */
    if ((mm = mm_open(img, imgoff, mmtype)) == NULL) {
	tsk_error_print(stderr);
	if (tsk_errno == TSK_ERR_MM_UNSUPTYPE)
	    mm_print_types(stderr);

	exit(1);
    }

    print_stats(mm);

    mm->close(mm);
    img->close(img);
    exit(0);
}
