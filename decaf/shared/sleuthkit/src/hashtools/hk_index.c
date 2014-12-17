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


/*
 * name can be NULL
 */
static int
hk_extract_md5(char *str, char **md5, char *name, int len,
	       char *other, int o_len)
{
    char *ptr = str;
    char *file = NULL, *dir = NULL, *file_id = NULL, *hash_id = NULL;
    int cnt = 0;

    if ((str == NULL) || (strlen(str) < LEN_MD5))
	return 1;

    if ((md5 == NULL) && (name == NULL) && (other == NULL))
	return 0;

    /*
     * 0 file_id
     * 1 hashset_id
     * 2 file_name
     * 3 directory
     * 4 hash
     * 5 file_size
     * 6 date_modified
     * 7 time modified
     * 8 time_zone
     * 9 comments
     * 10 date_accessed
     * 11 time_accessed
     */

    /* Assign the file_id if we are looking for it */
    if (other != NULL) {
	file_id = ptr;
    }

    while (NULL != (ptr = strchr(ptr, ','))) {
	cnt++;

	/* End of file_id, begin hash_id */
	if ((cnt == 1) && (other != NULL)) {
	    *ptr = '\0';
	    ptr++;
	    hash_id = ptr;

	}
	/* End of hash_id, begin name */
	else if (cnt == 2) {

	    /* Finish the 'other' stuff */
	    if (other != NULL) {
		*ptr = '\0';
		snprintf(other, o_len, "Hash ID: %s  File ID: %s",
			 hash_id, file_id);
	    }

	    /* Are we done? */
	    if ((name == NULL) && (md5 == NULL))
		return 0;

	    /* get the pointer to the name */
	    if (name) {
		if (ptr[1] != '"')
		    return 1;

		file = &ptr[2];
		/* We utilize the other loop code to find the end of
		 * the name */
	    }
	}
	/* end of the name, begin directory - which may not exist */
	else if ((cnt == 3) && (name != NULL)) {

	    /* finish up the name */
	    if (ptr[-1] != '"')
		return 1;

	    ptr[-1] = '\0';

	    /* get the directory start, if it exists */
	    if (ptr[1] == '"') {
		dir = &ptr[2];
	    }
	    else {
		dir = NULL;
	    }
	}
	/* end of directory, begin MD5 value */
	else if (cnt == 4) {

	    /* Copy the name into the buffer */
	    if (name != NULL) {
		name[0] = '\0';
		if (dir) {
		    /* finish up the dir */
		    if (ptr[-1] != '"')
			return 1;

		    ptr[-1] = '\0';

		    strncpy(name, dir, len);
		    strncat(name, "\\", len);
		}
		if (file) {
		    strncat(name, file, len);
		}
		else {
		    return 1;
		}
	    }

	    if (md5 == NULL)
		return 0;

	    /* Do a length check and more sanity checks */
	    if ((strlen(ptr) < 2 + LEN_MD5) || (ptr[1] != '"') ||
		(ptr[2 + LEN_MD5] != '"')) {
		return 1;
	    }

	    ptr = &ptr[2];
	    ptr[LEN_MD5] = '\0';

	    *md5 = ptr;

	    /* Final sanity check */
	    if (NULL != strchr(ptr, ',')) {
		return 1;
	    }

	    return 0;
	}

	/* If the next field is in quotes then we need to skip to the
	 * next quote and ignore any ',' in there
	 */
	if (ptr[1] == '"') {
	    if (NULL == (ptr = strchr(&ptr[2], '"'))) {
		return 1;
	    }
	}
	else {
	    ptr++;
	}
    }

    return 1;
}


/*
 * Create the index file 
 */
int
hk_init(char *db_file)
{
    FILE *hIn, *hOut;
    int i, len = 0;
    char buf[LEN_1], unsorted[LEN_1], sorted[LEN_1];
    char *hash = NULL, phash[LEN_MD5 + 1];
    off_t offset = 0;
    char offstr[OFF_LEN + 1], temp[OFF_LEN + 1];
    int db_cnt = 0, idx_cnt = 0, ig_cnt = 0;


    /* Open the database file */
    if (NULL == (hIn = fopen(db_file, "r"))) {
	fprintf(stderr, "Error opening hash keeper file: %s\n", db_file);
	exit(1);
    }

    snprintf(unsorted, LEN_1, "%s-md5-ns.idx", db_file);
    snprintf(sorted, LEN_1, "%s-md5.idx", db_file);

    /* Create temp unsorted file of offsets */
    if (NULL == (hOut = fopen(unsorted, "w"))) {
	fprintf(stderr, "Error creating hash keeper index file: %s\n",
		unsorted);
	exit(1);
    }

    /* Print the header */
    fprintf(hOut, "%s|%s\n", STR_HEAD, STR_HK);

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

	if (hk_extract_md5(buf, &hash, NULL, 0, NULL, 0)) {
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
	fprintf(stderr, "Error parsing hash keeper DB File: %s\n",
		db_file);
    }

    if (0 != unlink(unsorted)) {
	fprintf(stderr, "Error removing temporary index file\n");
	exit(1);
    }

    return 0;
}


void
hk_print(char *db_file, char *hash, off_t offset, int flags)
{
    FILE *hFile;
    char buf[LEN_1], name[LEN_1], *ptr = NULL, pname[LEN_1], other[LEN_1];
    int found = 0;

    if (strlen(hash) != LEN_MD5) {
	fprintf(stderr, "hk_print: Invalid hash value: %s\n", hash);
	exit(1);
    }

    /* Open the Database */
    if (NULL == (hFile = fopen(db_file, "r"))) {
	fprintf(stderr,
		"hk_print: Error opening hash keeper DB file: %s\n",
		db_file);
	return;
    }

    memset(pname, '0', LEN_1);

    /* Loop so that we can find multiple occurances of the same hash */
    while (1) {
	int len;

	if (0 != fseek(hFile, offset, SEEK_SET)) {
	    fprintf(stderr,
		    "hk_print: Error seeking to get file name: %lu\n",
		    (unsigned long) offset);
	    exit(1);
	}

	if (NULL == fgets(buf, LEN_1, hFile)) {
	    if (feof(hFile)) {
		break;
	    }

	    fprintf(stderr, "hk_print: Error reading database\n");
	    exit(1);
	}

	len = strlen(buf);

	if (hk_extract_md5(buf, &ptr, name, LEN_1,
			   ((flags & FLAG_EXT) ? other : NULL),
			   ((flags & FLAG_EXT) ? LEN_1 : 0))) {
	    fprintf(stderr, "hk_print: Invalid entry in database: %s\n",
		    buf);
	    exit(1);
	}

	/* Is this the one that we want? */
	if (0 != strcasecmp(ptr, hash)) {
	    break;
	}

	if (strcmp(name, pname) != 0) {
	    if (flags & FLAG_EXT)
		printf("%s\t%s\t(%s)\n", hash, name, other);
	    else
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
