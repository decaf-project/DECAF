/*
 * The Sleuth Kit
 *
 * $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
 * 
 * Brian Carrier [carrier@sleuthkit.org]
 * Copyright (c) 2005 Brian Carrier.  All rights reserved 
 *
 * Based on code from http://www.win.tue.nl/~aeb/linux/setmax.c
 * and from hdparm
 *
 * This software is distributed under the Common Public License 1.0
 *
 */

/* setmax.c - aeb, 000326 - use on 2.4.0test9 or newer */
/* IBM part thanks to Matan Ziv-Av <matan@svgalib.org> */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "tsk_os.h"
#include "tsk_types.h"

#if defined(LINUX2)

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/hdreg.h>
#include <string.h>

#include "aux_lib.h"
#include "disk_ide.h"

DISK_INFO *
identify_device(int fd)
{
    unsigned char id_args[4 + 512];
    uint16_t *id_val;
    DISK_INFO *di;
    uint64_t id_size;

    di = (DISK_INFO *) mymalloc(sizeof(DISK_INFO));
    di->flags = 0;

    /* Execute the IDENTIFY DEVICE command */
    memset(id_args, 0, 516);
    id_args[0] = WIN_IDENTIFY;
    id_args[3] = 1;

    if (ioctl(fd, HDIO_DRIVE_CMD, &id_args)) {
	id_args[0] = WIN_PIDENTIFY;
	if (ioctl(fd, HDIO_DRIVE_CMD, &id_args)) {
	    fprintf(stderr, "IDENTIFY DEVICE failed\n");
	    exit(1);
	}
    }

    /* The result is organized by 16-bit words */
    id_val = (uint16_t *) & id_args[4];

    if (id_val[0] & 0x8000) {
	fprintf(stderr, "Device is not an ATA disk\n");
	exit(1);
    }
    /* Give up if LBA or HPA is not supported */
    if ((id_val[49] & 0x0200) == 0) {
	fprintf(stderr, "Error: LBA mode not supported by drive\n");
	exit(1);
    }

    if (id_val[82] & 0x400) {
	di->flags |= DISK_HPA;
    }

    /* Check if the capabilities field is valid and 48-bit is set */
    if (((id_val[83] & 0xc000) == 0x4000) && (id_val[83] & 0x0400)) {

	id_size = (uint64_t) id_val[103] << 48 |
	    (uint64_t) id_val[102] << 32 |
	    (uint64_t) id_val[101] << 16 | (uint64_t) id_val[100];

	if (id_size == 0)
	    id_size = (uint64_t) id_val[61] << 16 | id_val[60];

	di->flags |= DISK_48;
    }

    /* Use the 28-bit fields */
    else {
	id_size = (uint64_t) id_val[61] << 16 | id_val[60];
    }

    di->capacity = id_size;

    return di;
}

uint64_t
get_native_max(int fd)
{
    unsigned char task_args[7];
    uint64_t nat_size;
    int i;

    /* Get the actual size using GET NATIVE MAX */
    task_args[0] = 0xF8;
    task_args[1] = 0x00;
    task_args[2] = 0x00;
    task_args[3] = 0x00;
    task_args[4] = 0x00;
    task_args[5] = 0x00;
    task_args[6] = 0x40;

    if (ioctl(fd, HDIO_DRIVE_TASK, &task_args)) {
	fprintf(stderr, "READ NATIVE MAX ADDRESS failed\n");
	for (i = 0; i < 7; i++)
	    fprintf(stderr, "%d = 0x%x\n", i, task_args[i]);
	exit(1);
    }

    nat_size = ((task_args[6] & 0xf) << 24) + (task_args[5] << 16) +
	(task_args[4] << 8) + task_args[3];

    /* @@@ Do the 48-bit version */
    if (nat_size == 0x0fffffff) {
	fprintf(stderr,
		"This disk uses the 48-bit ATA commands, which are not supported\n");
	exit(1);
    }

    return nat_size;
}

void
set_max(int fd, uint64_t addr)
{
    unsigned char task_args[7];
    uint64_t tmp_size;
    int i;

    /* Does this require the EXT version? */
    if (addr > 0x0fffffff) {
	// @@@ Need EXT version
	fprintf(stderr,
		"This disk requires the 48-bit commands, which are not yet supported\n");
	exit(1);
    }

    else {
	/* Now we reset the max address to nat_size */
	task_args[0] = 0xf9;
	task_args[1] = 0;
	task_args[2] = 0;	// Make it temporary

	/* Convert the LBA address to the proper register location */
	tmp_size = addr;
	task_args[3] = (tmp_size & 0xff);
	tmp_size >>= 8;
	task_args[4] = (tmp_size & 0xff);
	tmp_size >>= 8;
	task_args[5] = (tmp_size & 0xff);
	tmp_size >>= 8;
	task_args[6] = (tmp_size & 0x0f);

	task_args[6] |= 0x40;	/* Set the LBA mode */

	if (ioctl(fd, HDIO_DRIVE_TASK, &task_args)) {
	    fprintf(stderr, "SET MAX failed\n");
	    for (i = 0; i < 7; i++)
		fprintf(stderr, "%d = 0x%x\n", i, task_args[i]);
	    exit(1);
	}
    }
}

#endif
