/*
 * $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
 *
 * Joachim Metz <forensics@hoffmannbv.nl>, Hoffmann Investigations
 * Copyright (c) 2006 Joachim Metz.  All rights reserved 
 *
 * ewf
 *
 * This software is distributed under the Common Public License 1.0
 *
 */

#include "img_tools.h"
#include "ewf.h"


static SSIZE_T
ewf_image_read_random(IMG_INFO * img_info, SSIZE_T vol_offset, char *buf,
    OFF_T len, OFF_T offset)
{
    SSIZE_T cnt;
    IMG_EWF_INFO *ewf_info = (IMG_EWF_INFO *) img_info;
    OFF_T tot_offset = offset + vol_offset;

    if (verbose)
	fprintf(stderr,
	    "ewf_read_random: byte offset: %" PRIuOFF " len: %" PRIuOFF
	    "\n", offset, len);

    cnt = libewf_read_random(ewf_info->handle, buf, len, tot_offset);
    if (cnt == -1) {
	// @@@ Add more specific error message
	tsk_errno = TSK_ERR_IMG_READ;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "ewf_read_random - offset: %" PRIuOFF " - len: %"
	    PRIuOFF " - %s", tot_offset, len, strerror(errno));
	tsk_errstr2[0] = '\0';
	return -1;
    }

    return cnt;
}

OFF_T
ewf_image_get_size(IMG_INFO * img_info)
{
    return img_info->size;
}

void
ewf_image_imgstat(IMG_INFO * img_info, FILE * hFile)
{
    IMG_EWF_INFO *ewf_info = (IMG_EWF_INFO *) img_info;

    fprintf(hFile, "IMAGE FILE INFORMATION\n");
    fprintf(hFile, "--------------------------------------------\n");
    fprintf(hFile, "Image Type:\t\tewf\n");
    fprintf(hFile, "\nSize of data in bytes:\t%" PRIuOFF "\n",
	img_info->size);

    if (ewf_info->md5hash != NULL) {
	fprintf(hFile, "MD5 hash of data:\t%s\n", ewf_info->md5hash);
    }
    return;
}

void
ewf_image_close(IMG_INFO * img_info)
{
    IMG_EWF_INFO *ewf_info = (IMG_EWF_INFO *) img_info;

    libewf_close(ewf_info->handle);

    free(ewf_info->md5hash);
    free(img_info);
}

/* Tests if the image file header against the
 * header (magic) signature specified.
 * Returns a 0 on no match and a 1 on a match, and -1 on error.
 */
int
img_file_header_signature_ncmp(const char *filename,
    const char *file_header_signature, int size_of_signature)
{
    int match;
    SSIZE_T read_count = 0;
    char header[512];
    int fd;

    if ((filename == NULL) || (file_header_signature == NULL)) {
	return (0);
    }
    if (size_of_signature <= 0) {
	return (0);
    }

    if ((fd = open(filename, O_RDONLY)) < 0) {
	tsk_errno = TSK_ERR_IMG_OPEN;
	snprintf(tsk_errstr, TSK_ERRSTR_L, "ewf magic testing: %s",
	    filename);
	tsk_errstr2[0] = '\0';
	return -1;
    }
    read_count = read(fd, header, 512);

    if (read_count != 512) {
	tsk_errno = TSK_ERR_IMG_READ;
	snprintf(tsk_errstr, TSK_ERRSTR_L, "ewf magic testing: %s",
	    filename);
	tsk_errstr2[0] = '\0';
	return -1;
    }
    close(fd);

    match = strncmp(file_header_signature, header, size_of_signature) == 0;

    return (match);
}


IMG_INFO *
ewf_open(int num_img, const char **images, IMG_INFO * next)
{
    IMG_EWF_INFO *ewf_info;
    IMG_INFO *img_info;

    if (next != NULL) {
	tsk_errno = TSK_ERR_IMG_LAYERS;
	snprintf(tsk_errstr, TSK_ERRSTR_L, "EWF must be lowest layer");
	tsk_errstr2[0] = '\0';
	return NULL;
    }

    ewf_info = (IMG_EWF_INFO *) mymalloc(sizeof(IMG_EWF_INFO));
    if (ewf_info == NULL) {
	return NULL;
    }
    memset((void *) ewf_info, 0, sizeof(IMG_EWF_INFO));

    img_info = (IMG_INFO *) ewf_info;

    /* check the magic before we call the library open */
    if (img_file_header_signature_ncmp(images[0],
	    "\x45\x56\x46\x09\x0d\x0a\xff\x00", 8) != 1) {
	//   if (libewf_check_file_signature(images[0]) == 0) {
	tsk_errno = TSK_ERR_IMG_MAGIC;
	snprintf(tsk_errstr, TSK_ERRSTR_L, "ewf_open: Not an EWF file");
	tsk_errstr2[0] = '\0';
	free(ewf_info);
	if (verbose)
	    fprintf(stderr, "Not an EWF file\n");

	return NULL;
    }

    ewf_info->handle = libewf_open(images, num_img, LIBEWF_OPEN_READ);
    img_info->size = libewf_data_size(ewf_info->handle);
    ewf_info->md5hash = libewf_data_md5hash(ewf_info->handle);

    img_info->itype = EWF_EWF;
    img_info->read_random = ewf_image_read_random;
    img_info->get_size = ewf_image_get_size;
    img_info->close = ewf_image_close;
    img_info->imgstat = ewf_image_imgstat;

    return img_info;
}
