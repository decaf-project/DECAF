/*
** fls
** The Sleuth Kit 
**
** $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
**
** Given an image and directory inode, display the file names and 
** directories that exist (both active and deleted)
**
** Brian Carrier [carrier@sleuthkit.org]
** Copyright (c) 2006 Brian Carrier, Basis Technology.  All Rights reserved
** Copyright (c) 2003-2005 Brian Carier.  All rights reserved
**
** TASK
** Copyright (c) 2002 @stake Inc.  All rights reserved
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
	"usage: %s [-adDFlpruvV] [-f fstype] [-i imgtype] [-m dir/] [-o imgoffset] [-z ZONE] [-s seconds] image [images] [inode]\n",
	progname);
    fprintf(stderr,
	"\tIf [inode] is not given, the root directory is used\n");
    fprintf(stderr, "\t-a: Display \".\" and \"..\" entries\n");
    fprintf(stderr, "\t-d: Display deleted entries only\n");
    fprintf(stderr, "\t-D: Display directory entries only\n");
    fprintf(stderr,
	"\t-F: Display file entries only (NOTE: This was -f in TCTUTILs)\n");
    fprintf(stderr, "\t-l: Display long version (like ls -l)\n");
    fprintf(stderr, "\t-i imgtype: Format of image file\n");
    fprintf(stderr, "\t-m: Display output in mactime input format with\n");
    fprintf(stderr,
	"\t      dir/ as the actual mount point of the image\n");
    fprintf(stderr,
	"\t-o imgoffset: Offset into image file (in sectors)\n");
    fprintf(stderr, "\t-p: Display full path for each file\n");
    fprintf(stderr, "\t-r: Recurse on directory entries\n");
    fprintf(stderr, "\t-u: Display undeleted entries only\n");
    fprintf(stderr, "\t-v: verbose output to stderr\n");
    fprintf(stderr, "\t-V: Print version\n");
    fprintf(stderr,
	"\t-z: Time zone of original machine (i.e. EST5EDT or GMT) (only useful with -l)\n");
    fprintf(stderr,
	"\t-s seconds: Time skew of original machine (in seconds) (only useful with -l & -m)\n");
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
    INUM_T inode;
    int flags = FS_FLAG_NAME_ALLOC | FS_FLAG_NAME_UNALLOC;
    int ch;
    FS_INFO *fs;
    extern int optind;
    IMG_INFO *img;
    char *imgtype = NULL, *cp;
    int lclflags;
    int32_t sec_skew = 0;
    static char *macpre = NULL;
    SSIZE_T imgoff = 0;

    progname = argv[0];
    setlocale(LC_ALL, "");

    lclflags = FLS_DIR | FLS_FILE;

    while ((ch = getopt(argc, argv, "adDf:Fi:m:lo:prs:uvVz:")) > 0) {
	switch (ch) {
	case '?':
	default:
	    fprintf(stderr, "Invalid argument: %s\n", argv[optind]);
	    usage();
	case 'a':
	    lclflags |= FLS_DOT;
	    break;
	case 'd':
	    flags &= ~FS_FLAG_NAME_ALLOC;
	    break;
	case 'D':
	    lclflags &= ~FLS_FILE;
	    lclflags |= FLS_DIR;
	    break;
	case 'f':
	    fstype = optarg;
	    break;
	case 'F':
	    lclflags &= ~FLS_DIR;
	    lclflags |= FLS_FILE;
	    break;
	case 'i':
	    imgtype = optarg;
	    break;
	case 'l':
	    lclflags |= FLS_LONG;
	    break;
	case 'm':
	    lclflags |= FLS_MAC;
	    macpre = optarg;
	    break;
	case 'o':
	    if ((imgoff = parse_offset(optarg)) == -1) {
		tsk_error_print(stderr);
		exit(1);
	    }
	    break;
	case 'p':
	    lclflags |= FLS_FULL;
	    break;
	case 'r':
	    flags |= FS_FLAG_NAME_RECURSE;
	    break;
	case 's':
	    sec_skew = atoi(optarg);
	    break;
	case 'u':
	    flags &= ~FS_FLAG_NAME_UNALLOC;
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
	    }
	    break;

	}
    }

    /* We need at least one more argument */
    if (optind == argc) {
	fprintf(stderr, "Missing image name\n");
	usage();
    }


    /* Set the full flag to print the full path name if recursion is
     ** set and we are only displaying files or deleted files
     */
    if ((flags & FS_FLAG_NAME_RECURSE) && (((flags & FS_FLAG_NAME_UNALLOC)
		&& (!(flags & FS_FLAG_NAME_ALLOC)))
	    || ((lclflags & FLS_FILE)
		&& (!(lclflags & FLS_DIR))))) {

	lclflags |= FLS_FULL;
    }

    /* set flag to save full path for mactimes style printing */
    if (lclflags & FLS_MAC) {
	lclflags |= FLS_FULL;
    }

    /* we need to append a / to the end of the directory if
     * one does not already exist
     */
    if (macpre) {
	int len = strlen(macpre);
	if (macpre[len - 1] != '/') {
	    char *tmp = macpre;
	    macpre = (char *) malloc(len + 2);
	    strncpy(macpre, tmp, len + 1);
	    strncat(macpre, "/", len + 2);
	}
    }

    /* open image - there is an optional inode address at the end of args 
     *
     * Check the final argument and see if it is a number
     */
    inode = strtoull(argv[argc - 1], &cp, 0);
    if (*cp || cp == argv[argc - 1]) {
	/* Not an inode at the end */
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
	inode = fs->root_inum;
    }
    else {
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
    }

    if (fs_fls(fs, lclflags, inode, flags, macpre, sec_skew)) {
	tsk_error_print(stderr);
	fs->close(fs);
	img->close(img);
	exit(1);
    }

    fs->close(fs);
    img->close(img);

    exit(0);
}
