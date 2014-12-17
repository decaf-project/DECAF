/*
 * The Sleuth Kit
 *
 * $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
 *
 * Brian Carrier [carrier@sleuthkit.org]
 * Copyright (c) 2003 Brian Carrier.  All rights reserved
 */

#ifndef _HFIND_H
#define _HFIND_H

#ifdef __cplusplus
extern "C"
{
#endif 

#define LEN_1	512

#define OFF_LEN 16		/* Size of the offset field */

/* Global Flags */
#define FLAG_QUICK  0x01	// only print 0 or 1 if the hash is found
#define FLAG_EXT	0x02	// print other details besides the name

/* Hash Flags */
#define HASH_MD5	0x01
#define HASH_SHA1	0x02

#define STR_MD5		"md5"
#define STR_SHA1	"sha1"

/* one longer than an sha-1 hash - so that it always sorts to the top */
#define STR_HEAD	"00000000000000000000000000000000000000000"


#define HASH_STR(x) \
    ( ((x) & HASH_MD5) ? (STR_MD5) : ( \
	( ((x) & HASH_SHA1) ? STR_SHA1 : "") ) )


#define LEN_SHA1 40
#define LEN_MD5 32
#define LEN_CRC32 8

#define HASH_LEN(x) \
    ( ((x) & HASH_MD5) ? (LEN_MD5) : ( \
	( ((x) & HASH_SHA1) ? LEN_SHA1 : 0) ) )


/* Length of an index file line - 2 for comma and newline */
#define IDX_LEN(x) \
    ( HASH_LEN(x) + OFF_LEN + 2)

/* DB Type Values */
#define TYPE_NSRL		1
#define TYPE_MD5SUM		2
#define TYPE_HK			3

/* String versions of DB types */
#define STR_NSRL		"nsrl"
#define STR_NSRL_MD5	"nsrl-md5"
#define STR_NSRL_SHA1	"nsrl-sha1"
#define STR_MD5SUM		"md5sum"
#define STR_HK			"hk"
#define STR_SUPPORT		"nsrl-md5, nsrl-sha1, md5sum, hk"

/* Error Messages */
#define MSG_NOTFOUND	"Hash Not Found"
#define MSG_INV_HASH	"Invalid Hash Value"


/* Functions */
extern int tm_init(char *, char *);
extern int tm_lookup(char *, char *, unsigned char);

extern int nsrl_init(char *, unsigned char);
extern void nsrl_print(char *, char *, off_t, int);

extern int md5sum_init(char *);
extern void md5sum_print(char *, char *, off_t, int);

extern int hk_init(char *);
extern void hk_print(char *, char *, off_t, int);

#ifdef __cplusplus
}
#endif

#endif
