/*
 * The Sleuth Kit
 *
 * $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
 *
 * Brian Carrier [carrier@sleuthkit.org]
 * Copyright (c) 2003-2005 Brian Carrier.  All rights reserved
 * 
 * 
 */

#ifndef _BSD_H
#define _BSD_H

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct {
	uint8_t magic[4];
	uint8_t type[2];
	uint8_t sub_type[2];
	uint8_t type_name[16];

	uint8_t packname[16];

	uint8_t sec_size[4];
	uint8_t sec_per_tr[4];
	uint8_t tr_per_cyl[4];
	uint8_t cyl_per_unit[4];
	uint8_t sec_per_cyl[4];
	uint8_t sec_per_unit[4];

	uint8_t spare_per_tr[2];
	uint8_t spare_per_cyl[2];

	uint8_t alt_per_unit[4];

	uint8_t rpm[2];
	uint8_t interleave[2];
	uint8_t trackskew[2];
	uint8_t cylskew[2];
	uint8_t headswitch[4];
	uint8_t track_seek[4];
	uint8_t flags[4];

	uint8_t drivedata[20];

	uint8_t reserved1[20];

	uint8_t magic2[4];
	uint8_t checksum[2];

	uint8_t num_parts[2];
	uint8_t bootarea_size[4];
	uint8_t sb_size[4];

	struct {
	    uint8_t size_sec[4];
	    uint8_t start_sec[4];
	    uint8_t frag_size[4];
	    uint8_t fstype;
	    uint8_t frag_per_block;
	    uint8_t cyl_per_grp[2];
	} part[16];

	/* padding to make it a full 512 bytes */
	uint8_t reserved2[108];

    } bsd_disklabel;

#define BSD_MAGIC	0x82564557
#define BSD_PART_SOFFSET	1

#ifdef __cplusplus
}
#endif
#endif
