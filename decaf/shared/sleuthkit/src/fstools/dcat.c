/*
** dcat
** The  Sleuth Kit 
**
** $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
**
** Given an image , block number, and size, display the contents
** of the block to stdout.
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

#define DLS_TYPE "dls"
#define RAW_STR "raw"

void
usage()
{
    fprintf(stderr,
	"usage: %s [-ahsvVw] [-f fstype] [-i imgtype] [-o imgoffset] [-u usize] image [images] unit_addr [num]\n",
	progname);
    fprintf(stderr, "\t-a: displays in all ASCII \n");
    fprintf(stderr, "\t-h: displays in hexdump-like fashion\n");
    fprintf(stderr, "\t-i imgtype: The format of the image file\n");
    fprintf(stderr,
	"\t-o imgoffset: The offset of the file system in the image (in sectors)\n");
    fprintf(stderr,
	"\t-s: display basic block stats such as unit size, fragments, etc.\n");
    fprintf(stderr, "\t-v: verbose output to stderr\n");
    fprintf(stderr, "\t-V: display version\n");
    fprintf(stderr, "\t-w: displays in web-like (html) fashion\n");
    fprintf(stderr, "\t-f fstype: File system type\n");
    fprintf(stderr,
	"\t-u usize: size of each data unit in image (for raw, dls, swap)\n");
    fprintf(stderr,
	"\t[num] is the number of data units to display (default is 1)\n");
    fs_print_types(stderr);
    fprintf(stderr, "\t%s (Unallocated Space)\n", DLS_TYPE);
    fprintf(stderr, "Supported image format types:\n");
    img_print_types(stderr);

    exit(1);
}


int
main(int argc, char **argv)
{
    FS_INFO *fs = NULL;
    DADDR_T addr = 0;
    char *fstype = NULL;
    DADDR_T read_num_units;	/* Number of data units */
    int usize = 0;		/* Length of each data unit */
    int ch;
    char format = 0, *cp, *imgtype = NULL;
    IMG_INFO *img;
    extern int optind;
    SSIZE_T imgoff = 0;

    progname = argv[0];
    setlocale(LC_ALL, "");

    while ((ch = getopt(argc, argv, "af:hi:o:su:vVw")) > 0) {
	switch (ch) {
	case 'a':
	    format |= DCAT_ASCII;
	    break;
	case 'f':
	    fstype = optarg;
	    if (strcmp(fstype, DLS_TYPE) == 0)
		fstype = RAW_STR;

	    break;
	case 'h':
	    format |= DCAT_HEX;
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
	    format |= DCAT_STAT;
	    break;
	case 'u':
	    usize = strtoul(optarg, &cp, 0);
	    if (*cp || cp == optarg) {
		fprintf(stderr, "Invalid block size: %s\n", optarg);
		usage();
	    }
	    break;
	case 'v':
	    verbose++;
	    break;
	case 'V':
	    print_version(stdout);
	    exit(0);
	    break;
	case 'w':
	    format |= DCAT_HTML;
	    break;
	case '?':
	default:
	    fprintf(stderr, "Invalid argument: %s\n", argv[optind]);
	    usage();
	}
    }

    if (format & DCAT_STAT) {
	if (optind == argc)
	    usage();

	if (format & (DCAT_HTML | DCAT_ASCII | DCAT_HEX)) {
	    fprintf(stderr, "NOTE: Additional flags will be ignored\n");
	}
    }
    /* We need at least two more arguments */
    else if (optind + 1 >= argc) {
	fprintf(stderr, "Missing image name and/or address\n");
	usage();
    }

    if ((format & DCAT_ASCII) && (format & DCAT_HEX)) {
	fprintf(stderr, "Ascii and Hex flags can not be used together\n");
	usage();
    }

    /* We need to figure out if there is a length argument... */
    /* Check out the second argument from the end */

    /* default number of units is 1 */
    read_num_units = 1;

    /* Get the block address */
    if (format & DCAT_STAT) {
	if ((img =
		img_open(imgtype, argc - optind,
		    (const char **) &argv[optind])) == NULL) {
	    tsk_error_print(stderr);
	    exit(1);
	}

    }
    else {
	addr = strtoull(argv[argc - 2], &cp, 0);
	if (*cp || cp == argv[argc - 2]) {

	    /* Not a number, so it is the image name and we do not have a length */
	    addr = strtoull(argv[argc - 1], &cp, 0);
	    if (*cp || cp == argv[argc - 1]) {
		fprintf(stderr, "Invalid block address: %s\n",
		    argv[argc - 1]);
		usage();
	    }

	    if ((img =
		    img_open(imgtype, argc - optind - 1,
			(const char **) &argv[optind])) == NULL) {
		tsk_error_print(stderr);
		exit(1);
	    }

	}
	else {
	    /* We got a number, so take the length as well while we are at it */
	    read_num_units = strtoull(argv[argc - 1], &cp, 0);
	    if (*cp || cp == argv[argc - 1]) {
		fprintf(stderr, "Invalid size: %s\n", argv[argc - 1]);
		usage();
	    }
	    else if (read_num_units <= 0) {
		fprintf(stderr, "Invalid size: %" PRIuDADDR "\n",
		    read_num_units);
		usage();
	    }

	    if ((img =
		    img_open(imgtype, argc - optind - 2,
			(const char **) &argv[optind])) == NULL) {

		tsk_error_print(stderr);
		exit(1);
	    }

	}
    }

    /* open the file */
    if ((fs = fs_open(img, imgoff, fstype)) == NULL) {
	tsk_error_print(stderr);
	if (tsk_errno == TSK_ERR_FS_UNSUPTYPE)
	    fs_print_types(stderr);
	img->close(img);
	exit(1);
    }



    /* Set the default size if given */
    if ((usize != 0) &&
	(((fs->ftype & FSMASK) == RAWFS_TYPE) ||
	    ((fs->ftype & FSMASK) == SWAPFS_TYPE))) {

	DADDR_T sectors;
	int orig_dsize, new_dsize;

	if (usize % 512) {
	    fprintf(stderr,
		"New data unit size not a multiple of 512 (%d)\n", usize);
	    usage();
	}

	/* We need to do some math to update the block_count value */

	/* Get the original number of sectors */
	orig_dsize = fs->block_size / 512;
	sectors = fs->block_count * orig_dsize;

	/* Convert that to the new size */
	new_dsize = usize / 512;
	fs->block_count = sectors / new_dsize;
	if (sectors % new_dsize)
	    fs->block_count++;
	fs->last_block = fs->block_count - 1;

	fs->block_size = usize;
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

    if (fs_dcat(fs, format, addr, read_num_units)) {
	tsk_error_print(stderr);
	fs->close(fs);
	img->close(img);
	exit(1);
    }

    fs->close(fs);
    img->close(img);

    exit(0);
}
