/*
** The Sleuth Kit
**
** $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
**
** Brian Carrier [carrier@sleuthkit.org]
** Copyright (c) 2006 Brian Carrier, Basis Technology.  All Rights reserved
** Copyright (c) 2003-2005 Brian Carrier.  All rights reserved 
**
** TASK
** Copyright (c) 2002 Brian Carrier, @stake Inc.  All rights reserved
**
** Copyright (c) 1997,1998,1999, International Business Machines
** Corporation and others. All Rights Reserved.
**
*/

/* TCT:
 *
 * LICENSE
 *	This software is distributed under the IBM Public License.
 * AUTHOR(S)
 *	Wietse Venema
 *	IBM T.J. Watson Research
 *	P.O. Box 704
 *	Yorktown Heights, NY 10598, USA
 */

#include "libfstools.h"


/* usage - explain and terminate */

static void
usage()
{
    fprintf(stderr,
	"usage: %s [-belvV] [-f fstype] [-i imgtype] [-o imgoffset] image [images] [start-stop]\n",
	progname);
    fprintf(stderr, "\t-b: no block padding\n");
    fprintf(stderr, "\t-e: every block\n");
    fprintf(stderr, "\t-l: print details in time machine list format\n");
    fprintf(stderr, "\t-i imgtype: The format of the image file\n");
    fprintf(stderr,
	"\t-o imgoffset: The offset of the file system in the image (in sectors)\n");
    fprintf(stderr,
	"\t-s: print slack space only (other flags are ignored\n");
    fprintf(stderr, "\t-v: verbose to stderr\n");
    fprintf(stderr, "\t-V: print version\n");
    fprintf(stderr, "\t-f fstype: File system type\n");
    fs_print_types(stderr);
    fprintf(stderr, "Supported image format types:\n");
    img_print_types(stderr);

    exit(1);
}






/* main - open file system, list block info */

int
main(int argc, char **argv)
{
    FS_INFO *fs;
    DADDR_T bstart = 0, blast = 0;
    int ch;
    int flags =
	FS_FLAG_DATA_UNALLOC | FS_FLAG_DATA_ALIGN | FS_FLAG_DATA_META |
	FS_FLAG_DATA_CONT;
    char *fstype = NULL;
    char lclflags = DLS_CAT, set_bounds = 1;
    char *imgtype = NULL, *cp, *dash;
    SSIZE_T imgoff = 0;
    IMG_INFO *img;

    progname = argv[0];
    setlocale(LC_ALL, "");

    while ((ch = getopt(argc, argv, "bef:i:lo:svV")) > 0) {
	switch (ch) {
	case '?':
	default:
	    fprintf(stderr, "Invalid argument: %s\n", argv[optind]);
	    usage();
	case 'b':
	    flags &= ~FS_FLAG_DATA_ALIGN;
	    break;
	case 'e':
	    flags |= FS_FLAG_DATA_ALLOC;
	    break;
	case 'f':
	    fstype = optarg;
	    break;
	case 'i':
	    imgtype = optarg;
	    break;
	case 'l':
	    lclflags = DLS_LIST;
	    break;
	case 'o':
	    if ((imgoff = parse_offset(optarg)) == -1) {
		tsk_error_print(stderr);
		exit(1);
	    }
	    break;

	case 's':
	    lclflags |= DLS_SLACK;
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

    /* Slack has only the image name */
    if (lclflags & DLS_SLACK) {

	if (lclflags & DLS_LIST) {
	    fprintf(stderr,
		"Other options igroned with the slack space flag, try again\n");
	    exit(1);
	}

	/* There should be no other arguments */
	img =
	    img_open(imgtype, argc - optind,
	    (const char **) &argv[optind]);

	if (img == NULL) {
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
    else {


	/* We need to determine if the block range was given */
	if ((dash = strchr(argv[argc - 1], '-')) == NULL) {
	    /* No dash in arg - therefore it is an image file name */
	    if ((img =
		    img_open(imgtype, argc - optind,
			(const char **) &argv[optind])) == NULL) {

		tsk_error_print(stderr);
		exit(1);
	    }

	    set_bounds = 1;

	}
	else {
	    /* We have a dash, but it could be part of the file name */
	    *dash = '\0';

	    bstart = strtoull(argv[argc - 1], &cp, 0);
	    if (*cp || cp == argv[argc - 1]) {
		/* Not a number - consider it a file name */
		*dash = '-';
		if ((img =
			img_open(imgtype, argc - optind,
			    (const char **) &argv[optind])) == NULL) {
		    tsk_error_print(stderr);
		    exit(1);
		}

		set_bounds = 1;
	    }
	    else {
		/* Check after the dash */
		dash++;
		blast = strtoull(dash, &cp, 0);
		if (*cp || cp == dash) {
		    /* Not a number - consider it a file name */
		    dash--;
		    *dash = '-';
		    if ((img =
			    img_open(imgtype, argc - optind,
				(const char **) &argv[optind])) == NULL) {
			tsk_error_print(stderr);
			exit(1);
		    }

		    set_bounds = 1;
		}
		else {

		    set_bounds = 0;
		    /* It was a block range, so do not include it in the open */
		    if ((img =
			    img_open(imgtype, argc - optind - 1,
				(const char **) &argv[optind])) == NULL) {
			tsk_error_print(stderr);
			exit(1);
		    }

		}
	    }
	}

	if ((fs = fs_open(img, imgoff, fstype)) == NULL) {
	    tsk_error_print(stderr);
	    if (tsk_errno == TSK_ERR_FS_UNSUPTYPE)
		fs_print_types(stderr);
	    img->close(img);
	    exit(1);
	}


	/* do we need to set the range or just check them? */
	if (set_bounds) {
	    bstart = fs->first_block;
	    blast = fs->last_block;
	}
	else {
	    if (bstart < fs->first_block)
		bstart = fs->first_block;

	    if (blast > fs->last_block)
		blast = fs->last_block;
	}
    }

    if (fs_dls(fs, lclflags, bstart, blast, flags)) {
	tsk_error_print(stderr);
	fs->close(fs);
	img->close(img);
	exit(1);

    }

    fs->close(fs);
    img->close(img);
    exit(0);
}
