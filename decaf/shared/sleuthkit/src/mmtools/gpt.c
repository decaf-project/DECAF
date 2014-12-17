/*
 * The Sleuth Kit
 *
 * $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
 *
 * Brian Carrier [carrier@sleuthkit.org]
 * Copyright (c) 2006 Brian Carrier, Basis Technology.  All rights reserved
 * Copyright (c) 2004-2005 Brian Carrier.  All rights reserved
 *
 * gpt: GUID Partition Tables 
 *
 *
 * This software is distributed under the Common Public License 1.0
 *
 */

#include "mm_tools.h"
#include "gpt.h"
#include "dos.h"


/* 
 * Process the partition table at the sector address 
 * 
 * It is loaded into the internal sorted list 
 */
static uint8_t
gpt_load_table(MM_INFO * mm)
{
    gpt_head head;
    gpt_entry *ent;
    dos_sect dos_part;
    unsigned int i, a;
    uint32_t ent_size;
    char *safe_str, *head_str, *tab_str, *ent_buf;
    SSIZE_T cnt;
    DADDR_T taddr = mm->offset / mm->block_size + GPT_PART_SOFFSET;
    DADDR_T max_addr = (mm->img_info->size - mm->offset) / mm->block_size;	// max sector

    if (verbose)
	fprintf(stderr, "gpt_load_table: Sector: %" PRIuDADDR "\n", taddr);

    cnt = mm_read_block_nobuf
	(mm, (char *) &dos_part, sizeof(dos_part), GPT_PART_SOFFSET);
    /* if -1, then tsk_errno is already set */
    if (cnt != sizeof(dos_part)) {
	if (cnt != -1) {
	    tsk_errno = TSK_ERR_MM_READ;
	    tsk_errstr[0] = '\0';
	}
	snprintf(tsk_errstr2, TSK_ERRSTR_L,
	    "Error reading DOS safety partition table in Sector: %"
	    PRIuDADDR, taddr);
	return 1;
    }

    /* Sanity Check */
    if (mm_guessu16(mm, dos_part.magic, DOS_MAGIC)) {
	tsk_errno = TSK_ERR_MM_MAGIC;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "Missing DOS safety partition (invalid magic) (Sector: %"
	    PRIuDADDR ")", taddr);
	tsk_errstr2[0] = '\0';
	return 1;
    }

    if (dos_part.ptable[0].ptype != GPT_DOS_TYPE) {
	tsk_errno = TSK_ERR_MM_MAGIC;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "Missing DOS safety partition (invalid type in table: %d)",
	    dos_part.ptable[0].ptype);
	tsk_errstr2[0] = '\0';
	return 1;
    }

    if ((safe_str = mymalloc(16)) == NULL)
	return 1;

    snprintf(safe_str, 16, "Safety Table");
    if (NULL == mm_part_add(mm, (DADDR_T) 0, (DADDR_T) 1, MM_TYPE_DESC,
	    safe_str, -1, -1))
	return 1;


    /* Read the GPT header */
    cnt = mm_read_block_nobuf
	(mm, (char *) &head, sizeof(head), GPT_PART_SOFFSET + 1);
    if (cnt != sizeof(head)) {
	if (cnt != -1) {
	    tsk_errno = TSK_ERR_MM_READ;
	    tsk_errstr[0] = '\0';
	}
	snprintf(tsk_errstr2, TSK_ERRSTR_L,
	    "GPT Header structure in Sector: %" PRIuDADDR, taddr + 1);
	return 1;
    }


    if (getu64(mm, &head.signature) != GPT_HEAD_SIG) {
	tsk_errno = TSK_ERR_MM_MAGIC;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "GPT Header: %" PRIx64, getu64(mm, &head.signature));
	tsk_errstr2[0] = '\0';
	return 1;
    }

    if ((head_str = mymalloc(16)) == NULL)
	return 1;

    snprintf(head_str, 16, "GPT Header");
    if (NULL == mm_part_add(mm, (DADDR_T) 1,
	    (DADDR_T) ((getu32(mm, &head.head_size_b) + 511) / 512),
	    MM_TYPE_DESC, head_str, -1, -1))
	return 1;

    /* Allocate a buffer for each table entry */
    ent_size = getu32(mm, &head.tab_size_b);
    if (ent_size < sizeof(gpt_entry)) {
	tsk_errno = TSK_ERR_MM_MAGIC;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "Header reports partition entry size of %" PRIu32
	    " and not %lu", ent_size, sizeof(gpt_entry));
	tsk_errstr2[0] = '\0';
	return 1;
    }

    if ((tab_str = mymalloc(20)) == NULL)
	return 1;

    snprintf(tab_str, 20, "Partition Table");
    if (NULL == mm_part_add(mm, (DADDR_T) getu64(mm, &head.tab_start_lba),
	    (DADDR_T) ((ent_size * getu32(mm, &head.tab_num_ent) +
		    511) / 512), MM_TYPE_DESC, tab_str, -1, -1))
	return 1;


    /* Process the partition table */
    if ((ent_buf = mymalloc(mm->block_size)) == NULL)
	return 1;

    i = 0;
    for (a = 0; i < getu32(mm, &head.tab_num_ent); a++) {
	char *name;

	/* Read a sector */
	cnt = mm_read_block_nobuf(mm, ent_buf, mm->block_size,
	    getu64(mm, &head.tab_start_lba) + a);
	if (cnt != mm->block_size) {
	    if (cnt != -1) {
		tsk_errno = TSK_ERR_MM_READ;
		tsk_errstr[0] = '\0';
	    }
	    snprintf(tsk_errstr2, TSK_ERRSTR_L,
		"Error reading GPT partition table sector : %"
		PRIuDADDR, getu64(mm, &head.tab_start_lba) + a);
	    return 1;
	}

	/* Process the sector */
	ent = (gpt_entry *) ent_buf;
	for (; (uintptr_t) ent < (uintptr_t) ent_buf + mm->block_size &&
	    i < getu32(mm, &head.tab_num_ent); ent++ && i++) {

	    if (verbose)
		fprintf(stderr,
		    "gpt_load: %d  Starting Sector: %" PRIu64
		    "  End: %" PRIu64 " Flag: %" PRIx64 "\n", i,
		    getu64(mm, ent->start_lba), getu64(mm,
			ent->end_lba), getu64(mm, ent->flags));


	    if (getu64(mm, ent->start_lba) == 0)
		continue;

	    if (getu64(mm, ent->start_lba) > max_addr) {
		tsk_errno = TSK_ERR_MM_BLK_NUM;
		snprintf(tsk_errstr, TSK_ERRSTR_L,
		    "gpt_load_table: Starting sector too large for image");
		tsk_errstr2[0] = '\0';
		return 1;
	    }


	    if ((name = mymalloc(72)) == NULL)
		return 1;

	    uni2ascii((char *) ent->name, 72, name, 72);
	    if (NULL == mm_part_add(mm, (DADDR_T) getu64(mm,
			ent->start_lba), (DADDR_T) (getu64(mm,
			    ent->end_lba) - getu64(mm,
			    ent->start_lba) + 1), MM_TYPE_VOL, name, -1,
		    i))
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
gpt_part_walk(MM_INFO * mm, PNUM_T start, PNUM_T last, int flags,
    MM_PART_WALK_FN action, char *ptr)
{
    MM_PART *part;
    unsigned int cnt = 0;

    if (start < mm->first_part || start > mm->last_part) {
	tsk_errno = TSK_ERR_MM_WALK_RNG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "Starting partition: %" PRIuPNUM "", start);
	tsk_errstr2[0] = '\0';
	return 1;
    }

    if (last < mm->first_part || last > mm->last_part) {
	tsk_errno = TSK_ERR_MM_WALK_RNG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "Ending partition: %" PRIuPNUM "", last);
	tsk_errstr2[0] = '\0';
	return 1;
    }

    part = mm->part_list;
    while ((part != NULL) && (cnt <= last)) {

	if (cnt >= start) {
	    int retval;
	    retval = action(mm, cnt, part, 0, ptr);
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
gpt_close(MM_INFO * mm)
{
    mm_part_free(mm);
    free(mm);
}

MM_INFO *
gpt_open(IMG_INFO * img_info, DADDR_T offset)
{
    MM_INFO *mm = (MM_INFO *) mymalloc(sizeof(*mm));
    if (mm == NULL)
	return NULL;

    mm->img_info = img_info;
    mm->mmtype = MM_GPT;
    mm->str_type = "GUID Partition Table";

    /* If an offset was given, then use that too */
    mm->offset = offset;

    /* inititialize settings */
    mm->part_list = NULL;
    mm->first_part = mm->last_part = 0;
    mm->endian = 0;
    mm->dev_bsize = 512;
    mm->block_size = 512;

    /* Assign functions */
    mm->part_walk = gpt_part_walk;
    mm->close = gpt_close;

    /* Load the partitions into the sorted list */
    if (gpt_load_table(mm)) {
	gpt_close(mm);
	return NULL;
    }

    /* fill in the sorted list with the 'unknown' values */
    if (mm_part_unused(mm)) {
	gpt_close(mm);
	return NULL;
    }

    return mm;
}
