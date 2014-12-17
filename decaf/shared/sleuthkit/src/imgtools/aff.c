/*
 * $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
 *
 * Brian Carrier [carrier@sleuthkit.org]
 * Copyright (c) 2006 Brian Carrier, Basis Technology.  All rights reserved
 *
 * This software is distributed under the Common Public License 1.0
 */

#include "img_tools.h"
#include "afflib.h"
#include "aff.h"

static SSIZE_T
aff_read_random(IMG_INFO * img_info, SSIZE_T vol_offset, char *buf,
    OFF_T len, OFF_T offset)
{
    SSIZE_T cnt;
    IMG_AFF_INFO *aff_info = (IMG_AFF_INFO *) img_info;
    off_t tot_offset = offset + vol_offset;

    if (verbose)
	fprintf(stderr,
	    "aff_read_random: byte offset: %" PRIuOFF " len: %" PRIuOFF
	    "\n", offset, len);


    if (aff_info->seek_pos != tot_offset) {
	if ((off_t) af_seek(aff_info->af_file, tot_offset, SEEK_SET) !=
	    tot_offset) {
	    // @@@ ADD more specific error messages
	    tsk_errno = TSK_ERR_IMG_SEEK;
	    snprintf(tsk_errstr, TSK_ERRSTR_L,
		"aff_read_random - %" PRIuOFF " - %s", tot_offset,
		strerror(errno));
	    tsk_errstr2[0] = '\0';
	    return -1;

	}
	aff_info->seek_pos = tot_offset;
    }

    cnt = af_read(aff_info->af_file, (unsigned char *) buf, len);
    if (cnt == -1) {
	// @@@ Add more specific error message
	tsk_errno = TSK_ERR_IMG_READ;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "aff_read_random - offset: %" PRIuOFF " - len: %"
	    PRIuOFF " - %s", tot_offset, len, strerror(errno));
	tsk_errstr2[0] = '\0';
	return -1;
    }

    aff_info->seek_pos += cnt;
    return cnt;
}

OFF_T
aff_get_size(IMG_INFO * img_info)
{
    return img_info->size;
}

void
aff_imgstat(IMG_INFO * img_info, FILE * hFile)
{
    IMG_AFF_INFO *aff_info = (IMG_AFF_INFO *) img_info;
    unsigned char buf[512];
    size_t buf_len = 512;


    fprintf(hFile, "IMAGE FILE INFORMATION\n");
    fprintf(hFile, "--------------------------------------------\n");
    fprintf(hFile, "Image Type: ");
    switch (aff_info->type) {
    case AF_IDENTIFY_AFF:
	fprintf(hFile, "AFF\n");
	break;
    case AF_IDENTIFY_AFD:
	fprintf(hFile, "AFD\n");
	break;
    case AF_IDENTIFY_AFM:
	fprintf(hFile, "AFM\n");
	break;
    default:
	fprintf(hFile, "?\n");
	break;
    }

    fprintf(hFile, "\nSize in bytes: %" PRIuOFF "\n", img_info->size);

    fprintf(hFile, "\nMD5: ");
    if (af_get_seg(aff_info->af_file, AF_MD5, NULL, buf, &buf_len) == 0) {
	int i;
	for (i = 0; i < 16; i++) {
	    fprintf(hFile, "%x", buf[i]);
	}
	fprintf(hFile, "\n");
    }
    else {
	fprintf(hFile, "Segment not found\n");
    }

    buf_len = 512;
    fprintf(hFile, "SHA1: ");
    if (af_get_seg(aff_info->af_file, AF_SHA1, NULL, buf, &buf_len) == 0) {
	int i;
	for (i = 0; i < 20; i++) {
	    fprintf(hFile, "%x", buf[i]);
	}
	fprintf(hFile, "\n");
    }
    else {
	fprintf(hFile, "Segment not found\n");
    }

    /* Creator segment */
    buf_len = 512;
    if (af_get_seg(aff_info->af_file, AF_CREATOR, NULL, buf,
	    &buf_len) == 0) {
	buf[buf_len] = '\0';
	fprintf(hFile, "Creator: %s\n", buf);
    }

    buf_len = 512;
    if (af_get_seg(aff_info->af_file, AF_CASE_NUM, NULL, buf,
	    &buf_len) == 0) {
	buf[buf_len] = '\0';
	fprintf(hFile, "Case Number: %s\n", buf);
    }

    buf_len = 512;
    if (af_get_seg(aff_info->af_file, AF_IMAGE_GID, NULL, buf,
	    &buf_len) == 0) {
	unsigned int i;
	fprintf(hFile, "Image GID: ");
	for (i = 0; i < buf_len; i++) {
	    fprintf(hFile, "%X", buf[i]);
	}
	fprintf(hFile, "\n");
    }

    buf_len = 512;
    if (af_get_seg(aff_info->af_file, AF_ACQUISITION_DATE, NULL, buf,
	    &buf_len) == 0) {
	buf[buf_len] = '\0';
	fprintf(hFile, "Acquisition Date: %s\n", buf);
    }

    buf_len = 512;
    if (af_get_seg(aff_info->af_file, AF_ACQUISITION_NOTES, NULL, buf,
	    &buf_len) == 0) {
	buf[buf_len] = '\0';
	fprintf(hFile, "Acquisition Notes: %s\n", buf);
    }

    buf_len = 512;
    if (af_get_seg(aff_info->af_file, AF_ACQUISITION_DEVICE, NULL, buf,
	    &buf_len) == 0) {
	buf[buf_len] = '\0';
	fprintf(hFile, "Acquisition Device: %s\n", buf);
    }

    buf_len = 512;
    if (af_get_seg(aff_info->af_file, AF_AFFLIB_VERSION, NULL, buf,
	    &buf_len) == 0) {
	buf[buf_len] = '\0';
	fprintf(hFile, "AFFLib Version: %s\n", buf);
    }

    buf_len = 512;
    if (af_get_seg(aff_info->af_file, AF_DEVICE_MANUFACTURER, NULL, buf,
	    &buf_len) == 0) {
	buf[buf_len] = '\0';
	fprintf(hFile, "Device Manufacturer: %s\n", buf);
    }

    buf_len = 512;
    if (af_get_seg(aff_info->af_file, AF_DEVICE_MODEL, NULL, buf,
	    &buf_len) == 0) {
	buf[buf_len] = '\0';
	fprintf(hFile, "Device Model: %s\n", buf);
    }

    buf_len = 512;
    if (af_get_seg(aff_info->af_file, AF_DEVICE_SN, NULL, buf,
	    &buf_len) == 0) {
	buf[buf_len] = '\0';
	fprintf(hFile, "Device SN: %s\n", buf);
    }

    return;
}

void
aff_close(IMG_INFO * img_info)
{
    IMG_AFF_INFO *aff_info = (IMG_AFF_INFO *) img_info;
    af_close(aff_info->af_file);
}


IMG_INFO *
aff_open(const char **images, IMG_INFO * next)
{
    IMG_AFF_INFO *aff_info;
    IMG_INFO *img_info;
    int type;

    if (next != NULL) {
	tsk_errno = TSK_ERR_IMG_LAYERS;
	snprintf(tsk_errstr, TSK_ERRSTR_L, "AFF must be lowest layer");
	tsk_errstr2[0] = '\0';
	return NULL;
    }

    aff_info = (IMG_AFF_INFO *) mymalloc(sizeof(IMG_AFF_INFO));
    if (aff_info == NULL) {
	return NULL;
    }
    memset((void *) aff_info, 0, sizeof(IMG_AFF_INFO));

    img_info = (IMG_INFO *) aff_info;

    img_info->read_random = aff_read_random;
    img_info->get_size = aff_get_size;
    img_info->close = aff_close;
    img_info->imgstat = aff_imgstat;


    type = af_identify_type(images[0]);
    if ((type == AF_IDENTIFY_ERR) || (type == AF_IDENTIFY_NOEXIST)) {
	if (verbose) {
	    fprintf(stderr,
		"aff_open: Error determining type of file: %s\n",
		images[0]);
	    perror("aff_open");
	}
	tsk_errno = TSK_ERR_IMG_OPEN;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "aff_open file: %s: Error checking type", images[0]);
	tsk_errstr2[0] = '\0';
	free(aff_info);
	return NULL;
    }
    else if (type == AF_IDENTIFY_AFF) {
	img_info->itype = AFF_AFF;
    }
    else if (type == AF_IDENTIFY_AFD) {
	img_info->itype = AFF_AFD;
    }
    else if (type == AF_IDENTIFY_AFM) {
	img_info->itype = AFF_AFM;
    }
//    else if ((type == AF_IDENTIFY_EVF) || (type ==AF_IDENTIFY_EVD  )) {
//      img_info->itype = AFF_AFF;
    //   }
    else {
	tsk_errno = TSK_ERR_IMG_MAGIC;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "aff_open: Not an AFF, AFD, or AFM file");
	tsk_errstr2[0] = '\0';
	free(aff_info);
	if (verbose)
	    fprintf(stderr, "Not an AFF/AFD/AFM file\n");

	return NULL;
    }

    aff_info->af_file = af_open(images[0], O_RDONLY, 0);
    if (!aff_info->af_file) {
	tsk_errno = TSK_ERR_IMG_OPEN;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "aff_open file: %s: Error opening - %s", images[0],
	    strerror(errno));
	tsk_errstr2[0] = '\0';
	free(aff_info);
	if (verbose) {
	    fprintf(stderr, "Error opening AFF/AFD/AFM file\n");
	    perror("aff_open");
	}
	return NULL;
    }
    aff_info->type = type;

    img_info->size = af_imagesize(aff_info->af_file);

    af_seek(aff_info->af_file, 0, SEEK_SET);
    aff_info->seek_pos = 0;

    return img_info;
}
