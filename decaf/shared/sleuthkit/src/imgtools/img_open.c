/*
 * $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
 *
 * Brian Carrier [carrier@sleuthkit.org]
 * Copyright (c) 2006 Brian Carrier, Basis Technology.  All Rights reserved
 * Copyright (c) 2005 Brian Carrier.  All rights reserved
 *
 * img_open
 *
 *
 * This software is distributed under the Common Public License 1.0
 *
 */

#include <string.h>
#include "img_tools.h"

#include "raw.h"
#include "split.h"
#include "aff.h"
#include "ewf.h"

/*
 * type is a list of types: "raw", "split", "split,raid", 
 * offset is the sector offset for the file system or other code to use when reading:
 *      "63", "63@512", "62@2048"
 * num_img is the number of images in the last argument
 * images is the array of image names to open
 *
 * The highest layer is returned or NULL if an error occurs
 */
IMG_INFO *
img_open(const char *type, const int num_img, const char **images)
{
    IMG_INFO *img_info = NULL;
    char *tp, type_lcl[128], *type_lcl_p, *next;
    const char **img_tmp;
    int num_img_tmp = num_img;


    if ((num_img == 0) || (images[0] == NULL)) {
	tsk_errno = TSK_ERR_IMG_NOFILE;
	snprintf(tsk_errstr, TSK_ERRSTR_L, "img_open");
	tsk_errstr2[0] = '\0';
	return NULL;
    }

    if (verbose)
	fprintf(stderr,
	    "img_open: Type: %s   NumImg: %d  Img1: %s\n",
	    (type ? type : "n/a"), num_img, images[0]);

    // only the first in list (lowest) layer gets the files
    img_tmp = images;

    /* If no type is given, then we use the autodetection methods 
     * In case the image file matches the signatures of multiple formats,
     * we try all of the embedded formats 
     */

    if (type == NULL) {
	IMG_INFO *img_set = NULL;
	char *set = NULL;

	// we rely on tsk_errno, so make sure it is 0
	tsk_errno = 0;

	/* Try the non-raw formats first */
	if ((img_info = aff_open(images, NULL)) != NULL) {
	    set = "AFF";
	    img_set = img_info;
	}
	else {
	    tsk_errno = 0;
	    tsk_errstr[0] = '\0';
	    tsk_errstr2[0] = '\0';
	}

	if ((img_info = ewf_open(num_img, images, NULL)) != NULL) {
	    if (set == NULL) {
		set = "EWF";
		img_set = img_info;
	    }
	    else {
		img_set->close(img_set);
		img_info->close(img_info);
		tsk_errno = TSK_ERR_IMG_UNKTYPE;
		snprintf(tsk_errstr, TSK_ERRSTR_L, "EWF or %s", set);
		tsk_errstr2[0] = '\0';
		return NULL;
	    }
	}
	else {
	    tsk_errno = 0;
	    tsk_errstr[0] = '\0';
	    tsk_errstr2[0] = '\0';
	}

	if (img_set != NULL)
	    return img_set;


	/* We'll use the raw format */
	if (num_img == 1) {
	    if ((img_info = raw_open(images, NULL)) != NULL) {
		return img_info;
	    }
	    else if (tsk_errno) {
		return NULL;
	    }
	}
	else {
	    if ((img_info = split_open(num_img, images, NULL)) != NULL) {
		return img_info;
	    }
	    else if (tsk_errno) {
		return NULL;
	    }
	}
	tsk_errno = TSK_ERR_IMG_UNKTYPE;
	tsk_errstr[0] = '\0';
	tsk_errstr2[0] = '\0';
	return NULL;
    }

    /*
     * Type values
     * Make a local copy that we can modify the string as we parse it
     */
    strncpy(type_lcl, type, 128);
    type_lcl_p = type_lcl;

    /* We parse this and go up in the layers */
    tp = strtok(type_lcl, ",");
    while (tp != NULL) {
	uint8_t imgtype;

	next = strtok(NULL, ",");

	imgtype = img_parse_type(type);
	switch (imgtype) {
	case RAW_SING:

	    /* If we have more than one image name, and raw was the only
	     * type given, then use split */
	    if ((num_img > 1) && (next == NULL) && (img_tmp != NULL)) {
		img_info = split_open(num_img_tmp, img_tmp, img_info);
		num_img_tmp = 0;
	    }
	    else {
		img_info = raw_open(img_tmp, img_info);
	    }
	    img_tmp = NULL;
	    break;

	case RAW_SPLIT:

	    /* If only one image file is given, and only one type was
	     * given then use raw */
	    if ((num_img == 1) && (next == NULL) && (img_tmp != NULL)) {
		img_info = raw_open(img_tmp, img_info);
	    }
	    else {
		img_info = split_open(num_img_tmp, img_tmp, img_info);
		num_img_tmp = 0;
	    }

	    img_tmp = NULL;
	    break;

	case AFF_AFF:
	case AFF_AFD:
	case AFF_AFM:
	    img_info = aff_open(img_tmp, img_info);
	    break;

	case EWF_EWF:
	    img_info = ewf_open(num_img_tmp, img_tmp, img_info);
	    break;
	    
	case QEMU_IMG:
		img_info = 	qemu_image_open((void *)images[0]);    
		break;    

	default:
	    tsk_errno = TSK_ERR_IMG_UNSUPTYPE;
	    snprintf(tsk_errstr, TSK_ERRSTR_L, "%s", tp);
	    tsk_errstr2[0] = '\0';
	    return NULL;
	}

	/* Advance the pointer */
	tp = next;
    }

    /* Return the highest layer */
    return img_info;
}
