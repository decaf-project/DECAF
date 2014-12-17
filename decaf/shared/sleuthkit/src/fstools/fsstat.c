/*
** fsstat
** The Sleuth Kit 
**
** $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
**
** Brian Carrier [carrier@sleuthkit.org]
** Copyright (c) 2006 Brian Carrier, Basis Technology.  All Rights reserved
** Copyright (c) 2003-2005 Brian Carrier.  All rights reserved 
**
** TASK
** Copyright (c) 2002 Brian Carrier, @stake Inc. All Rights reserved
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
	"usage: %s [-tvV] [-f fstype] [-i imgtype] [-o imgoffset] image\n",
	progname);
    fprintf(stderr, "\t-t: display type only\n");
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
    FS_INFO *fs;
    IMG_INFO *img;
    char *fstype = NULL;
    char *imgtype = NULL;
    int ch;
    uint8_t type = 0;
    SSIZE_T imgoff = 0;

    progname = argv[0];
    setlocale(LC_ALL, "");

    while ((ch = getopt(argc, argv, "f:i:o:tvV")) > 0) {
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

    if ((fs = fs_open(img, imgoff, fstype)) == NULL) {
	tsk_error_print(stderr);
	if (tsk_errno == TSK_ERR_FS_UNSUPTYPE)
	    fs_print_types(stderr);
	img->close(img);
	exit(1);
    }


    if (type) {
	char *str = fs_get_type(fs->ftype);
	printf("%s\n", str);
    }
    else {
	if (fs->fsstat(fs, stdout)) {
	    tsk_error_print(stderr);
	    fs->close(fs);
	    img->close(img);
	    exit(1);
	}
    }

    fs->close(fs);
    img->close(img);

    exit(0);
}
