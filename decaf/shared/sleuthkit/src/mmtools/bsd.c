/*
 * The Sleuth Kit
 *
 * $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
 *
 * Brian Carrier [carrier@sleuthkit.org]
 * Copyright (c) 2006 Brian Carrier, Basis Technology.  All rights reserved
 * Copyright (c) 2003-2005 Brian Carrier.  All rights reserved
 *
 * bsd.c: BSD Disk labels
 *
 *
 * This software is distributed under the Common Public License 1.0
 *
 */

#include "mm_tools.h"
#include "bsd.h"


/*
 * Return a buffer with a description of the partition type
 */
static char *
bsd_get_desc(uint8_t fstype)
{
    char *str = mymalloc(64);
    if (str == NULL)
	return "";

    switch (fstype) {

    case 0:
	strncpy(str, "Unused (0x00)", 64);
	break;
    case 1:
	strncpy(str, "Swap (0x01)", 64);
	break;
    case 2:
	strncpy(str, "Version 6 (0x02)", 64);
	break;
    case 3:
	strncpy(str, "Version 7 (0x03)", 64);
	break;
    case 4:
	strncpy(str, "System V (0x04)", 64);
	break;
    case 5:
	strncpy(str, "4.1BSD (0x05)", 64);
	break;
    case 6:
	strncpy(str, "Eighth Edition (0x06)", 64);
	break;
    case 7:
	strncpy(str, "4.2BSD (0x07)", 64);
	break;
    case 8:
	strncpy(str, "MSDOS (0x08)", 64);
	break;
    case 9:
	strncpy(str, "4.4LFS (0x09)", 64);
	break;
    case 10:
	strncpy(str, "Unknown (0x0A)", 64);
	break;
    case 11:
	strncpy(str, "HPFS (0x0B)", 64);
	break;
    case 12:
	strncpy(str, "ISO9660 (0x0C)", 64);
	break;
    case 13:
	strncpy(str, "Boot (0x0D)", 64);
	break;
    case 14:
	strncpy(str, "Vinum (0x0E)", 64);
	break;
    default:
	snprintf(str, 64, "Unknown Type (0x%.2x)", fstype);
	break;
    }

    return str;
}

/* 
 * Process the partition table at the sector address 
 *
 * Return 1 on error and 0 if no error
 */
static uint8_t
bsd_load_table(MM_INFO * mm)
{

    bsd_disklabel dlabel;
    uint32_t idx = 0;
    SSIZE_T cnt;
    DADDR_T laddr = mm->offset / mm->block_size + BSD_PART_SOFFSET;	// used for printing only
    DADDR_T max_addr = (mm->img_info->size - mm->offset) / mm->block_size;	// max sector

    if (verbose)
	fprintf(stderr, "bsd_load_table: Table Sector: %" PRIuDADDR "\n",
	    laddr);

    /* read the block */
    cnt = mm_read_block_nobuf
	(mm, (char *) &dlabel, sizeof(dlabel), BSD_PART_SOFFSET);
    if (cnt != sizeof(dlabel)) {
	snprintf(tsk_errstr2, TSK_ERRSTR_L,
	    "BSD Disk Label in Sector: %" PRIuDADDR, laddr);
	if (cnt != -1) {
	    tsk_errno = TSK_ERR_MM_READ;
	    tsk_errstr[0] = '\0';
	}
	return 1;
    }

    /* Check the magic  */
    if (mm_guessu32(mm, dlabel.magic, BSD_MAGIC)) {
	tsk_errno = TSK_ERR_MM_MAGIC;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "BSD partition table (magic #1) (Sector: %"
	    PRIuDADDR ") %" PRIx32, laddr, getu32(mm, dlabel.magic));
	tsk_errstr2[0] = '\0';
	return 1;
    }

    /* Check the second magic value */
    if (getu32(mm, dlabel.magic2) != BSD_MAGIC) {
	tsk_errno = TSK_ERR_MM_MAGIC;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "BSD disk label (magic #2) (Sector: %"
	    PRIuDADDR ")  %" PRIx32, laddr, getu32(mm, dlabel.magic2));
	tsk_errstr2[0] = '\0';
	return 1;
    }

    /* Cycle through the partitions, there are either 8 or 16 */
    for (idx = 0; idx < getu16(mm, dlabel.num_parts); idx++) {

	uint32_t part_start;
	uint32_t part_size;

	part_start = getu32(mm, dlabel.part[idx].start_sec);
	part_size = getu32(mm, dlabel.part[idx].size_sec);

	if (verbose)
	    fprintf(stderr,
		"load_table: %d  Starting Sector: %" PRIu32
		"  Size: %" PRIu32 "  Type: %d\n", idx, part_start,
		part_size, dlabel.part[idx].fstype);

	if (part_size == 0)
	    continue;

	if (part_start > max_addr) {
	    tsk_errno = TSK_ERR_MM_BLK_NUM;
	    snprintf(tsk_errstr, TSK_ERRSTR_L,
		"bsd_load_table: Starting sector too large for image");
	    tsk_errstr2[0] = '\0';
	    return 1;
	}


	/* Add the partition to the internal sorted list */
	if (NULL == mm_part_add(mm, (DADDR_T) part_start,
		(DADDR_T) part_size, MM_TYPE_VOL,
		bsd_get_desc(dlabel.part[idx].fstype), -1, idx)) {
	    return 1;
	}
    }

    return 0;
}


/* 
 * Walk the partitions that have already been loaded during _open
 *
 * Return 1 on error and 0 on success
 */
uint8_t
bsd_part_walk(MM_INFO * mm, PNUM_T start, PNUM_T last, int flags,
    MM_PART_WALK_FN action, char *ptr)
{
    MM_PART *part;
    PNUM_T cnt = 0;

    if (start < mm->first_part || start > mm->last_part) {
	tsk_errno = TSK_ERR_MM_WALK_RNG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "bsd_part_walk: Start partition: %" PRIuPNUM "", start);
	tsk_errstr2[0] = '\0';
	return 1;
    }

    if (last < mm->first_part || last > mm->last_part) {
	tsk_errno = TSK_ERR_MM_WALK_RNG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "bsd_part_walk: End partition: %" PRIuPNUM "", last);
	tsk_errstr2[0] = '\0';
	return 1;
    }

    part = mm->part_list;
    while ((part != NULL) && (cnt <= last)) {

	if (cnt >= start) {
	    int retval;
	    retval = action(mm, cnt, part, 0, ptr);
	    if (retval == WALK_STOP) {
		return 0;
	    }
	    else if (retval == WALK_ERROR) {
		return 1;
	    }
	}

	part = part->next;
	cnt++;
    }

    return 0;
}


void
bsd_close(MM_INFO * mm)
{
    mm_part_free(mm);
    free(mm);
}

/*
 * analyze the image in img_info and process it as BSD
 * Initialize the MM_INFO structure
 *
 * Return MM_INFO or NULL if not BSD or an error 
 */
MM_INFO *
bsd_open(IMG_INFO * img_info, DADDR_T offset)
{
    MM_INFO *mm = (MM_INFO *) mymalloc(sizeof(*mm));
    if (mm == NULL)
	return NULL;

    mm->img_info = img_info;
    mm->mmtype = MM_BSD;
    mm->str_type = "BSD Disk Label";

    /* use the offset provided */
    mm->offset = offset;

    /* inititialize settings */
    mm->part_list = NULL;
    mm->first_part = mm->last_part = 0;
    mm->endian = 0;
    mm->dev_bsize = 512;
    mm->block_size = 512;

    /* Assign functions */
    mm->part_walk = bsd_part_walk;
    mm->close = bsd_close;

    /* Load the partitions into the sorted list */
    if (bsd_load_table(mm)) {
	bsd_close(mm);
	return NULL;
    }

    /* fill in the sorted list with the 'unknown' values */
    if (mm_part_unused(mm)) {
	bsd_close(mm);
	return NULL;
    }

    return mm;
}
