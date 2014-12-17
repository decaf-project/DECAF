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

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include "hfind.h"


#define STR_EMPTY ""

/*
 * name can be NULL
 */
static int
md5sum_extract_md5(char *str, char **md5, char **name)
{
    char *ptr;

    if (strlen(str) < LEN_MD5 + 1)
	return 1;

    /* Format of: MD5      NAME  or even just the MD5 value */
    if ((isxdigit((int) str[0])) && (isxdigit((int) str[LEN_MD5 - 1])) &&
	(isspace((int) str[LEN_MD5]))) {
	int i, len = strlen(str);

	if (md5 != NULL) {
	    *md5 = &str[0];
	}
	i = LEN_MD5;
	str[i++] = '\0';

	/* Just the MD5 values */
	if (i >= len) {
	    if (name != NULL) {
		*name = STR_EMPTY;
	    }
	    return 0;
	}

	while ((i < len) && ((str[i] == ' ') || (str[i] == '\t'))) {
	    i++;
	}

	if ((len == i) || (str[i] == '\n')) {
	    return 0;
	}

	if (str[i] == '*') {
	    i++;
	}

	if (name != NULL) {
	    *name = &str[i];
	}
	ptr = &str[i];

	if (ptr[strlen(ptr) - 1] == '\n')
	    ptr[strlen(ptr) - 1] = '\0';
    }

    /* Format of: MD5 (NAME) = MD5 */
    else if ((str[0] == 'M') && (str[1] == 'D') &&
	     (str[2] == '5') && (str[3] == ' ') && (str[4] == '(')) {

	ptr = &str[5];

	if (name != NULL) {
	    *name = ptr;
	}

	if (NULL == (ptr = strchr(ptr, ')'))) {
	    return 1;
	}
	*ptr = '\0';
	ptr++;

	if (4 + LEN_MD5 > strlen(ptr)) {
	    return 1;
	}

	if ((*(ptr) != ' ') || (*(++ptr) != '=') ||
	    (*(++ptr) != ' ') || (!isxdigit((int) *(++ptr))) ||
	    (ptr[LEN_MD5] != '\n')) {
	    return 1;
	}

	*md5 = ptr;
	ptr[LEN_MD5] = '\0';
    }

    else {
	fprintf(stderr, "Invalid md5sum format in file: %s\n", str);
	exit(1);
    }

    return 0;
}

/*
 * Create the index file 
 */
int
md5sum_init(char *db_file)
{
    FILE *hIn, *hOut;
    int i, len;
    char buf[LEN_1], unsorted[LEN_1], sorted[LEN_1];
    char *hash = NULL, phash[LEN_MD5 + 1];
    off_t offset = 0;
    char offstr[OFF_LEN + 1], temp[OFF_LEN + 1];
    int db_cnt = 0, idx_cnt = 0, ig_cnt = 0;


    /* Open the database file */
    if (NULL == (hIn = fopen(db_file, "r"))) {
	fprintf(stderr, "Error opening md5sum file: %s\n", db_file);
	exit(1);
    }

    snprintf(unsorted, LEN_1, "%s-md5-ns.idx", db_file);
    snprintf(sorted, LEN_1, "%s-md5.idx", db_file);

    /* Create temp unsorted file of offsets */
    if (NULL == (hOut = fopen(unsorted, "w"))) {
	fprintf(stderr, "Error creating md5sum index file: %s\n",
		unsorted);
	exit(1);
    }

    /* Print the header */
    fprintf(hOut, "%s|%s\n", STR_HEAD, STR_MD5SUM);

    /* Status */
    printf("Extracting Data from Database (%s)\n", db_file);

    /* Allocate a buffer for the previous hash value */
    memset(phash, '0', LEN_MD5 + 1);


    /* initialize the offset buffer */
    strcpy(offstr, "0000000000000000");

    /* read the file */
    for (i = 0; NULL != fgets(buf, LEN_1, hIn); offset += len, i++) {
	char *ptr;
	len = strlen(buf);

	if (md5sum_extract_md5(buf, &hash, NULL)) {
	    ig_cnt++;
	    continue;
	}
	db_cnt++;

	/* We only want to add one of each hash to the index */
	if (memcmp(hash, phash, LEN_MD5) == 0) {
	    continue;
	}

	/* Make the offset value as a constant width */
	snprintf(temp, OFF_LEN + 1, "%i", (int) offset);
	ptr = offstr + OFF_LEN - strlen(temp);
	strcpy(ptr, temp);

	/* Print the index file */
	fprintf(hOut, "%s|%s\n", hash, offstr);

	idx_cnt++;

	/* Set the previous has value */
	strncpy(phash, hash, LEN_MD5 + 1);
    }

    fclose(hIn);
    fclose(hOut);


    if (idx_cnt > 0) {

	char *root = "/bin/sort";
	char *usr = "/usr/bin/sort";
	char *local = "/usr/local/bin/sort";
	struct stat stats;

	printf("  Valid Database Entries: %d\n", db_cnt);
	printf("  Invalid Database Entries (headers or errors): %d\n",
	       ig_cnt);
	printf("  Index File Entries %s: %d\n",
	       (idx_cnt == db_cnt) ? "" : "(optimized)", idx_cnt);

	printf("Sorting Index (%s)\n", sorted);

	if (0 == stat(local, &stats)) {
	    snprintf(buf, LEN_1, "%s -o %s %s", local, sorted, unsorted);
	}
	else if (0 == stat(usr, &stats)) {
	    snprintf(buf, LEN_1, "%s -o \"%s\" \"%s\"",
		     usr, sorted, unsorted);
	}
	else if (0 == stat(root, &stats)) {
	    snprintf(buf, LEN_1, "%s -o \"%s\" \"%s\"",
		     root, sorted, unsorted);
	}
	else {
	    fprintf(stderr, "Error finding 'sort' binary\n");
	    exit(1);
	}

	if (0 != system(buf)) {
	    fprintf(stderr, "Error sorting index file using %s\n", buf);
	    exit(1);
	}
    }
    else {
	fprintf(stderr, "Error parsing md5sum DB File: %s\n", db_file);
    }

    if (0 != unlink(unsorted)) {
	fprintf(stderr, "Error removing temporary index file\n");
	exit(1);
    }

    return 0;
}


void
md5sum_print(char *db_file, char *hash, off_t offset, int flags)
{
    FILE *hFile;
    char buf[LEN_1], *name, *ptr = NULL, pname[LEN_1];
    int found = 0;

    if (strlen(hash) != LEN_MD5) {
	fprintf(stderr, "md5sum_print: Invalid hash value: %s\n", hash);
	exit(1);
    }

    /* Open the Database */
    if (NULL == (hFile = fopen(db_file, "r"))) {
	fprintf(stderr,
		"md5sum_print: Error opening md5sum DB file: %s\n",
		db_file);
	return;
    }

    memset(pname, '0', LEN_1);

    /* Loop so that we can find multiple occurances of the same hash */
    while (1) {
	int len;

	if (0 != fseek(hFile, offset, SEEK_SET)) {
	    fprintf(stderr,
		    "md5sum_print: Error seeking to get file name: %lu\n",
		    (unsigned long) offset);
	    exit(1);
	}

	if (NULL == fgets(buf, LEN_1, hFile)) {
	    if (feof(hFile)) {
		break;
	    }

	    fprintf(stderr, "md5sum_print: Error reading database\n");
	    exit(1);
	}

	len = strlen(buf);

	if (md5sum_extract_md5(buf, &ptr, &name)) {
	    fprintf(stderr, "md5sum_print: Invalid entry in database\n");
	    exit(1);
	}

	/* Is this the one that we want? */
	if (0 != strcasecmp(ptr, hash)) {
	    break;
	}

	if (strcmp(name, pname) != 0) {
	    printf("%s\t%s\n", hash, name);
	    found = 1;
	    strncpy(pname, name, LEN_1);
	}

	/* Advance to the next row */
	offset += len;
    }

    if (found == 0) {
	fprintf(stderr, "Hash not found in file at offset: %lu\n",
		(unsigned long) offset);
	exit(1);
    }

    fclose(hFile);
    return;
}
