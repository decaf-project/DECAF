/*
** get
** The Sleuth Kit
**
** $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $ 
**
** routines to get values in a structure that solves endian issues
**
** Brian Carrier [carrier@sleuthkit.org]
** Copyright (c) 2003-2005 Brian Carrier.  All rights reserved 
**
** Copyright (c) 2002 Brian Carrier, @stake Inc.  All rights reserved
**
** This software is distributed under the Common Public License 1.0
**
*/

#include "tsk_os.h"
#include "tsk_types.h"
#include "tsk_endian.h"


/* A temporary data structure with an endian field */
typedef struct {
    uint8_t endian;
} tmp_ds;

/*
 * try both endian orderings to figure out which one is equal to 'val'
 *
 * if neither of them do, then 1 is returned.  Else 0 is.
 * fs->flags will be set accordingly
 */
uint8_t
guess_end_u16(uint8_t * flag, uint8_t * x, uint16_t val)
{
    tmp_ds ds;

    /* try little */
    ds.endian = TSK_LIT_ENDIAN;
    if (getu16(&ds, x) == val) {
	*flag &= ~TSK_BIG_ENDIAN;
	*flag |= TSK_LIT_ENDIAN;
	return 0;
    }

    /* ok, big now */
    ds.endian = TSK_BIG_ENDIAN;
    if (getu16(&ds, x) == val) {
	*flag &= ~TSK_LIT_ENDIAN;
	*flag |= TSK_BIG_ENDIAN;
	return 0;
    }

    /* didn't find it */
    return 1;
}

/*
 * same idea as guessu16 except that val is a 32-bit value
 *
 * return 1 on error and 0 else
 */
uint8_t
guess_end_u32(uint8_t * flag, uint8_t * x, uint32_t val)
{
    tmp_ds ds;

    /* try little */
    ds.endian = TSK_LIT_ENDIAN;
    if (getu32(&ds, x) == val) {
	*flag &= ~TSK_BIG_ENDIAN;
	*flag |= TSK_LIT_ENDIAN;
	return 0;
    }

    /* ok, big now */
    ds.endian = TSK_BIG_ENDIAN;
    if (getu32(&ds, x) == val) {
	*flag &= ~TSK_LIT_ENDIAN;
	*flag |= TSK_BIG_ENDIAN;
	return 0;
    }

    return 1;
}
