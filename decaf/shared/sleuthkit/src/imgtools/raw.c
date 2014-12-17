/*
 * $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
 *
 * Brian Carrier [carrier@sleuthkit.org]
 * Copyright (c) 2006 Brian Carrier, Basis Technology.  All rights reserved
 * Copyright (c) 2005 Brian Carrier.  All rights reserved
 *
 * raw
 *
 * This software is distributed under the Common Public License 1.0
 *
 */

#include <sys/stat.h>
#include "img_tools.h"
#include "raw.h"


/* Return the size read and -1 if error */
static SSIZE_T
raw_read_random(IMG_INFO * img_info, SSIZE_T vol_offset, char *buf,
    OFF_T len, OFF_T offset)
{
    SSIZE_T cnt;
    IMG_RAW_INFO *raw_info = (IMG_RAW_INFO *) img_info;

    if (verbose)
	fprintf(stderr,
	    "raw_read_random: byte offset: %" PRIuOFF " len: %" PRIuOFF
	    "\n", offset, len);

    // is there another layer?
    if (img_info->next) {
	return img_info->next->read_random(img_info->next, vol_offset, buf,
	    len, offset);
    }

    // Read the data
    else {
	off_t tot_offset = offset + vol_offset;

	if (raw_info->seek_pos != tot_offset) {
	    if (lseek(raw_info->fd, tot_offset, SEEK_SET) != tot_offset) {
		tsk_errno = TSK_ERR_IMG_SEEK;
		snprintf(tsk_errstr, TSK_ERRSTR_L,
		    "raw_read_random - %" PRIuOFF " - %s",
		    tot_offset, strerror(errno));
		tsk_errstr2[0] = '\0';
		return -1;
	    }
	    raw_info->seek_pos = tot_offset;
	}

	cnt = read(raw_info->fd, buf, len);
	if (cnt == -1) {
	    tsk_errno = TSK_ERR_IMG_READ;
	    snprintf(tsk_errstr, TSK_ERRSTR_L,
		"raw_read_random - offset: %" PRIuOFF " - len: %"
		PRIuOFF " - %s", tot_offset, len, strerror(errno));
	    tsk_errstr2[0] = '\0';
	    return -1;
	}
	raw_info->seek_pos += cnt;
	return cnt;
    }
}

OFF_T
raw_get_size(IMG_INFO * img_info)
{
    return img_info->size;
}

void
raw_imgstat(IMG_INFO * img_info, FILE * hFile)
{
    fprintf(hFile, "IMAGE FILE INFORMATION\n");
    fprintf(hFile, "--------------------------------------------\n");
    fprintf(hFile, "Image Type: raw\n");
    fprintf(hFile, "\nSize in bytes: %" PRIuOFF "\n", img_info->size);
    return;
}

void
raw_close(IMG_INFO * img_info)
{
    IMG_RAW_INFO *raw_info = (IMG_RAW_INFO *) img_info;
    close(raw_info->fd);
}


/*
 *  Open the file as a raw image.  Return the IMG_INFO structure
 *  or NULL if the file cannot be opened.  There are no magic values
 *  to test for a raw file
 */
int raw_fd = 0;

IMG_INFO *
raw_open(const char **images, IMG_INFO * next)
{
    IMG_RAW_INFO *raw_info;
    IMG_INFO *img_info;

    if ((raw_info =
	    (IMG_RAW_INFO *) mymalloc(sizeof(IMG_RAW_INFO))) == NULL)
	return NULL;

    memset((void *) raw_info, 0, sizeof(IMG_RAW_INFO));

    img_info = (IMG_INFO *) raw_info;

    img_info->itype = RAW_SING;
    img_info->read_random = raw_read_random;
    img_info->get_size = raw_get_size;
    img_info->close = raw_close;
    img_info->imgstat = raw_imgstat;

    if (next) {
	img_info->next = next;
	img_info->size = next->get_size(next);
    }

    /* Open the file */
    if(raw_fd) raw_info->fd = raw_fd;
    else {
	struct stat stat_buf;

	/* Exit if we are given a directory */
	if (stat(images[0], &stat_buf) == -1) {
	    tsk_errno = TSK_ERR_IMG_STAT;
	    snprintf(tsk_errstr, TSK_ERRSTR_L, "raw_open directory check");
	    tsk_errstr2[0] = '\0';
	    return NULL;
	}
	else if ((stat_buf.st_mode & S_IFMT) == S_IFDIR) {
	    if (verbose)
		fprintf(stderr, "raw_open: image %s is a directory\n",
		    images[0]);

	    tsk_errno = TSK_ERR_IMG_MAGIC;
	    snprintf(tsk_errstr, TSK_ERRSTR_L,
		"raw_open: Image is a directory");
	    tsk_errstr2[0] = '\0';
	    return NULL;
	}

	if ((raw_info->fd = open(images[0], O_RDONLY|O_RSYNC|O_SYNC|O_DSYNC)) < 0) {
	    tsk_errno = TSK_ERR_IMG_OPEN;
	    snprintf(tsk_errstr, TSK_ERRSTR_L, "raw_open file: %s msg: %s",
		images[0], strerror(errno));
	    tsk_errstr2[0] = '\0';
	    return NULL;
	}
    }

    img_info->size = lseek(raw_info->fd, 0, SEEK_END);
    lseek(raw_info->fd, 0, SEEK_SET);
    raw_info->seek_pos = 0;
    
    return img_info;
}

qemu_pread_t qemu_pread = NULL;

/* Return the size read and -1 if error */
static SSIZE_T
qemu_read_random(IMG_INFO * img_info, SSIZE_T vol_offset, char *buf,
    OFF_T len, OFF_T offset)
{
	IMG_QEMU_INFO *qemu_info = (IMG_QEMU_INFO *)img_info;
	if(qemu_pread) return qemu_pread(qemu_info->opaque, vol_offset+offset, buf, len);
	return -1;
}

OFF_T
qemu_get_size(IMG_INFO * img_info)
{
    return 2048*1024*1024; //fixme!!!!
}

void
qemu_imgstat(IMG_INFO * img_info, FILE * hFile)
{
    fprintf(hFile, "IMAGE FILE INFORMATION\n");
    fprintf(hFile, "--------------------------------------------\n");
    fprintf(hFile, "Image Type: qemu image\n");
    fprintf(hFile, "\nSize in bytes: %" PRIuOFF "\n", img_info->size);
    return;
}

void
qemu_close(IMG_INFO * img_info)
{
}



IMG_INFO *
qemu_image_open(void *opaque)
{
    IMG_QEMU_INFO *qemu_info;
    IMG_INFO *img_info;

    if ((qemu_info =
	    (IMG_QEMU_INFO *) mymalloc(sizeof(IMG_QEMU_INFO))) == NULL)
	return NULL;

    memset((void *) qemu_info, 0, sizeof(IMG_QEMU_INFO));

    img_info = (IMG_INFO *) qemu_info;

    img_info->itype = RAW_SING;
    img_info->read_random = qemu_read_random;
    img_info->get_size = qemu_get_size;
    img_info->close = qemu_close;
    img_info->imgstat = qemu_imgstat;
    qemu_info->opaque = opaque;
    img_info->size = 2048*1024*1024; //fixme !!!
 
    return img_info;
}
