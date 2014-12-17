/*
** dstat
** The Sleuth Kit 
**
** $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
**
** Get the details about a data unit
**
** Brian Carrier [carrier@sleuthkit.org]
** Copyright (c) 2006 Brian Carrier, Basis Technology.  All Rights reserved
** Copyright (c) 2003-2005 Brian Carrier.  All rights reserved 
**
** TASK
** Copyright (c) 2002 Brian Carrier, @stake Inc.  All rights reserved
**
** This software is distributed under the Common Public License 1.0
**
*/
#include "libfstools.h"

void
usage()
{
    fprintf(stderr,
	"usage: %s [-vV] [-f fstype] [-i imgtype] [-o imgoffset] image [images] addr\n",
	progname);
    fprintf(stderr, "\t-i imgtype: The format of the image file\n");
    fprintf(stderr,
	"\t-o imgoffset: The offset of the file system in the image (in sectors)\n");
    fprintf(stderr, "\t-v: Verbose output to stderr\n");
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
    char *cp;
    extern int optind;
    DADDR_T addr;
    FS_INFO *fs;
    int flags =
	(FS_FLAG_DATA_UNALLOC | FS_FLAG_DATA_ALLOC | FS_FLAG_DATA_META |
	FS_FLAG_DATA_CONT);
    char *imgtype = NULL;
    IMG_INFO *img;
    SSIZE_T imgoff = 0;

    progname = argv[0];
    setlocale(LC_ALL, "");

    while ((ch = getopt(argc, argv, "f:i:o:uvV")) > 0) {
	switch (ch) {
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
	case '?':
	default:
	    fprintf(stderr, "Invalid argument: %s\n", argv[optind]);
	    usage();
	}
    }

    if (optind + 1 >= argc) {
	fprintf(stderr, "Missing image name and/or address\n");
	usage();
    }

    /* Get the address */
    addr = strtoull(argv[argc - 1], &cp, 0);
    if (*cp || cp == argv[argc - 1]) {
	fprintf(stderr, "Invalid address\n");
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


    if (addr > fs->last_block) {
	fprintf(stderr,
	    "Data unit address too large for image (%" PRIuDADDR ")\n",
	    fs->last_block);
	fs->close(fs);
	img->close(img);
	exit(1);
    }
    if (addr < fs->first_block) {
	fprintf(stderr,
	    "Data unit address too small for image (%" PRIuDADDR ")\n",
	    fs->first_block);
	fs->close(fs);
	img->close(img);
	exit(1);
    }

    if (fs_dstat(fs, 0, addr, flags)) {
	tsk_error_print(stderr);
	fs->close(fs);
	img->close(img);
	exit(1);

    }

    fs->close(fs);
    img->close(img);

    exit(0);
}
