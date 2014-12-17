/*
 * The Sleuth Kit
 *
 * $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
 *
 * Brian Carrier [carrier@sleuthkit.org]
 * Copyright (c) 2006 Brian Carrier, Basis Technology.  All rights reserved
 * Copyright (c) 2003-2005 Brian Carrier.  All rights reserved
 *
 * mac: Mac partition structures
 *
 *
 * This software is distributed under the Common Public License 1.0
 *
 */

#include "mm_tools.h"
#include "mac.h"


/* 
 * Process the partition table at the sector address 
 * 
 * It is loaded into the internal sorted list 
 *
 * Return 1 on error and 0 on success
 */
static uint8_t
mac_load_table(MM_INFO * mm)
{
    mac_part part;
    char *table_str;
    uint32_t idx, max_part;
    DADDR_T taddr = mm->offset / mm->block_size + MAC_PART_SOFFSET;
    DADDR_T max_addr = (mm->img_info->size - mm->offset) / mm->block_size;	// max sector

    if (verbose)
	fprintf(stderr, "mac_load_table: Sector: %" PRIuDADDR "\n", taddr);

    /* The table can be variable length, so we loop on it 
     * The idx variable shows which round it is
     * Each structure is 512-bytes each
     */

    max_part = 1;		/* set it to 1 and it will be set in the first loop */
    for (idx = 0; idx < max_part; idx++) {
	uint32_t part_start;
	uint32_t part_size;
	char *str;
	SSIZE_T cnt;


	/* Read the entry */
	cnt = mm_read_block_nobuf
	    (mm, (char *) &part, sizeof(part), MAC_PART_SOFFSET + idx);

	/* If -1, then tsk_errno is already set */
	if (cnt != sizeof(part)) {
	    if (cnt != -1) {
		tsk_errno = TSK_ERR_MM_READ;
		tsk_errstr[0] = '\0';
	    }
	    snprintf(tsk_errstr2, TSK_ERRSTR_L,
		"MAC Partition entry %" PRIuDADDR, taddr + idx);
	    return 1;
	}


	/* Sanity Check */
	if (idx == 0) {
	    /* Set the endian ordering the first time around */
	    if (mm_guessu16(mm, part.magic, MAC_MAGIC)) {
		tsk_errno = TSK_ERR_MM_MAGIC;
		snprintf(tsk_errstr, TSK_ERRSTR_L,
		    "Mac partition table entry (Sector: %"
		    PRIuDADDR ") %" PRIx16,
		    (taddr + idx), getu16(mm, part.magic));
		tsk_errstr2[0] = '\0';
		return 1;
	    }

	    /* Get the number of partitions */
	    max_part = getu32(mm, part.pmap_size);
	}
	else if (getu16(mm, part.magic) != MAC_MAGIC) {
	    tsk_errno = TSK_ERR_MM_MAGIC;
	    snprintf(tsk_errstr, TSK_ERRSTR_L,
		"Mac partition table entry (Sector: %"
		PRIuDADDR ") %" PRIx16, (taddr + idx),
		getu16(mm, part.magic));
	    tsk_errstr2[0] = '\0';
	    return 1;
	}


	part_start = getu32(mm, part.start_sec);
	part_size = getu32(mm, part.size_sec);

	if (verbose)
	    fprintf(stderr,
		"mac_load: %d  Starting Sector: %" PRIu32 "  Size: %"
		PRIu32 " Type: %s\n", idx, part_start, part_size,
		part.type);

	if (part_size == 0)
	    continue;

	if (part_start > max_addr) {
	    tsk_errno = TSK_ERR_MM_BLK_NUM;
	    snprintf(tsk_errstr, TSK_ERRSTR_L,
		"mac_load_table: Starting sector too large for image");
	    tsk_errstr2[0] = '\0';
	    return 1;
	}


	if ((str = mymalloc(sizeof(part.name))) == NULL)
	    return 1;

	strncpy(str, (char *) part.type, sizeof(part.name));

	if (NULL == mm_part_add(mm, (DADDR_T) part_start,
		(DADDR_T) part_size, MM_TYPE_VOL, str, -1, idx))
	    return 1;
    }

    /* Add an entry for the table length */
    if ((table_str = mymalloc(16)) == NULL)
	return 1;

    snprintf(table_str, 16, "Table");
    if (NULL == mm_part_add(mm, taddr, max_part, MM_TYPE_DESC,
	    table_str, -1, -1))
	return 1;

    return 0;
}


/* 
 * Walk the partitions that have already been loaded during _open
 *
 * return 0 on success and 1 on error
 */
uint8_t
mac_part_walk(MM_INFO * mm, PNUM_T start, PNUM_T last, int flags,
    MM_PART_WALK_FN action, char *ptr)
{
    MM_PART *part;
    unsigned int cnt = 0;

    if (start < mm->first_part || start > mm->last_part) {
	tsk_errno = TSK_ERR_MM_WALK_RNG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "mac_part_walk: Start partition: %" PRIuPNUM "", start);
	tsk_errstr2[0] = '\0';
	return 1;
    }

    if (last < mm->first_part || last > mm->last_part) {
	tsk_errno = TSK_ERR_MM_WALK_RNG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "mac_part_walk: Ending partition: %" PRIuPNUM "", last);
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
mac_close(MM_INFO * mm)
{
    mm_part_free(mm);
    free(mm);
}

/* 
 * Process img_info as a Mac disk.  Initialize mm_info or return
 * NULL on error
 * */
MM_INFO *
mac_open(IMG_INFO * img_info, DADDR_T offset)
{
    MM_INFO *mm = (MM_INFO *) mymalloc(sizeof(*mm));
    if (mm == NULL)
	return NULL;

    mm->img_info = img_info;
    mm->mmtype = MM_MAC;
    mm->str_type = "MAC Partition Map";

    /* If an offset was given, then use that too */
    mm->offset = offset;

    //mm->sect_offset = offset + MAC_PART_OFFSET;

    /* inititialize settings */
    mm->part_list = NULL;
    mm->first_part = mm->last_part = 0;
    mm->endian = 0;
    mm->dev_bsize = 512;
    mm->block_size = 512;

    /* Assign functions */
    mm->part_walk = mac_part_walk;
    mm->close = mac_close;

    /* Load the partitions into the sorted list */
    if (mac_load_table(mm)) {
	mac_close(mm);
	return NULL;
    }

    /* fill in the sorted list with the 'unknown' values */
    if (mm_part_unused(mm)) {
	mac_close(mm);
	return NULL;
    }

    return mm;
}
