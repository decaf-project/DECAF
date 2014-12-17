/*
** fs_types
** The Sleuth Kit 
**
** $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
**
** Brian Carrier [carrier@sleuthkit.org]
** Copyright (c) 2003-2005 Brian Carrier.  All rights reserved
**
** TASK
** Copyright (c) 2002 @stake Inc.  All rights reserved
*/

#ifndef _FS_TYPES_H
#define _FS_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

    extern uint8_t fs_parse_type(const char *);
    extern void fs_print_types(FILE *);
    extern char *fs_get_type(uint8_t);

/*
** the most-sig-nibble is the file system type, which indictates which
** _open function to call.  The least-sig-nibble is the specific type
** of implementation.  
*/
#define FSMASK			0xf0
#define OSMASK			0x0f

#define UNSUPP_FS		0x00

/* FFS */
#define FFS_TYPE		0x10

#define FFS_1			0x11	/* UFS1 - FreeBSD, OpenBSD, BSDI ... */
#define FFS_1B			0x12	/* Solaris (no type) */
#define FFS_2			0x13	/* UFS2 - FreeBSD, NetBSD */
#define FFSAUTO			0x14


#define	EXTxFS_TYPE		0x20
#define EXT2FS			0x21
#define EXT3FS			0x22
#define EXTAUTO			0x23

/* FAT */
#define FATFS_TYPE		0x30

#define FAT12		0x31
#define FAT16		0x32
#define FAT32		0x33
#define FATAUTO		0x34

#define NTFS_TYPE		0x40
#define NTFS			0x40

#define	SWAPFS_TYPE		0x50
#define	SWAP			0x50

#define	RAWFS_TYPE		0x60
#define	RAW				0x60


#define ISO9660_TYPE		0x70
#define ISO9660			0x70

#define HFS_TYPE		0x80
#define HFS			0x80

#ifdef __cplusplus
}
#endif
#endif
