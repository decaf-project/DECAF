/*
** The Sleuth Kit 
**
** $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
**
** Brian Carrier [carrier@sleuthkit.org]
** Copyright (c) 2004-2005 Brian Carrier.  All rights reserved
*/

#ifndef _TSK_TYPES_H
#define _TSK_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

/* printf macros - if the OS doesnot have inttypes.h yet */

#ifndef PRIx64
#define PRIx64 "llx"
#endif

#ifndef PRIX64
#define PRIX64 "llX"
#endif

#ifndef PRIu64
#define PRIu64 "llu"
#endif

#ifndef PRId64
#define PRId64 "lld"
#endif

#ifndef PRIo64
#define PRIo64 "llo"
#endif

#ifndef PRIx32
#define PRIx32 "x"
#endif

#ifndef PRIX32
#define PRIX32 "X"
#endif

#ifndef PRIu32
#define PRIu32 "u"
#endif

#ifndef PRId32
#define PRId32 "d"
#endif

#ifndef PRIx16
#define PRIx16 "hx"
#endif

#ifndef PRIX16
#define PRIX16 "hX"
#endif

#ifndef PRIu16
#define PRIu16 "hu"
#endif

#ifndef PRIu8
#define PRIu8 "hhu"
#endif

#ifndef PRIx8
#define PRIx8 "hhx"
#endif



    typedef unsigned long ULONG;
    typedef unsigned long long ULLONG;
    typedef unsigned char UCHAR;



/* Standard local variable sizes */

// Metadata - inode number
    typedef uint64_t INUM_T;
#define PRIuINUM	PRIu64
#define PRIxINUM	PRIx64
#define PRIdINUM	PRId64

// Disk sector / block address
    typedef uint64_t DADDR_T;
#define PRIuDADDR   PRIu64
#define PRIxDADDR   PRIx64
#define PRIdDADDR   PRId64

// Byte offset
    typedef uint64_t OFF_T;
#define PRIuOFF		PRIu64
#define PRIxOFF		PRIx64
#define PRIdOFF		PRId64


    typedef int64_t SSIZE_T;
#define PRIuSSIZE		PRIu64
#define PRIxSSIZE		PRIx64
#define PRIdSSIZE		PRId64

// Partition Number
    typedef uint32_t PNUM_T;
#define PRIuPNUM	PRIu32
#define PRIxPNUM	PRIx32
#define PRIdPNUM	PRId32

#ifdef __cplusplus
}
#endif
#endif
