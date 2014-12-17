/*
 * The Sleuth Kit
 *
 * $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
 *
 * Brian Carrier [carrier@sleuthkit.org]
 * Copyright (c) 2006 Brian Carrier, Basis Technology.  All rights reserved
 * Copyright (c) 2003-2005 Brian Carrier.  All rights reserved
 *
 * mmls - list media management structure contents
 *
 *
 * This software is distributed under the Common Public License 1.0
 *
 */

#include "mm_tools.h"


extern char *progname;

static uint8_t print_bytes = 0;
static uint8_t recurse = 0;

static int recurse_cnt = 0;
static DADDR_T recurse_list[64];

void
usage()
{
    fprintf(stderr,
	"%s [-i imgtype] [-o imgoffset] [-brvV] [-t mmtype] image [images]\n",
	progname);
    fprintf(stderr, "\t-t mmtype: The type of partition system\n");
    fprintf(stderr, "\t-i imgtype: The format of the image file\n");
    fprintf(stderr,
	"\t-o imgoffset: Offset to the start of the volume that contains the partition system (in sectors)\n");
    fprintf(stderr, "\t-b: print the rounded length in bytes\n");
    fprintf(stderr,
	"\t-r: recurse and look for other partition tables in partitions (DOS Only)\n");
    fprintf(stderr, "\t-v: verbose output\n");
    fprintf(stderr, "\t-V: print the version\n");
    mm_print_types(stderr);
    fprintf(stderr, "Supported image formats:\n");
    img_print_types(stderr);
    exit(1);
}

/*
 * The callback action for the part_walk
 *
 * Prints the layout information
 * */
uint8_t
part_act(MM_INFO * mm, PNUM_T pnum, MM_PART * part, int flag, char *ptr)
{
    /* Neither table or slot were given */
    if ((part->table_num == -1) && (part->slot_num == -1))
	printf("%.2" PRIuPNUM ":  -----   ", pnum);

    /* Table was not given, but slot was */
    else if ((part->table_num == -1) && (part->slot_num != -1))
	printf("%.2" PRIuPNUM ":  %.2" PRIuPNUM "      ",
	    pnum, part->slot_num);

    /* The Table was given, but slot wasn't */
    else if ((part->table_num != -1) && (part->slot_num == -1))
	printf("%.2" PRIuPNUM ":  -----   ", pnum);

    /* Both table and slot were given */
    else if ((part->table_num != -1) && (part->slot_num != -1))
	printf("%.2" PRIuPNUM ":  %.2d:%.2d   ",
	    pnum, part->table_num, part->slot_num);

    if (print_bytes) {
	OFF_T size;
	char unit = ' ';
	size = part->len;

	if (part->len < 2) {
	    size = 512 * part->len;
	    unit = 'B';
	}
	else if (size < (2 << 10)) {
	    size = part->len / 2;
	    unit = 'K';
	}
	else if (size < (2 << 20)) {
	    size = part->len / (2 << 10);
	    unit = 'M';
	}
	else if (size < ((OFF_T) 2 << 30)) {
	    size = part->len / (2 << 20);
	    unit = 'G';
	}
	else if (size < ((OFF_T) 2 << 40)) {
	    size = part->len / (2 << 30);
	    unit = 'T';
	}

	/* Print the layout */
	printf("%.10" PRIuDADDR "   %.10" PRIuDADDR "   %.10" PRIuDADDR
	    "   %.4" PRIuOFF "%c   %s\n", part->start,
	    (DADDR_T) (part->start + part->len - 1), part->len, size, unit,
	    part->desc);
    }
    else {

	/* Print the layout */
	printf("%.10" PRIuDADDR "   %.10" PRIuDADDR "   %.10" PRIuDADDR
	    "   %s\n", part->start,
	    (DADDR_T) (part->start + part->len - 1), part->len,
	    part->desc);
    }

    if ((recurse) && (mm->mmtype == MM_DOS) && (part->type == MM_TYPE_VOL)) {
	// @@@ This assumes 512-byte sectors
	if (recurse_cnt < 64)
	    recurse_list[recurse_cnt++] = part->start * 512;
    }

    return WALK_CONT;
}

static void
print_header(MM_INFO * mm)
{
    printf("%s\n", mm->str_type);
    printf("Offset Sector: %" PRIuDADDR "\n",
	(DADDR_T) (mm->offset / mm->block_size));
    printf("Units are in %d-byte sectors\n\n", mm->block_size);
    if (print_bytes)
	printf
	    ("     Slot    Start        End          Length       Size    Description\n");
    else
	printf
	    ("     Slot    Start        End          Length       Description\n");
}


int
main(int argc, char **argv)
{
    MM_INFO *mm;
    char *mmtype = NULL;
    int ch;
    SSIZE_T imgoff = 0;
    uint8_t flags = 0;
    char *imgtype = NULL;
    IMG_INFO *img;

    progname = argv[0];

    while ((ch = getopt(argc, argv, "bi:o:rt:vV")) > 0) {
	switch (ch) {
	case 'b':
	    print_bytes = 1;
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
	case 'r':
	    recurse = 1;
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
    img = img_open(imgtype, argc - optind, (const char **) &argv[optind]);

    if (img == NULL) {
	tsk_error_print(stderr);
	exit(1);
    }

    /* process the partition tables */
    mm = mm_open(img, (OFF_T) imgoff, mmtype);
    if (mm == NULL) {
	tsk_error_print(stderr);
	if (tsk_errno == TSK_ERR_MM_UNSUPTYPE)
	    mm_print_types(stderr);
	exit(1);
    }

    print_header(mm);

    if (mm->part_walk(mm, mm->first_part, mm->last_part, flags,
	    part_act, NULL)) {
	tsk_error_print(stderr);
	mm->close(mm);
	exit(1);
    }

    mm->close(mm);
    if ((recurse) && (mm->mmtype == MM_DOS)) {
	int i;
	/* disable recursing incase we hit another DOS partition
	 * future versions may support more layers */
	recurse = 0;

	for (i = 0; i < recurse_cnt; i++) {
	    mm = mm_open(img, recurse_list[i], NULL);
	    if (mm != NULL) {
		printf("\n\n");
		print_header(mm);
		if (mm->part_walk(mm, mm->first_part, mm->last_part, flags,
			part_act, NULL)) {
		    tsk_errno = 0;
		}
		mm->close(mm);
	    }
	    else {
		/* Ignore error in this case and reset */
		tsk_errno = 0;
	    }
	}
    }

    img->close(img);
    exit(0);
}
