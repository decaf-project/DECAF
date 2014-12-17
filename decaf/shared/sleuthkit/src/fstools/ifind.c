/*
** ifind (inode find)
** The Sleuth Kit
**
** $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
**
** Given an image  and block number, identify which inode it is used by
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

static uint8_t localflags;

static void
usage()
{
    fprintf(stderr,
	"usage: %s [-alvV] [-f fstype] [-i imgtype] [-o imgoffset] [-d unit_addr] [-n file] [-p par_addr] [-z ZONE] image [images]\n",
	progname);
    fprintf(stderr, "\t-a: find all inodes\n");
    fprintf(stderr,
	"\t-d unit_addr: Find the meta data given the data unit\n");
    fprintf(stderr, "\t-l: long format when -p is given\n");
    fprintf(stderr, "\t-n file: Find the meta data given the file name\n");
    fprintf(stderr,
	"\t-p par_addr: Find UNALLOCATED MFT entries given the parent's meta address (NTFS only)\n");
    fprintf(stderr, "\t-i imgtype: The format of the image file\n");
    fprintf(stderr,
	"\t-o imgoffset: The offset of the file system in the image (in sectors)\n");
    fprintf(stderr, "\t-v: Verbose output to stderr\n");
    fprintf(stderr, "\t-V: Print version\n");
    fprintf(stderr, "\t-z ZONE: Time zone setting when -l -p is given\n");
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

    FS_INFO *fs;
    int ch;
    char *cp;
    extern int optind;
    char *imgtype = NULL;
    IMG_INFO *img;
    DADDR_T block = 0;		/* the block to find */
    INUM_T parinode = 0;
    char *path = NULL;
    SSIZE_T imgoff = 0;

    progname = argv[0];
    setlocale(LC_ALL, "");

    localflags = 0;

    while ((ch = getopt(argc, argv, "ad:f:i:ln:o:p:vVz:")) > 0) {
	switch (ch) {
	case 'a':
	    localflags |= IFIND_ALL;

	    break;
	case 'd':
	    if (localflags & (IFIND_PAR | IFIND_PATH)) {
		fprintf(stderr,
		    "error: only one address type can be given\n");
		usage();
	    }
	    localflags |= IFIND_DATA;
	    block = strtoull(optarg, &cp, 0);
	    if (*cp || cp == optarg) {
		fprintf(stderr, "Invalid block address: %s\n", optarg);
		usage();
	    }
	    break;

	case 'f':
	    fstype = optarg;
	    break;
	case 'i':
	    imgtype = optarg;
	    break;

	case 'l':
	    localflags |= IFIND_PAR_LONG;
	    break;

	case 'n':
	    if (localflags & (IFIND_PAR | IFIND_DATA)) {
		fprintf(stderr,
		    "error: only one address type can be given\n");
		usage();
	    }
	    localflags |= IFIND_PATH;
	    if ((path = mymalloc(strlen(optarg) + 1)) == NULL) {
		tsk_error_print(stderr);
		exit(1);
	    }
	    strncpy(path, optarg, strlen(optarg) + 1);
	    break;
	case 'o':
	    if ((imgoff = parse_offset(optarg)) == -1) {
		tsk_error_print(stderr);
		exit(1);
	    }
	    break;

	case 'p':
	    if (localflags & (IFIND_PATH | IFIND_DATA)) {
		fprintf(stderr,
		    "error: only one address type can be given\n");
		usage();
	    }
	    localflags |= IFIND_PAR;
	    parinode = strtoull(optarg, &cp, 0);
	    if (*cp || cp == optarg) {
		fprintf(stderr, "Invalid block address: %s\n", optarg);
		usage();
	    }
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

		/* we should be checking this somehow */
		tzset();
		break;
	    }
	case '?':
	default:
	    fprintf(stderr, "Invalid argument: %s\n", argv[optind]);
	    usage();
	}
    }

    /* We need at least one more argument */
    if (optind >= argc) {
	fprintf(stderr, "Missing image name\n");
	usage();
    }

    if (0 == (localflags & (IFIND_PATH | IFIND_DATA | IFIND_PAR))) {
	fprintf(stderr, "-d, -n, or -p must be given\n");
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

    if (localflags & IFIND_DATA) {

	if (block > fs->last_block) {
	    fprintf(stderr,
		"Block %" PRIuDADDR
		" is larger than last block in image (%" PRIuDADDR
		")\n", block, fs->last_block);
	    fs->close(fs);
	    img->close(img);
	    exit(1);
	}
	else if (block == 0) {
	    printf("Inode not found\n");
	    fs->close(fs);
	    img->close(img);
	    exit(1);
	}
	if (fs_ifind_data(fs, localflags, block)) {
	    tsk_error_print(stderr);
	    fs->close(fs);
	    img->close(img);
	    exit(1);
	}
    }

    else if (localflags & IFIND_PAR) {
	if ((fs->ftype & FSMASK) != NTFS_TYPE) {
	    fprintf(stderr, "-p works only with NTFS file systems\n");
	    fs->close(fs);
	    img->close(img);
	    exit(1);
	}
	else if (parinode > fs->last_inum) {
	    fprintf(stderr,
		"Meta data %" PRIuINUM
		" is larger than last MFT entry in image (%" PRIuINUM
		")\n", parinode, fs->last_inum);
	    fs->close(fs);
	    img->close(img);
	    exit(1);
	}
	if (fs_ifind_par(fs, localflags, parinode)) {
	    tsk_error_print(stderr);
	    fs->close(fs);
	    img->close(img);
	    exit(1);

	}
    }

    else if (localflags & IFIND_PATH) {
	if (fs_ifind_path(fs, localflags, path)) {
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
