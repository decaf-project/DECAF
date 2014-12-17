/*
 * The Sleuth Kit
 *
 * $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
 *
 * Brian Carrier [carrier@sleuthkit.org]
 * Copyright (c) 2006 Brian Carrier, Basis Technology.  All rights reserved
 * Copyright (c) 2003-2005 Brian Carrier.  All rights reserved
 *
 * SUN - Sun VTOC code
 *
 *
 * This software is distributed under the Common Public License 1.0
 *
 */

#include "mm_tools.h"
#include "sun.h"


/*
 * Return a buffer with a description of the partition type
 */
static char *
sun_get_desc(uint8_t fstype)
{
    char *str = mymalloc(64);
    if (str == NULL)
	return "";
    switch (fstype) {

    case 0:
	strncpy(str, "Unassigned (0x00)", 64);
	break;
    case 1:
	strncpy(str, "boot (0x01)", 64);
	break;
    case 2:
	strncpy(str, "/ (0x02)", 64);
	break;
    case 3:
	strncpy(str, "swap (0x03)", 64);
	break;
    case 4:
	strncpy(str, "/usr/ (0x04)", 64);
	break;
    case 5:
	strncpy(str, "backup (0x05)", 64);
	break;
    case 6:
	strncpy(str, "stand (0x06)", 64);
	break;
    case 7:
	strncpy(str, "/var/ (0x07)", 64);
	break;
    case 8:
	strncpy(str, "/home/ (0x08)", 64);
	break;
    case 9:
	strncpy(str, "alt sector (0x09)", 64);
	break;
    case 10:
	strncpy(str, "cachefs (0x0A)", 64);
	break;
    default:
	snprintf(str, 64, "Unknown Type (0x%.2x)", fstype);
	break;
    }

    return str;
}


/* 
 * Load an Intel disk label, this is called by sun_load_table 
 */

static uint8_t
sun_load_table_i386(MM_INFO * mm, sun_dlabel_i386 * dlabel_x86)
{
    uint32_t idx = 0;
    DADDR_T max_addr = (mm->img_info->size - mm->offset) / mm->block_size;	// max sector

    if (verbose)
	fprintf(stderr, "load_table_i386: Number of partitions: %d\n",
	    getu16(mm, dlabel_x86->num_parts));

    /* Cycle through the partitions, there are either 8 or 16 */
    for (idx = 0; idx < getu16(mm, dlabel_x86->num_parts); idx++) {

	if (verbose)
	    fprintf(stderr,
		"load_table_i386: %d  Starting Sector: %lu  Size: %lu  Type: %d\n",
		idx,
		(ULONG) getu32(mm, dlabel_x86->part[idx].start_sec),
		(ULONG) getu32(mm, dlabel_x86->part[idx].size_sec),
		getu16(mm, dlabel_x86->part[idx].type));

	if (getu32(mm, dlabel_x86->part[idx].size_sec) == 0)
	    continue;

	if (getu32(mm, dlabel_x86->part[idx].start_sec) > max_addr) {
	    tsk_errno = TSK_ERR_MM_BLK_NUM;
	    snprintf(tsk_errstr, TSK_ERRSTR_L,
		"sun_load_i386: Starting sector too large for image");
	    tsk_errstr2[0] = '\0';
	    return 1;
	}


	/* Add the partition to the internal sorted list */
	if (NULL == mm_part_add(mm,
		(DADDR_T) getu32(mm, dlabel_x86->part[idx].start_sec),
		(DADDR_T) getu32(mm, dlabel_x86->part[idx].size_sec),
		MM_TYPE_VOL,
		sun_get_desc(getu16(mm, dlabel_x86->part[idx].type)),
		-1, idx)) {
	    return 1;
	}
    }

    return 0;
}


/* load a sparc disk label, this is called from the general
 * sun_load_table
 */
static uint8_t
sun_load_table_sparc(MM_INFO * mm, sun_dlabel_sparc * dlabel_sp)
{
    uint32_t idx = 0;
    uint32_t cyl_conv;
    DADDR_T max_addr = (mm->img_info->size - mm->offset) / mm->block_size;	// max sector

    /* The value to convert cylinders to sectors */
    cyl_conv = getu16(mm, dlabel_sp->sec_per_tr) *
	getu16(mm, dlabel_sp->num_head);

    if (verbose)
	fprintf(stderr, "load_table_sparc: Number of partitions: %d\n",
	    getu16(mm, dlabel_sp->num_parts));

    /* Cycle through the partitions, there are either 8 or 16 */
    for (idx = 0; idx < getu16(mm, dlabel_sp->num_parts); idx++) {
	uint32_t part_start = cyl_conv *
	    getu32(mm, dlabel_sp->part_layout[idx].start_cyl);

	uint32_t part_size = getu32(mm,
	    dlabel_sp->part_layout[idx].size_blk);

	if (verbose)
	    fprintf(stderr,
		"load_table_sparc: %d  Starting Sector: %lu  Size: %lu  Type: %d\n",
		idx,
		(ULONG) part_start,
		(ULONG) part_size,
		getu16(mm, dlabel_sp->part_meta[idx].type));

	if (part_size == 0)
	    continue;

	if (part_start > max_addr) {
	    tsk_errno = TSK_ERR_MM_BLK_NUM;
	    snprintf(tsk_errstr, TSK_ERRSTR_L,
		"sun_load_sparc: Starting sector too large for image");
	    tsk_errstr2[0] = '\0';
	    return 1;
	}


	/* Add the partition to the internal sorted list */
	if (NULL == mm_part_add(mm, (DADDR_T) part_start,
		(DADDR_T) part_size, MM_TYPE_VOL, sun_get_desc(getu16(mm,
			dlabel_sp->part_meta[idx].type)), -1, idx))
	    return 1;
    }

    return 0;
}


/* 
 * Process the partition table at the sector address 
 *
 * This method just finds out if it is sparc or Intel and then
 * calls the appropriate method
 *
 * Return 0 on success and 1 on error
 */
static uint8_t
sun_load_table(MM_INFO * mm)
{
/* this will need to change if any of the disk label structures change */
#define LABEL_BUF_SIZE	512

    sun_dlabel_sparc *dlabel_sp;
    sun_dlabel_i386 *dlabel_x86;
    char buf[LABEL_BUF_SIZE];
    SSIZE_T cnt;
    DADDR_T taddr = mm->offset / mm->block_size + SUN_SPARC_PART_SOFFSET;


    /* Sanity check in case label sizes change */
    if ((sizeof(*dlabel_sp) > LABEL_BUF_SIZE) ||
	(sizeof(*dlabel_x86) > LABEL_BUF_SIZE)) {
	tsk_errno = TSK_ERR_MM_BUF;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "sun_load_table: Buffer smaller than label sizes");
	tsk_errstr2[0] = '\0';
	return 1;
    }

    if (verbose)
	fprintf(stderr, "sun_load_table: Trying sector: %" PRIuDADDR "\n",
	    taddr);

    /* Try the given offset */
    cnt = mm_read_block_nobuf
	(mm, (char *) &buf, LABEL_BUF_SIZE, SUN_SPARC_PART_SOFFSET);

    /* If -1 is returned, tsk_errno is already set */
    if (cnt != LABEL_BUF_SIZE) {
	if (cnt != -1) {
	    tsk_errno = TSK_ERR_MM_READ;
	    tsk_errstr[0] = '\0';
	}
	snprintf(tsk_errstr2, TSK_ERRSTR_L,
	    "SUN Disk Label in Sector: %" PRIuDADDR, taddr);
	return 1;
    }


    /* Check the magic value 
     * Both intel and sparc have the magic value in the same location
     *
     * We try both in case someone specifies the exact location of the
     * intel disk label.
     * */
    dlabel_sp = (sun_dlabel_sparc *) buf;
    dlabel_x86 = (sun_dlabel_i386 *) buf;
    if (mm_guessu16(mm, dlabel_sp->magic, SUN_MAGIC) == 0) {
	if (getu32(mm, dlabel_sp->sanity) == SUN_SANITY) {
	    return sun_load_table_sparc(mm, dlabel_sp);
	}
	else if (getu32(mm, dlabel_x86->sanity) == SUN_SANITY) {
	    return sun_load_table_i386(mm, dlabel_x86);
	}
    }


    /* Now try the next sector, which is where the intel 
     * could be stored */

    taddr = mm->offset / mm->block_size / SUN_I386_PART_SOFFSET;
    if (verbose)
	fprintf(stderr, "sun_load_table: Trying sector: %" PRIuDADDR "\n",
	    taddr + 1);

    cnt = mm_read_block_nobuf
	(mm, (char *) &buf, LABEL_BUF_SIZE, SUN_I386_PART_SOFFSET);

    if (cnt != LABEL_BUF_SIZE) {
	if (cnt != -1) {
	    tsk_errno = TSK_ERR_MM_READ;
	    tsk_errstr[0] = '\0';
	}
	snprintf(tsk_errstr2, TSK_ERRSTR_L,
	    "SUN (Intel) Disk Label in Sector: %" PRIuDADDR, taddr);
	return 1;
    }

    dlabel_x86 = (sun_dlabel_i386 *) buf;
    if (mm_guessu16(mm, dlabel_x86->magic, SUN_MAGIC)) {
	tsk_errno = TSK_ERR_MM_MAGIC;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "SUN (intel) partition table (Sector: %"
	    PRIuDADDR ") %x", taddr, getu16(mm, dlabel_sp->magic));
	tsk_errstr2[0] = '\0';
	return 1;
    }

    if (getu32(mm, dlabel_x86->sanity) != SUN_SANITY) {
	tsk_errno = TSK_ERR_MM_MAGIC;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "SUN (intel) sanity value (Sector: %" PRIuDADDR
	    ") %x", taddr, getu16(mm, dlabel_sp->magic));
	tsk_errstr2[0] = '\0';
	return 1;
    }

    return sun_load_table_i386(mm, dlabel_x86);
}


/* 
 * Walk the partitions that have already been loaded during _open
 *
 * Return 1 on error and 0 on success
 */
uint8_t
sun_part_walk(MM_INFO * mm, PNUM_T start, PNUM_T last, int flags,
    MM_PART_WALK_FN action, char *ptr)
{
    MM_PART *part;
    unsigned int cnt = 0;

    if (start < mm->first_part || start > mm->last_part) {
	tsk_errno = TSK_ERR_MM_WALK_RNG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "sun_part_walk: Starting partition: %d", start);
	tsk_errstr2[0] = '\0';
	return 1;
    }

    if (last < mm->first_part || last > mm->last_part) {
	tsk_errno = TSK_ERR_MM_WALK_RNG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "sun_part_walk: Ending partition: %d", last);
	tsk_errstr2[0] = '\0';
	return 1;
    }

    part = mm->part_list;
    while ((part != NULL) && (cnt <= last)) {

	if (cnt >= start) {
	    int retval = action(mm, cnt, part, 0, ptr);
	    if (retval == WALK_STOP)
		return 0;
	    else if (retval == WALK_ERROR)
		return 1;
	}

	part = part->next;
	cnt++;
    }

    return 0;
}


void
sun_close(MM_INFO * mm)
{
    mm_part_free(mm);
    free(mm);
}

MM_INFO *
sun_open(IMG_INFO * img_info, DADDR_T offset)
{
    MM_INFO *mm = (MM_INFO *) mymalloc(sizeof(*mm));
    if (mm == NULL)
	return NULL;

    mm->img_info = img_info;
    mm->mmtype = MM_SUN;
    mm->str_type = "Sun VTOC";

    mm->offset = offset;

    /* inititialize settings */
    mm->part_list = NULL;
    mm->first_part = mm->last_part = 0;
    mm->endian = 0;
    mm->dev_bsize = 512;
    mm->block_size = 512;

    /* Assign functions */
    mm->part_walk = sun_part_walk;
    mm->close = sun_close;

    /* Load the partitions into the sorted list */
    if (sun_load_table(mm)) {
	sun_close(mm);
	return NULL;
    }

    /* fill in the sorted list with the 'unknown' values */
    if (mm_part_unused(mm)) {
	sun_close(mm);
	return NULL;
    }

    return mm;
}
