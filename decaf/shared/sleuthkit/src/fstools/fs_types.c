/*
** fs_types
** The Sleuth Kit 
**
** $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
**
** Identify the type of file system being used
**
** Brian Carrier [carrier@sleuthkit.org]
** Copyright (c) 2006 Brian Carrier, Basis Technology.  All Rights reserved
** Copyright (c) 2003-2005 Brian Carrier.  All rights reserved 
**
** TASK
** Copyright (c) 2002 Brian Carrier, @stake Inc.  All rights reserved
**
**
** This software is distributed under the Common Public License 1.0
**
*/

#include "fs_tools.h"

/* Based on fs_open.c in TCT-1.07 */

typedef struct {
    char *name;
    uint8_t code;
    char *comment;
} FS_TYPES;

/* The table used to parse input strings - supports
 * legacy strings - in order of expected usage
 */
FS_TYPES fs_open_table[] = {
    {"ntfs", NTFS, "NTFS"},
    {"fat", FATAUTO, "auto-detect FAT"},
    {"ext", EXTAUTO, "Ext2/Ext3"},
    {"iso9660", ISO9660, "ISO9660 CD"},
//    {"hfs", HFS, "HFS+"},
    {"ufs", FFSAUTO, "UFS 1 & 2"},
    {"raw", RAW, "Raw Data"},
    {"swap", SWAP, "Swap Space"},
    {"fat12", FAT12, "FAT12"},
    {"fat16", FAT16, "FAT16"},
    {"fat32", FAT32, "FAT32"},
    {"linux-ext", EXTAUTO, "auto-detect Linux EXTxFS"},
    {"linux-ext2", EXT2FS, "Linux EXT2FS"},
    {"linux-ext3", EXT3FS, "Linux EXT3FS"},
    {"bsdi", FFS_1, "BSDi FFS"},
    {"freebsd", FFS_1, "FreeBSD FFS"},
    {"netbsd", FFS_1, "NetBSD FFS"},
    {"openbsd", FFS_1, "OpenBSD FFS"},
    {"solaris", FFS_1B, "Solaris FFS"},
    {0},
};

/* Used to print the name given the code */
FS_TYPES fs_test_table[] = {
    {"ntfs", NTFS, ""},
    {"fat", FATAUTO, ""},
    {"ext", EXTAUTO, ""},
    {"ufs", FFSAUTO, ""},
//    {"hfs", HFS, ""},
    {"iso9660", ISO9660, ""},
    {"raw", RAW, ""},
    {"swap", SWAP, ""},
    {"fat12", FAT12, ""},
    {"fat16", FAT16, ""},
    {"fat32", FAT32, ""},
    {"linux-ext2", EXT2FS, ""},
    {"linux-ext3", EXT3FS, ""},
    {"ufs", FFS_1, ""},
    {"ufs", FFS_1B, ""},
    {"ufs", FFS_2, ""},
    {0},
};

FS_TYPES fs_usage_table[] = {
    {"ext", 0, "Ext2/Ext3"},
    {"fat", 0, "FAT12/16/32"},
    {"ntfs", 0, "NTFS"},
    //{"hfs", 0, "HFS+"},
    {"iso9660", 0, "ISO9660 CD"},
    {"ufs", 0, "UFS 1 & 2"},
    {"raw", 0, "Raw Data"},
    {"swap", 0, "Swap Space"},
    {0},
};


uint8_t
fs_parse_type(const char *str)
{
    FS_TYPES *sp;

    for (sp = fs_open_table; sp->name; sp++) {
	if (strcmp(str, sp->name) == 0) {
	    return sp->code;
	}
    }
    return UNSUPP_FS;
}


/* Used by the usage functions to display supported types */
void
fs_print_types(FILE * hFile)
{
    FS_TYPES *sp;
    fprintf(hFile, "Supported file system types:\n");
    for (sp = fs_usage_table; sp->name; sp++)
	fprintf(hFile, "\t%s (%s)\n", sp->name, sp->comment);
}

char *
fs_get_type(uint8_t ftype)
{
    FS_TYPES *sp;
    for (sp = fs_test_table; sp->name; sp++)
	if (sp->code == ftype)
	    return sp->name;

    return NULL;
}
