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
#include "hfind.h"

#define NSRL_FORM1		1
#define NSRL_FORM2		2

static int
get_format_ver(char *str)
{

/*
 "SHA-1","FileName","FileSize","ProductCode","OpSystemCode","MD4","MD5","CRC32","SpecialCode"
*/
    if ((str[9] == 'F') && (str[20] == 'F') && (str[24] == 'S') &&
	(str[31] == 'P') && (str[45] == 'O'))
	return NSRL_FORM1;

/*
"SHA-1","MD5","CRC32","FileName","FileSize","ProductCode","OpSystemCode","Specia
lCode"
*/
    else if ((str[9] == 'M') && (str[15] == 'C') && (str[23] == 'F') &&
	     (str[34] == 'F') && (str[45] == 'P'))
	return NSRL_FORM2;


    fprintf(stderr, "nsrl: Unknown header format: %s\n", str);
    exit(1);
}


/* make sure it is an SHA1 value with a " before and after */
#define is_valid_nsrl(x) \
	( (strlen((x)) > LEN_SHA1 + 4) && \
	((x)[0] == '"') && ((x)[LEN_SHA1 + 1] == '"') && \
	((x)[LEN_SHA1 + 2] == ',') && ((x)[LEN_SHA1 + 3] == '"') )


static int
nsrl_extract_sha1(char *str, char **sha1, char **name, int ver)
{
    char *ptr = NULL;

    /* Sanity check */
    if (is_valid_nsrl(str) == 0) {
	return 1;
    }

    /* Do they want the hash? */
    if (sha1 != NULL) {
	/* set the hash pointer to just the SHA value (past the ") */
	ptr = &str[1];
	ptr[LEN_SHA1] = '\0';

	/* Final sanity check to make sure there are no ',' in hash */
	if (NULL != strchr(ptr, ',')) {
	    return 1;
	}

	/* Assign the argument if it is not NULL */
	*sha1 = ptr;
    }

    /* Do they want the name ? */
    if (name != NULL) {
	if (ver == NSRL_FORM1) {
	    /* Extract out the name  - the field after SHA1 */
	    ptr = &str[LEN_SHA1 + 4];	// 4 = 3 " and 1 ,
	    *name = ptr;

	    if (NULL == (ptr = strchr(ptr, ','))) {
		return 1;
	    }

	    /* Seek back to cover the final " */
	    ptr[-1] = '\0';
	}
	else if (ver == NSRL_FORM2) {
	    /* Extract out the name  - the field after SHA1, MD5, and CRC */
	    ptr = &str[1 + LEN_SHA1 + 3 + LEN_MD5 + 3 + LEN_CRC32 + 3];
	    *name = ptr;

	    if (NULL == (ptr = strchr(ptr, ','))) {
		return 1;
	    }

	    /* Seek back to cover the final " */
	    ptr[-1] = '\0';
	}
    }

    return 0;
}

static int
nsrl_extract_md5(char *str, char **md5, char **name, int ver)
{
    char *ptr = NULL;
    int cnt = 0;

    /* Sanity check */
    if (is_valid_nsrl(str) == 0) {
	return 1;
    }

    if ((md5 == NULL) && (name == NULL))
	return 0;

    if (ver == NSRL_FORM1) {
	ptr = str;

	/* Cycle through the fields to extract name and MD5
	 *
	 * 1. before name
	 * 2. before size
	 * 3. before prod code
	 * 4. before OS
	 * 5. before MD4
	 * 6. before MD5
	 */
	cnt = 0;
	while (NULL != (ptr = strchr(ptr, ','))) {
	    cnt++;

	    /* Begining of the name */
	    if ((cnt == 1) && (name != NULL)) {
		*name = &ptr[2];
		/* We utilize the other loop code to find the end of
		 * the name */
	    }
	    /* end of the name */
	    else if ((cnt == 2) && (name != NULL)) {
		if (ptr[-1] != '"')
		    return 1;

		ptr[-1] = '\0';

		if (md5 == NULL)
		    return 0;
	    }
	    /* MD5 value */
	    else if ((cnt == 6) && (md5 != NULL)) {
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
    }
    else if (ver == NSRL_FORM2) {
	/* Do they want the hash? */
	if (md5 != NULL) {
	    /* set the hash pointer to just the MD5 value (past the SHA1") */
	    ptr = &str[1 + LEN_SHA1 + 3];
	    ptr[LEN_MD5] = '\0';

	    /* Final sanity check to make sure there are no ',' in hash */
	    if (NULL != strchr(ptr, ',')) {
		return 1;
	    }

	    *md5 = ptr;
	}

	/* do they want the name */
	if (name != NULL) {
	    /* Extract out the name  - the field after SHA1, MD5, and CRC */
	    ptr = &str[1 + LEN_SHA1 + 3 + LEN_MD5 + 3 + LEN_CRC32 + 3];
	    *name = ptr;

	    if (NULL == (ptr = strchr(ptr, ','))) {
		return 1;
	    }

	    /* Seek back to cover the final " */
	    ptr -= 1;
	    *ptr = '\0';

	}
	return 0;
    }

    return 1;
}




/*
 * Create the index file 
 */
int
nsrl_init(char *db_file, unsigned char htype)
{
    FILE *hIn, *hOut;
    int i, len;
    char buf[LEN_1], unsorted[LEN_1], sorted[LEN_1];
    char *hash = NULL, *phash;
    char *hash_str;
    off_t offset = 0;
    char offstr[OFF_LEN + 1], temp[OFF_LEN + 1];
    int hash_len, ver = 0;
    int db_cnt = 0, idx_cnt = 0, ig_cnt = 0;

    /* Verify that at least one valid algo was given */
    if (htype & HASH_MD5) {
	hash_str = STR_MD5;
    }
    else if (htype & HASH_SHA1) {
	hash_str = STR_SHA1;
    }
    else {
	fprintf(stderr,
		"nsrl_init: At least one hashing algorithm must be given\n");
	exit(1);
    }

    hash_len = HASH_LEN(htype);

    /* Open the database file */
    if (NULL == (hIn = fopen(db_file, "r"))) {
	fprintf(stderr, "Error opening NSRL file: %s\n", db_file);
	exit(1);
    }

    snprintf(unsorted, LEN_1, "%s-%s-ns.idx", db_file, hash_str);
    snprintf(sorted, LEN_1, "%s-%s.idx", db_file, hash_str);

    /* Create temp unsorted file of offsets */
    if (NULL == (hOut = fopen(unsorted, "w"))) {
	fprintf(stderr, "Error creating NSRL index file: %s\n", unsorted);
	exit(1);
    }

    /* Print the header */
    fprintf(hOut, "%s|%s\n", STR_HEAD, STR_NSRL);

    /* Status */
    printf("Extracting Data from Database (%s)\n", db_file);

    /* Allocate a buffer for the previous hash value */
    phash = malloc(hash_len + 1);
    if (phash == NULL) {
	fprintf(stderr, "nsrl_init: Error allocating memory\n");
	exit(1);
    }
    memset(phash, '0', hash_len + 1);


    /* initialize the offset buffer */
    strcpy(offstr, "0000000000000000");

    /* read the file */
    for (i = 0; NULL != fgets(buf, LEN_1, hIn); offset += len, i++) {
	char *ptr;
	len = strlen(buf);

	if (i == 0) {
	    ver = get_format_ver(buf);
	    ig_cnt++;
	    continue;
	}

	if (htype & HASH_SHA1) {
	    if (nsrl_extract_sha1(buf, &hash, NULL, ver)) {
		ig_cnt++;
		continue;
	    }
	}
	else if (htype & HASH_MD5) {
	    if (nsrl_extract_md5(buf, &hash, NULL, ver)) {
		ig_cnt++;
		continue;
	    }
	}

	db_cnt++;

	/* We only want to add one of each hash to the index */
	if (memcmp(hash, phash, hash_len) == 0) {
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
	strncpy(phash, hash, hash_len + 1);
    }

    free(phash);

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
	    snprintf(buf, LEN_1, "%s -o \"%s\" \"%s\"",
		     local, sorted, unsorted);
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
	fprintf(stderr, "Error parsing NSRL DB File: %s\n", db_file);
    }

    if (0 != unlink(unsorted)) {
	fprintf(stderr, "Error removing temporary index file\n");
	exit(1);
    }

    return 0;
}


/*
 * Using the offset determined in the index file, get the name from
 * the original file.  The hash value that is passed is used to verify
 * the entry and find additional entries with the same hash value
 */
void
nsrl_print(char *db_file, char *hash, off_t offset, int flags)
{
    FILE *hFile;
    char buf[LEN_1], *name, *cur_hash, pname[LEN_1];
    int found = 0;
    int hash_len, ver;

    hash_len = strlen(hash);
    if ((hash_len != LEN_SHA1) && (hash_len != LEN_MD5)) {
	fprintf(stderr, "nsrl_print: Invalid hash value: %s\n", hash);
	exit(1);
    }

    /* Open the Database */
    if (NULL == (hFile = fopen(db_file, "r"))) {
	fprintf(stderr,
		"nsrl_print: Error opening NSRL DB file: %s\n", db_file);
	return;
    }

    if (NULL == fgets(buf, LEN_1, hFile)) {
	fprintf(stderr, "nsrl_init: Error reading NSRLFile.txt\n");
	exit(1);
    }

    ver = get_format_ver(buf);


    memset(pname, '0', LEN_1);

    /* Loop so that we can find multiple occurances of the same SHA */
    while (1) {
	int len;

	if (0 != fseek(hFile, offset, SEEK_SET)) {
	    fprintf(stderr,
		    "nsrl_print: Error seeking to get file name: %lu\n",
		    (unsigned long) offset);
	    exit(1);
	}

	if (NULL == fgets(buf, LEN_1, hFile)) {
	    if (feof(hFile))
		break;

	    fprintf(stderr, "nsrl_print: Error reading database\n");
	    exit(1);
	}

	len = strlen(buf);

	if (len < LEN_SHA1 + 5) {
	    fprintf(stderr,
		    "nsrl_print: Invalid entry in database (too short)\n");
	    exit(1);
	}

	/* Which field are we looking for */
	if (hash_len == LEN_SHA1) {
	    if (nsrl_extract_sha1(buf, &cur_hash, &name, ver)) {
		fprintf(stderr, "nsrl_print: Invalid entry in database\n");
		exit(1);
	    }
	}
	else if (hash_len == LEN_MD5) {
	    if (nsrl_extract_md5(buf, &cur_hash, &name, ver)) {
		fprintf(stderr, "nsrl_print: Invalid entry in database\n");
		exit(1);
	    }
	}


	/* Is this the one that we want? */
	if (0 != strcasecmp(cur_hash, hash)) {
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
