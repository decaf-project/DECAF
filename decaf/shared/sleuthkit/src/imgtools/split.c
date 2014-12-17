/*
 * $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
 *
 * Brian Carrier [carrier@sleuthkit.org]
 * Copyright (c) 2006 Brian Carrier, Basis Technology.  All rights reserved
 * Copyright (c) 2005 Brian Carrier.  All rights reserved
 *
 * split
 *
 *
 * This software is distributed under the Common Public License 1.0
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include "img_tools.h"
#include "split.h"


/* return the size read or -1 on error */
static SSIZE_T
split_read_random(IMG_INFO * img_info, SSIZE_T vol_offset, char *buf,
    OFF_T len, OFF_T offset)
{
    IMG_SPLIT_INFO *split_info = (IMG_SPLIT_INFO *) img_info;
    OFF_T tot_offset;
    int i;

    if (verbose)
	fprintf(stderr,
	    "split_read_random: byte offset: %" PRIuOFF " len: %"
	    PRIuOFF "\n", offset, len);

    // Find the offset of the data
    tot_offset = offset + vol_offset;

    // Find the location of the offset
    for (i = 0; i < split_info->num_img; i++) {

	/* Does the data start in this image? */
	if (tot_offset < split_info->max_off[i]) {
	    off_t rel_offset;
	    OFF_T read_len;
	    SSIZE_T cnt;
	    IMG_SPLIT_CACHE *cimg;;


	    /* Get the offset relative to this image */
	    if (i > 0) {
		rel_offset = tot_offset - split_info->max_off[i - 1];
	    }
	    else {
		rel_offset = tot_offset;
	    }

	    /* Get the length to read */
	    if ((split_info->max_off[i] - tot_offset) >= len)
		read_len = len;
	    else
		read_len = split_info->max_off[i] - tot_offset;


	    if (verbose)
		fprintf(stderr,
		    "split_read_rand: found in image %d relative: %"
		    PRIuOFF "  len: %" PRIuOFF "\n", i, rel_offset,
		    read_len);

	    /* Is the image already open? */
	    if (split_info->cptr[i] == -1) {
		if (verbose)
		    fprintf(stderr,
			"split_read_rand: opening file into slot %d %s\n",
			split_info->next_slot, split_info->images[i]);

		/* Grab the next cache slot */
		cimg = &split_info->cache[split_info->next_slot];

		/* Free it if being used */
		if (cimg->fd != 0) {
		    if (verbose)
			fprintf(stderr,
			    "split_read_rand: closing file %s\n",
			    split_info->images[cimg->image]);
		    close(cimg->fd);
		    split_info->cptr[cimg->image] = -1;
		}

		if ((cimg->fd = open(split_info->images[i], O_RDONLY)) < 0) {
		    tsk_errno = TSK_ERR_IMG_OPEN;
		    snprintf(tsk_errstr, TSK_ERRSTR_L,
			"split_read_random file: %s msg: %s",
			split_info->images[i], strerror(errno));
		    tsk_errstr2[0] = '\0';
		    return -1;
		}

		cimg->image = i;
		cimg->seek_pos = 0;
		split_info->cptr[i] = split_info->next_slot;
		if (++split_info->next_slot == SPLIT_CACHE) {
		    split_info->next_slot = 0;
		}
	    }
	    else {
		cimg = &split_info->cache[split_info->cptr[i]];
	    }

	    if (cimg->seek_pos != rel_offset) {
		if (lseek(cimg->fd, rel_offset, SEEK_SET) != rel_offset) {
		    tsk_errno = TSK_ERR_IMG_SEEK;
		    snprintf(tsk_errstr, TSK_ERRSTR_L,
			"split_read_random - %s - %" PRIuOFF " - %s",
			split_info->images[i], rel_offset,
			strerror(errno));
		    tsk_errstr2[0] = '\0';
		    return -1;
		}
		cimg->seek_pos = rel_offset;
	    }

	    cnt = read(cimg->fd, buf, read_len);
	    if (cnt == -1) {
		tsk_errno = TSK_ERR_IMG_READ;
		snprintf(tsk_errstr, TSK_ERRSTR_L,
		    "split_read_random - offset: %" PRIuOFF
		    " - len: %" PRIuOFF " - %s", tot_offset, len,
		    strerror(errno));
		tsk_errstr2[0] = '\0';
		return -1;
	    }
	    cimg->seek_pos += cnt;


	    /* Go to the next image(s) */
	    if (((OFF_T) cnt == read_len) && (read_len != len)) {
		SSIZE_T cnt2;

		len -= read_len;

		while (len > 0) {
		    /* go to the next image */
		    i++;

		    if (split_info->max_off[i] -
			split_info->max_off[i - 1] >= len)
			read_len = len;
		    else
			read_len =
			    split_info->max_off[i] -
			    split_info->max_off[i - 1];

		    if (verbose)
			fprintf(stderr,
			    "split_read_rand: Additional image reads: image %d  len: %"
			    PRIuOFF "\n", i, read_len);

		    /* Is the image already open? */
		    if (split_info->cptr[i] == -1) {
			if (verbose)
			    fprintf(stderr,
				"split_read_rand: opening file into slot %d %s\n",
				split_info->next_slot,
				split_info->images[i]);

			/* Grab the next cache slot */
			cimg = &split_info->cache[split_info->next_slot];

			/* Free it if being used */
			if (cimg->fd != 0) {
			    if (verbose)
				fprintf(stderr,
				    "split_read_rand: closing file %s\n",
				    split_info->images[cimg->image]);
			    close(cimg->fd);
			    split_info->cptr[cimg->image] = -1;
			}

			if ((cimg->fd =
				open(split_info->images[i],
				    O_RDONLY)) < 0) {
			    tsk_errno = TSK_ERR_IMG_OPEN;
			    snprintf(tsk_errstr, TSK_ERRSTR_L,
				"split_read_random file: %s   msg: %s",
				split_info->images[i], strerror(errno));
			    tsk_errstr2[0] = '\0';
			    return -1;
			}

			cimg->image = i;
			cimg->seek_pos = 0;
			split_info->cptr[i] = split_info->next_slot;
			if (++split_info->next_slot == SPLIT_CACHE) {
			    split_info->next_slot = 0;
			}
		    }
		    else {
			cimg = &split_info->cache[split_info->cptr[i]];
		    }

		    /* Go to the beginning */
		    if (cimg->seek_pos != 0) {
			if (lseek(cimg->fd, 0, SEEK_SET) != 0) {
			    tsk_errno = TSK_ERR_IMG_SEEK;
			    snprintf(tsk_errstr, TSK_ERRSTR_L,
				"split_read_random - %s - 0 - %s",
				split_info->images[i], strerror(errno));
			    tsk_errstr2[0] = '\0';
			    return -1;
			}
			cimg->seek_pos = 0;
		    }

		    cnt2 = read(cimg->fd, &buf[cnt], read_len);
		    if (cnt2 == -1) {
			tsk_errno = TSK_ERR_IMG_READ;
			snprintf(tsk_errstr, TSK_ERRSTR_L,
			    "split_read_random - offset: 0 - len: %"
			    PRIuOFF " - %s", read_len, strerror(errno));
			tsk_errstr2[0] = '\0';
			return -1;
		    }
		    cimg->seek_pos += cnt2;
		    cnt += cnt2;

		    if ((OFF_T) cnt2 != read_len)
			return cnt;

		    len -= cnt2;
		}
	    }

	    return cnt;
	}
    }

    tsk_errno = TSK_ERR_IMG_READ_OFF;
    snprintf(tsk_errstr, TSK_ERRSTR_L,
	"split_read_random - %" PRIuOFF " - %s", tot_offset,
	strerror(errno));
    tsk_errstr2[0] = '\0';
    return -1;
}

void
split_imgstat(IMG_INFO * img_info, FILE * hFile)
{
    IMG_SPLIT_INFO *split_info = (IMG_SPLIT_INFO *) img_info;
    int i;

    fprintf(hFile, "IMAGE FILE INFORMATION\n");
    fprintf(hFile, "--------------------------------------------\n");
    fprintf(hFile, "Image Type: split\n");
    fprintf(hFile, "\nSize in bytes: %" PRIuOFF "\n", img_info->size);

    fprintf(hFile, "\n--------------------------------------------\n");
    fprintf(hFile, "Split Information:\n");

    for (i = 0; i < split_info->num_img; i++) {
	fprintf(hFile, "%s  (%" PRIuOFF " to %" PRIuOFF ")\n",
	    split_info->images[i],
	    (OFF_T) (i == 0) ? 0 : split_info->max_off[i - 1],
	    (OFF_T) (split_info->max_off[i] - 1));
    }

}


OFF_T
split_get_size(IMG_INFO * img_info)
{
    return img_info->size;
}

void
split_close(IMG_INFO * img_info)
{
    int i;
    IMG_SPLIT_INFO *split_info = (IMG_SPLIT_INFO *) img_info;
    for (i = 0; i < SPLIT_CACHE; i++) {
	if (split_info->cache[i].fd != 0)
	    close(split_info->cache[i].fd);
    }
    free(split_info->cptr);
    free(split_info);
}


/*
 * Return IMG_INFO or NULL if an error occurs
 */
IMG_INFO *
split_open(int num_img, const char **images, IMG_INFO * next)
{
    IMG_SPLIT_INFO *split_info;
    IMG_INFO *img_info;
    int i;

    if (next != NULL) {
	tsk_errno = TSK_ERR_IMG_LAYERS;
	snprintf(tsk_errstr, TSK_ERRSTR_L, "split must be lowest layer");
	tsk_errstr2[0] = '\0';
	return NULL;
    }

    if ((split_info =
	    (IMG_SPLIT_INFO *) mymalloc(sizeof(IMG_SPLIT_INFO))) == NULL)
	return NULL;

    memset((void *) split_info, 0, sizeof(IMG_SPLIT_INFO));

    img_info = (IMG_INFO *) split_info;

    img_info->itype = RAW_SPLIT;
    img_info->read_random = split_read_random;
    img_info->get_size = split_get_size;
    img_info->close = split_close;
    img_info->imgstat = split_imgstat;
    img_info->next = NULL;

    /* Open the files */
    split_info->cptr = (int *) mymalloc(num_img * sizeof(int));
    if (split_info->cptr == NULL) {
	free(split_info);
	return NULL;
    }

    memset((void *) &split_info->cache, 0,
	SPLIT_CACHE * sizeof(IMG_SPLIT_CACHE));
    split_info->next_slot = 0;

    split_info->max_off = (OFF_T *) mymalloc(num_img * sizeof(OFF_T));
    if (split_info->max_off == NULL) {
	free(split_info->cptr);
	free(split_info);
	return NULL;
    }
    img_info->size = 0;

    split_info->num_img = num_img;
    split_info->images = images;

    /* Get size info for each file - we do not open each one because that
     * could cause us to run out of file decsriptors when we only need a few.
     * The descriptors are opened as needed
     */
    for (i = 0; i < num_img; i++) {
	struct stat sb;

	split_info->cptr[i] = -1;
	if (stat(images[i], &sb) == -1) {
	    tsk_errno = TSK_ERR_IMG_STAT;
	    snprintf(tsk_errstr, TSK_ERRSTR_L, "split_open - %s - %s",
		images[i], strerror(errno));
	    tsk_errstr2[0] = '\0';
	    free(split_info->max_off);
	    free(split_info->cptr);
	    free(split_info);
	    return NULL;
	}
	else if ((sb.st_mode & S_IFMT) == S_IFDIR) {
	    if (verbose)
		fprintf(stderr, "split_open: image %s is a directory\n",
		    images[i]);

	    tsk_errno = TSK_ERR_IMG_MAGIC;
	    snprintf(tsk_errstr, TSK_ERRSTR_L,
		"split_open: Image is a directory");
	    tsk_errstr2[0] = '\0';
	    return NULL;
	}

	/* Add the size of this image to the total and save the current max */
	img_info->size += sb.st_size;
	split_info->max_off[i] = img_info->size;

	if (verbose)
	    fprintf(stderr,
		"split_open: %d  size: %" PRIuOFF "  max offset: %"
		PRIuOFF "  Name: %s\n", i, sb.st_size,
		split_info->max_off[i], images[i]);
    }

    return img_info;
}
