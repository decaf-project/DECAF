/*
 * The Sleuth Kit
 *
 * $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
 *
 * Brian Carrier [carrier@sleuthkit.org]
 * Copyright (c) 2003 Brian Carrier.  All rights reserved
 *
 *
 * This software is distributed under the Common Public License 1.0
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "hfind.h"



/* Give the offset and the length of a line, this returns the 
 * address of the begining of the line
 */
#define base_addr(off, len) \
  ((len) * ( ( ( (off) + (len) - 1) / (len) ) - 1 ) )


/* Wrapper method to print the results using the appropriate db */
static void
tm_print(int type, char *db_file, char *hash, off_t db_off, int flags)
{
    if (type == TYPE_NSRL) {
	nsrl_print(db_file, hash, db_off, flags);
    }
    else if (type == TYPE_MD5SUM) {
	md5sum_print(db_file, hash, db_off, flags);
    }
    else if (type == TYPE_HK) {
	hk_print(db_file, hash, db_off, flags);
    }
}


static void
print_notfound(char *hash, unsigned char flags)
{
    if (flags & FLAG_QUICK) {
	printf("0\n");
    }
    else {
	printf("%s\t%s\n", hash, MSG_NOTFOUND);
    }
    return;
}

/*
 * Perform the binary search for hash
 */
int
tm_lookup(char *db_file, char *hash, unsigned char flags)
{
    char *buf, head[LEN_1], idx_file[LEN_1];
    FILE *hIdx;
    off_t offset, poffset, up, low;
    struct stat sb;
    int cmp, hash_len, idx_len, type;

    /* use the size of the hash that was given to determine where the
     * index is
     */
    if (strlen(hash) == LEN_MD5) {
	snprintf(idx_file, LEN_1, "%s-%s.idx", db_file, STR_MD5);
	if (NULL == (hIdx = fopen(idx_file, "r"))) {
	    fprintf(stderr, "The MD5 index file was not found for %s\n",
		    db_file);
	    fprintf(stderr,
		    "You must generate one with the '-i' option\n");
	    exit(1);
	}
	hash_len = HASH_LEN(HASH_MD5);
	idx_len = IDX_LEN(HASH_MD5);
    }

    else if (strlen(hash) == LEN_SHA1) {
	snprintf(idx_file, LEN_1, "%s-%s.idx", db_file, STR_SHA1);
	if (NULL == (hIdx = fopen(idx_file, "r"))) {
	    fprintf(stderr, "The SHA1 index file was not found for %s\n",
		    db_file);
	    fprintf(stderr,
		    "You must generate one with the '-i' option\n");
	    exit(1);
	}
	hash_len = HASH_LEN(HASH_SHA1);
	idx_len = IDX_LEN(HASH_SHA1);
    }
    else {
	fprintf(stderr, "%s\t%s\n", hash, MSG_INV_HASH);
	return 1;
    }


    /* Get the size of the index file so we have a range */
    if (0 != lstat(idx_file, &sb)) {
	fprintf(stderr, "Error getting size of index file: %s\n",
		idx_file);
	exit(1);
    }

    /* Read the first line here and set the offset */
    if (NULL == fgets(head, LEN_1, hIdx)) {
	fprintf(stderr, "Error reading header line of index file\n");
	exit(1);
    }

    if (strncmp(head, STR_HEAD, strlen(STR_HEAD)) == 0) {
	char *ptr;

	low = strlen(head);

	/* Skip the space */
	ptr = &head[strlen(STR_HEAD) + 1];

	ptr[strlen(ptr) - 1] = '\0';

	if (strcmp(ptr, STR_NSRL) == 0) {
	    type = TYPE_NSRL;
	}
	else if (strcmp(ptr, STR_MD5SUM) == 0) {
	    type = TYPE_MD5SUM;
	}
	else if (strcmp(ptr, STR_HK) == 0) {
	    type = TYPE_HK;
	}
	else {
	    fprintf(stderr, "Unknown Database Type: %s\n", ptr);
	    exit(1);
	}
    }
    else {
	fprintf(stderr, "Invalid index file: Missing header line\n");
	exit(1);
    }

    up = sb.st_size;

    if (((up - low) % idx_len) != 0) {
	fprintf(stderr,
		"Error, size of index file is not a multiple of row size\n");
	exit(1);
    }


    /* allocate a buffer for a row */
    buf = malloc(idx_len + 1);
    if (!buf) {
	fprintf(stderr, "hash_lookup: Error allocating memory\n");
	exit(1);
    }

    poffset = 0;
    while (1) {

	if (up == low) {
	    print_notfound(hash, flags);
	    break;
	}

	/* Get offset to look at */
	offset = base_addr(((up - low) / 2), idx_len);

	/* Sanity Check */
	if ((offset % idx_len) != 0) {
	    fprintf(stderr,
		    "Error, offset is not a multiple of the line length\n");
	    exit(1);
	}

	offset += low;

	/* Have we moved */
	if (poffset == offset) {
	    print_notfound(hash, flags);
	    break;
	}

	/* Seek to there and read it */
	if (0 != fseek(hIdx, offset, SEEK_SET)) {
	    fprintf(stderr, "tm_lookup: Error seeking in search: %lu\n",
		    (unsigned long) offset);
	    exit(1);
	}

	if (NULL == fgets(buf, idx_len + 1, hIdx)) {
	    if (feof(hIdx)) {
		print_notfound(hash, flags);
		break;
	    }

	    fprintf(stderr, "Error reading index file\n");
	    exit(1);
	}

	/* Sanity Check */
	if ((strlen(buf) < idx_len) || (buf[hash_len] != '|')) {
	    fprintf(stderr, "Invalid line in index file: %lu (%s)\n",
		    (unsigned long) (offset / idx_len), buf);
	    exit(1);
	}

	/* Set the delimter to NULL so we can treat the hash as a string */
	buf[hash_len] = '\0';
	cmp = strcasecmp(buf, hash);

	/* The one we just read is too small, so set the new lower bound
	 * at the start of the next row */
	if (cmp < 0) {
	    low = offset + idx_len;
	}

	/* The one we just read is too big, so set the upper bound at this
	 * entry */
	else if (cmp > 0) {
	    up = offset;
	}

	/* We found it */
	else {

	    if (flags & FLAG_QUICK) {
		printf("1\n");
	    }
	    else {
		off_t tmpoff, db_off;

		db_off = (off_t) strtoul(buf + hash_len + 1, NULL, 10);

		/* Print the one that we found first */
		tm_print(type, db_file, hash, db_off, flags);


		/* there could be additional entries both before and after
		 * this entry - but we can restrict ourselves to the up
		 * and low bounds from our previous hunting 
		 */

		tmpoff = offset - idx_len;
		while (tmpoff >= low) {

		    /* Break if we are at the header */
		    if (tmpoff <= 0)
			break;

		    if (0 != fseek(hIdx, tmpoff, SEEK_SET)) {
			fprintf(stderr,
				"tm_lookup: Error seeking for prev entries: %lu\n",
				(unsigned long) tmpoff);
			exit(1);
		    }

		    if (NULL == fgets(buf, idx_len + 1, hIdx)) {
			fprintf(stderr,
				"Error reading index file (prev)\n");
			exit(1);
		    }
		    else if (strlen(buf) < idx_len) {
			fprintf(stderr,
				"Invalid index file line (prev)\n");
			exit(1);
		    }

		    buf[hash_len] = '\0';
		    if (strcasecmp(buf, hash) != 0) {
			break;
		    }

		    db_off = (off_t) strtoul(buf + hash_len + 1, NULL, 10);
		    tm_print(type, db_file, hash, db_off, flags);
		    tmpoff -= idx_len;
		}

		/* next entries */
		tmpoff = offset + idx_len;
		while (tmpoff < up) {

		    if (0 != fseek(hIdx, tmpoff, SEEK_SET)) {
			fprintf(stderr,
				"tm_lookup: Error seeking for next entries: %lu\n",
				(unsigned long) tmpoff);
			exit(1);
		    }

		    if (NULL == fgets(buf, idx_len + 1, hIdx)) {
			if (feof(hIdx))
			    break;
			fprintf(stderr,
				"Error reading index file (next)\n");
			exit(1);
		    }
		    else if (strlen(buf) < idx_len) {
			fprintf(stderr,
				"Invalid index file line (next)\n");
			exit(1);
		    }

		    buf[hash_len] = '\0';
		    if (strcasecmp(buf, hash) != 0) {
			break;
		    }

		    db_off = (off_t) strtoul(buf + hash_len + 1, NULL, 10);
		    tm_print(type, db_file, hash, db_off, flags);

		    tmpoff += idx_len;
		}
	    }
	    break;
	}
	poffset = offset;
    }

    fclose(hIdx);
    free(buf);

    return 0;
}


int
tm_init(char *db_file, char *type)
{
    if (strcmp(type, STR_NSRL_MD5) == 0) {
	return nsrl_init(db_file, HASH_MD5);
    }
    else if (strcmp(type, STR_NSRL_SHA1) == 0) {
	return nsrl_init(db_file, HASH_SHA1);
    }
    else if (strcmp(type, STR_MD5SUM) == 0) {
	return md5sum_init(db_file);
    }
    else if (strcmp(type, STR_HK) == 0) {
	return hk_init(db_file);
    }
    else {
	fprintf(stderr, "Unsupported type: %s\n", type);
	fprintf(stderr, "\t%s\n", STR_SUPPORT);
    }
    return 1;
}
