/*
 * The Sleuth Kit
 *
 * $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
 *
 * Brian Carrier [carrier@sleuthkit.org]
 * Copyright (c) 2003 Brian Carrier.  All rights reserved
 *
 * This software is distributed under the Common Public License 1.0
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "hfind.h"


static void
usage()
{
    fprintf(stderr,
	    "\nhfind [-eqV] [-f lookup_file] [-i db_type] db_file [hashes]\n");
    fprintf(stderr,
	    "\t-e: Extended mode - where values other than just the name are printed\n");
    fprintf(stderr,
	    "\t-q: Quick mode - where a 1 is printed if it is found, else 0\n");
    fprintf(stderr, "\t-V: Print version to STDOUT\n");
    fprintf(stderr,
	    "\t-f lookup_file: File with one hash per line to lookup\n");
    fprintf(stderr,
	    "\t-i db_type: Create index file for a given hash database type\n");
    fprintf(stderr,
	    "\tdb_file: The location of the original hash database\n");
    fprintf(stderr,
	    "\t[hashes]: hashes to lookup (STDIN is used otherwise)\n");
    fprintf(stderr, "\n\tSupported types: %s\n", STR_SUPPORT);

    exit(1);
}

void
print_version()
{
#ifndef VER
#define VER 0
#endif
    printf("The Sleuth Kit ver %s\n", VER);
    return;
}

int
main(int argc, char **argv)
{
    int ch;
    char *init_type = NULL;
    char *db_file = NULL, *lookup_file = NULL;
    unsigned char flags = 0;

    while ((ch = getopt(argc, argv, "ef:i:qV")) > 0) {
	switch (ch) {
	case 'e':
	    flags |= FLAG_EXT;
	    break;

	case 'f':
	    lookup_file = optarg;
	    break;

	case 'i':
	    init_type = optarg;
	    break;

	case 'q':
	    flags |= FLAG_QUICK;
	    break;
	case 'V':
	    print_version();
	    exit(0);
	default:
	    usage();

	}
    }

    if (optind + 1 > argc) {
	fprintf(stderr, "Error: You must provide a location\n");
	usage();
    }

    db_file = argv[optind++];

    /* What mode are we doing to run in */
    if (init_type != NULL) {
	/* Get the flags right */
	if (lookup_file != NULL) {
	    fprintf(stderr, "'-f' flag can't be used with '-i'\n");
	    usage();
	}

	if (flags & FLAG_QUICK) {
	    fprintf(stderr, "'-q' flag can't be used with '-i'\n");
	    usage();
	}
	if (flags & FLAG_EXT) {
	    fprintf(stderr, "'-e' flag can't be used with '-i'\n");
	    usage();
	}

	tm_init(db_file, init_type);
    }

    /* Do lookups */
    else {
	/* The values were passed on the command line */
	if (optind < argc) {

	    if ((optind + 1 < argc) && (flags & FLAG_QUICK)) {
		fprintf(stderr,
			"Error: Only one hash can be given with quick option\n");
		exit(1);
	    }

	    if ((flags & FLAG_EXT) && (flags & FLAG_QUICK)) {
		fprintf(stderr, "'-e' flag can't be used with '-q'\n");
		usage();
	    }

	    if (lookup_file != NULL) {
		fprintf(stderr,
			"Error: -f can't be used when hashes are also given\n");
		exit(1);
	    }

	    /* Loop through all provided values
	     */
	    while (optind < argc) {
		tm_lookup(db_file, argv[optind++], flags);
	    }
	}
	/* stdin */
	else {
	    char buf[100];
	    FILE *handle;

	    /* If the file was specified, use that - otherwise stdin */
	    if (lookup_file != NULL) {
		handle = fopen(lookup_file, "r");
		if (!handle) {
		    fprintf(stderr, "Error opening hash file: %s\n",
			    lookup_file);
		    exit(1);
		}
	    }
	    else {
		handle = stdin;
	    }

	    while (1) {
		if (NULL == fgets(buf, 100, handle)) {
		    break;
		}

		/* Remove the newline */
		buf[strlen(buf) - 1] = '\0';

		tm_lookup(db_file, buf, flags);

		/* We only do one hash in quick mode */
		if (flags & FLAG_QUICK)
		    break;
	    }
	}
    }

    return 0;

}
