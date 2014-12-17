/*
 * The Sleuth Kit
 *
 * $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
 *
 * Brian Carrier [carrier@sleuthkit.org]
 * Copyright (c) 2003-2005 Brian Carrier.  All rights reserved
 *
 */

#ifndef _MM_TOOLS_H
#define _MM_TOOLS_H

#ifdef __cplusplus
extern "C" {
#endif

    /*
     * External interface.
     */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/types.h>
#include <string.h>

#include "tsk_os.h"
#include "tsk_types.h"

#include "libauxtools.h"
#include "libimgtools.h"
#include "mm_types.h"


/* Flags for the return value of walk actions */
#define WALK_CONT	0x0
#define WALK_STOP	0x1
#define WALK_ERROR	0x2

/* Structures */
    typedef struct MM_INFO MM_INFO;
    typedef struct MM_PART MM_PART;

/* walk action functions */
    typedef uint8_t(*MM_PART_WALK_FN) (MM_INFO *, PNUM_T, MM_PART *, int,
	char *);

/* Walk flags */
// #define MM_FLAG_UNUSED       (1<<0)          /* Fill in the unused space */


/***************************************************************
 * MM_INFO: Allocated when a disk is opened
 */
    struct MM_INFO {
	IMG_INFO *img_info;	/* Image layer pointer */
	uint8_t mmtype;		/* type of media management */
	DADDR_T offset;		/* byte offset where volume system starts */
	char *str_type;
	unsigned int block_size;
	unsigned int dev_bsize;

	/* endian ordering flag - values given in tsk_endian.h */
	uint8_t endian;

	MM_PART *part_list;	/* linked list of partitions */

	PNUM_T first_part;	/* number of first partition */
	PNUM_T last_part;	/* number of last partition */

	/* media management type specific function pointers */
	 uint8_t(*part_walk) (MM_INFO *, PNUM_T, PNUM_T, int,
	    MM_PART_WALK_FN, char *);
	void (*close) (MM_INFO *);
    };




/***************************************************************
 * Generic structures  for partitions / slices
 */

    struct MM_PART {
	MM_PART *prev;
	MM_PART *next;

	DADDR_T start;
	DADDR_T len;
	char *desc;
	int8_t table_num;
	int8_t slot_num;
	uint8_t type;
    };

#define MM_TYPE_DESC    0x01
#define MM_TYPE_VOL     0x02

    extern uint8_t mm_part_unused(MM_INFO *);
    extern void mm_part_print(MM_INFO *);
    extern MM_PART *mm_part_add(MM_INFO *, DADDR_T, DADDR_T, uint8_t,
	char *, int8_t, int8_t);
    extern void mm_part_free(MM_INFO *);



/**************************************************************8
 * Generic routines.
 */
    extern MM_INFO *mm_open(IMG_INFO *, DADDR_T, const char *);
//extern SSIZE_T mm_read_block(FS_INFO *, FS_BUF *, int, DADDR_T, const char *);
    extern SSIZE_T mm_read_block_nobuf(MM_INFO *, char *, OFF_T, DADDR_T);
    extern void mm_print_types(FILE *);

    /*
     * Support for DOS Partitions
     */
    extern MM_INFO *dos_open(IMG_INFO *, DADDR_T, uint8_t);
    extern MM_INFO *mac_open(IMG_INFO *, DADDR_T);
    extern MM_INFO *bsd_open(IMG_INFO *, DADDR_T);
    extern MM_INFO *sun_open(IMG_INFO *, DADDR_T);
    extern MM_INFO *gpt_open(IMG_INFO *, DADDR_T);


// Endian macros - actual functions in misc/
#define mm_guessu16(mm, x, mag)   \
	guess_end_u16(&(mm->endian), (x), (mag))

#define mm_guessu32(mm, x, mag)   \
	guess_end_u32(&(mm->endian), (x), (mag))

#ifdef __cplusplus
}
#endif
#endif
