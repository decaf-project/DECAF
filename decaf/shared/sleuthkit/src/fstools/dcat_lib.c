/*
** dcat
** The  Sleuth Kit 
**
** $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
**
** Given an image , block number, and size, display the contents
** of the block to stdout.
** 
** Brian Carrier [carrier@sleuthkit.org]
** Copyright (c) 2006 Brian Carrier, Basis Technology.  All Rights reserved
** Copyright (c) 2003-2005 Brian Carrier.  All rights reserved
**
** TASK
** Copyright (c) 2002 Brian Carrier, @stake Inc.  All rights reserved
**
** TCTUTILs
** Copyright (c) 2001 Brian Carrier.  All rights reserved
**
**
** This software is distributed under the Common Public License 1.0
**
*/

#include "libfstools.h"
#include <ctype.h>


static void
stats(FS_INFO * fs)
{
    printf("%d: Size of Addressable Unit\n", fs->block_size);
}


/* return 1 on error and 0 on success */
uint8_t
fs_dcat(FS_INFO * fs, uint8_t lclflags, DADDR_T addr,
    DADDR_T read_num_units)
{
    OFF_T read_num_bytes;
    DATA_BUF *buf;
    SSIZE_T cnt;

    if (lclflags & DCAT_STAT) {
	stats(fs);
	return 0;
    }

    /* Multiply number of units by block size  to get size in bytes */
    read_num_bytes = read_num_units * fs->block_size;

    if (lclflags & DCAT_HTML) {
	printf("<html>\n");
	printf("<head>\n");
	printf("<title>Unit: %" PRIuDADDR "   Size: %" PRIuOFF
	    " bytes</title>\n", addr, read_num_bytes);
	printf("</head>\n");
	printf("<body>\n");

    }

    buf = data_buf_alloc(read_num_bytes);
    if (buf == NULL) {
	return 1;
    }

    /* Read the data */
    if (addr > fs->last_block) {
	tsk_errno = TSK_ERR_FS_ARG;
	snprintf(tsk_errstr, TSK_ERRSTR_L,
	    "fs_dcat: block is larger than last block in image (%"
	    PRIuDADDR ")", fs->last_block);
	return 1;
    }
    cnt = fs_read_block(fs, buf, read_num_bytes, addr);
    if (cnt != (SSIZE_T) read_num_bytes) {
	if (cnt != -1) {
	    tsk_errno = TSK_ERR_FS_READ;
	    tsk_errstr[0] = '\0';
	}
	snprintf(tsk_errstr2, TSK_ERRSTR_L,
	    "dcat: Error reading block at %" PRIuDADDR, addr);
	return 1;
    }


    /* do a hexdump like printout */
    if (lclflags & DCAT_HEX) {
	OFF_T idx1, idx2;

	if (lclflags & DCAT_HTML)
	    printf("<table border=0>\n");

	for (idx1 = 0; idx1 < read_num_bytes; idx1 += 16) {
	    if (lclflags & DCAT_HTML)
		printf("<tr><td>%" PRIuOFF "</td>", idx1);
	    else
		printf("%" PRIuOFF "\t", idx1);


	    for (idx2 = 0; idx2 < 16; idx2++) {
		if ((lclflags & DCAT_HTML) && (0 == (idx2 % 4)))
		    printf("<td>");

		printf("%.2x", buf->data[idx2 + idx1] & 0xff);

		if (3 == (idx2 % 4)) {
		    if (lclflags & DCAT_HTML)
			printf("</td>");
		    else
			printf(" ");
		}
	    }

	    printf("\t");
	    for (idx2 = 0; idx2 < 16; idx2++) {
		if ((lclflags & DCAT_HTML) && (0 == (idx2 % 4)))
		    printf("<td>");

		if ((isascii((int) buf->data[idx2 + idx1])) &&
		    (!iscntrl((int) buf->data[idx2 + idx1])))
		    printf("%c", buf->data[idx2 + idx1]);
		else
		    printf(".");

		if (3 == (idx2 % 4)) {
		    if (lclflags & DCAT_HTML)
			printf("</td>");
		    else
			printf(" ");
		}
	    }

	    if (lclflags & DCAT_HTML)
		printf("</tr>");

	    printf("\n");
	}


	if (lclflags & DCAT_HTML)
	    printf("</table>\n");
	else
	    printf("\n");

    }				/* end of if hexdump */

    /* print in all ASCII */
    else if (lclflags & DCAT_ASCII) {
	OFF_T idx;
	for (idx = 0; idx < read_num_bytes; idx++) {

	    if ((isprint((int) buf->data[idx]))
		|| (buf->data[idx] == '\t')) {
		printf("%c", buf->data[idx]);
	    }
	    else if ((buf->data[idx] == '\n') || (buf->data[idx] == '\r')) {
		if (lclflags & DCAT_HTML)
		    printf("<br>");
		printf("%c", buf->data[idx]);
	    }
	    else
		printf(".");
	}
	if (lclflags & DCAT_HTML)
	    printf("<br>");

	printf("\n");
    }

    /* print raw */
    else {
	if (fwrite(buf->data, read_num_bytes, 1, stdout) != 1) {
	    tsk_errno = TSK_ERR_FS_WRITE;
	    snprintf(tsk_errstr, TSK_ERRSTR_L,
		"dcat_lib: error writing to stdout: %s", strerror(errno));
	    data_buf_free(buf);
	    return 1;
	}

	if (lclflags & DCAT_HTML)
	    printf("<br>\n");
    }

    data_buf_free(buf);

    if (lclflags & DCAT_HTML)
	printf("</body>\n</html>\n");

    return 0;
}
