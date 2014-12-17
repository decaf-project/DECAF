/*
 * The Sleuth Kit
 *
 * $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
 *
 * Brian Carrier [carrier@sleuthkit.org]
 * Copyright (c) 2003-2005 Brian Carrier.  All rights reserved
 *
 * mm_open - wrapper function for specific partition type
 *
 *
 * This software is distributed under the Common Public License 1.0
 *
 */

#include "mm_tools.h"


/* 
 * THe main wrapper function for opening an image
 *
 * Offset is the byte offset to the start of the volume system
 */
MM_INFO *
mm_open(IMG_INFO * img_info, DADDR_T offset, const char *type)
{
    /* Autodetect mode 
     * We need to try all of them in case there are multiple 
     * installations
     *
     *
     * NOte that errors that are encountered during the testing process
     * will not be reported
     */
    if (type == NULL) {
	MM_INFO *mm_info, *mm_set = NULL;
	char *set = NULL;

	if ((mm_info = dos_open(img_info, offset, 1)) != NULL) {
	    set = "DOS";
	    mm_set = mm_info;
	}
	else {
	    tsk_errno = 0;
	    tsk_errstr[0] = '\0';
	    tsk_errstr2[0] = '\0';
	}
	if ((mm_info = bsd_open(img_info, offset)) != NULL) {
	    // if (set == NULL) {
	    // In this case, BSD takes priority because BSD partitions start off with
	    // the DOS magic value in the first sector with the boot code.
	    set = "BSD";
	    mm_set = mm_info;
	    /*
	       }
	       else {
	       mm_set->close(mm_set);
	       mm_info->close(mm_info);
	       tsk_errno = TSK_ERR_MM_UNKTYPE;
	       snprintf(tsk_errstr, TSK_ERRSTR_L,
	       "BSD or %s at %" PRIuDADDR, set, offset);
	       tsk_errstr2[0] = '\0';
	       return NULL;
	       }
	     */
	}
	else {
	    tsk_errno = 0;
	    tsk_errstr[0] = '\0';
	    tsk_errstr2[0] = '\0';
	}
	if ((mm_info = gpt_open(img_info, offset)) != NULL) {
	    if (set == NULL) {
		set = "GPT";
		mm_set = mm_info;
	    }
	    else {
		mm_set->close(mm_set);
		mm_info->close(mm_info);
		tsk_errno = TSK_ERR_MM_UNKTYPE;
		snprintf(tsk_errstr, TSK_ERRSTR_L,
		    "GPT or %s at %" PRIuDADDR, set, offset);
		tsk_errstr2[0] = '\0';
		return NULL;
	    }
	}
	else {
	    tsk_errno = 0;
	    tsk_errstr[0] = '\0';
	    tsk_errstr2[0] = '\0';
	}

	if ((mm_info = sun_open(img_info, offset)) != NULL) {
	    if (set == NULL) {
		set = "Sun";
		mm_set = mm_info;
	    }
	    else {
		mm_set->close(mm_set);
		mm_info->close(mm_info);
		tsk_errno = TSK_ERR_MM_UNKTYPE;
		snprintf(tsk_errstr, TSK_ERRSTR_L,
		    "Sun or %s at %" PRIuDADDR, set, offset);
		tsk_errstr2[0] = '\0';
		return NULL;
	    }
	}
	else {
	    tsk_errno = 0;
	    tsk_errstr[0] = '\0';
	    tsk_errstr2[0] = '\0';
	}

	if ((mm_info = mac_open(img_info, offset)) != NULL) {
	    if (set == NULL) {
		set = "Mac";
		mm_set = mm_info;
	    }
	    else {
		mm_set->close(mm_set);
		mm_info->close(mm_info);
		tsk_errno = TSK_ERR_MM_UNKTYPE;
		snprintf(tsk_errstr, TSK_ERRSTR_L,
		    "Mac or %s at %" PRIuDADDR, set, offset);
		tsk_errstr2[0] = '\0';
		return NULL;
	    }
	}
	else {
	    tsk_errno = 0;
	    tsk_errstr[0] = '\0';
	    tsk_errstr2[0] = '\0';
	}

	if (mm_set == NULL) {
	    tsk_errno = TSK_ERR_MM_UNKTYPE;
	    tsk_errstr[0] = '\0';
	    tsk_errstr2[0] = '\0';
	    return NULL;
	}

	return mm_set;
    }
    else {
	uint8_t mmtype;

	/* Transate the string into the number */
	mmtype = mm_parse_type(type);

	switch (mmtype) {
	case MM_DOS:
	    return dos_open(img_info, offset, 0);
	case MM_MAC:
	    return mac_open(img_info, offset);
	case MM_BSD:
	    return bsd_open(img_info, offset);
	case MM_SUN:
	    return sun_open(img_info, offset);
	case MM_GPT:
	    return gpt_open(img_info, offset);
	case MM_UNSUPP:
	default:
	    tsk_errno = TSK_ERR_MM_UNSUPTYPE;
	    snprintf(tsk_errstr, TSK_ERRSTR_L, "%s", type);
	    tsk_errstr2[0] = '\0';
	    return NULL;
	}
    }
}
