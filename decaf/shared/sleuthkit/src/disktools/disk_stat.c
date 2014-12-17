/*
 * The Sleuth Kit
 *
 * $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
 * 
 * Brian Carrier [carrier@sleuthkit.org]
 * Copyright (c) 2005 Brian Carrier.  All rights reserved 
 *
 *
 * This software is distributed under the Common Public License 1.0
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "tsk_os.h"
#include "disk_ide.h"
#include "libauxtools.h"

void
usage()
{
    fprintf(stderr, "usage: disk_stat [-V] DEVICE\n");
    fprintf(stderr, "\t-V: Print version\n");
    return;
}

#if defined(LINUX2)

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


int
main(int argc, char **argv)
{
    int fd;
    char *device = NULL;	/* e.g. "/dev/hda" */
    struct stat devstat;
    int ch;

    DISK_INFO *di;
    uint64_t nat_max = 0;

    while ((ch = getopt(argc, argv, "V")) > 0) {
	switch (ch) {
	case 'V':
	    print_version(stdout);
	    return 0;
	default:
	    usage();
	    return 0;
	}
    }

    if (optind < argc)
	device = argv[optind];

    if (!device) {
	fprintf(stderr, "no device specified\n");
	usage();
	exit(1);
    }

    if (0 != stat(device, &devstat)) {
	fprintf(stderr, "Error opening %s\n", device);
	exit(1);
    }

    if ((S_ISCHR(devstat.st_mode) == 0) && (S_ISBLK(devstat.st_mode) == 0)) {
	fprintf(stderr, "The file name must correspond to a device\n");
	exit(1);
    }

    fd = open(device, O_RDONLY);
    if (fd == -1) {
	fprintf(stderr,
	       "error opening device %s (%s)", device, strerror(errno));
	exit(1);
    }

    di = identify_device(fd);

    /* Check if the disk supports HPA */
    if ((di->flags & DISK_HPA) == 0) {
	nat_max = di->capacity - 1;
    }
    else {
	nat_max = get_native_max(fd);
    }

    close(fd);

    printf("Maximum Disk Sector: %" PRIu64 "\n", nat_max);
    printf("Maximum User Sector: %" PRIu64 "\n", di->capacity - 1);

    if (di->capacity - 1 != nat_max) {
	printf("\n** HPA Detected (Sectors %" PRIu64 " - %" PRIu64
	       ") **\n\n", di->capacity, nat_max);
    }

    if ((di->flags & DISK_HPA) == 0)
	printf("(Disk does not support HPA)\n");

    exit(0);
}

#else

int
main(int argc, char **argv)
{
    int ch;

    while ((ch = getopt(argc, argv, "V")) > 0) {
	switch (ch) {
	case 'V':
	    print_version(stdout);
	    return 0;
	default:
	    usage();
	    return 0;
	}
    }

    fprintf(stderr, "This tool works only on Linux systems\n");
    return 0;
}

#endif
