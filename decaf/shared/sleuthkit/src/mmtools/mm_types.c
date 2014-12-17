/*
 * The Sleuth Kit
 *
 * $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
 *
 * Brian Carrier [carrier@sleuthkit.org]
 * Copyright (c) 2003-2005 Brian Carrier.  All rights reserved
 *
 * mm_types - set the type value given a string of partition type
 *
 *
 * This software is distributed under the Common Public License 1.0
 *
 */

#include "mm_tools.h"

typedef struct {
    char *name;
    char code;
    char *comment;
} MM_TYPES;


MM_TYPES mm_open_table[] = {
    {"dos", MM_DOS, "DOS-based partitions [Windows, Linux, etc.]"},
    {"mac", MM_MAC, "MAC partitions"},
    {"bsd", MM_BSD, "BSD Disklabels [FreeBSD, OpenBSD, NetBSD]"},
    {"sun", MM_SUN, "Sun Volume Table of Contents (Solaris)"},
    {"gpt", MM_GPT, "GUID Partition Table (EFI)"},
    {0},
};

/* parse the string and return the value 
 *
 * Returns MM_UNSUPP if string cannot be parsed
 * */
char
mm_parse_type(const char *str)
{
    MM_TYPES *types;

    for (types = mm_open_table; types->name; types++) {
	if (strcmp(str, types->name) == 0) {
	    return types->code;
	}
    }
    return MM_UNSUPP;
}

void
mm_print_types(FILE * hFile)
{
    MM_TYPES *types;
    fprintf(hFile, "Supported partition types:\n");
    for (types = mm_open_table; types->name; types++)
	fprintf(hFile, "\t%s (%s)\n", types->name, types->comment);
}


/*
 * Return the string name of a partition type given its code
 *
 * Returns NULL if a match is not made
 * */
char *
mm_get_type(char mmtype)
{
    MM_TYPES *types;
    for (types = mm_open_table; types->name; types++)
	if (types->code == mmtype)
	    return types->name;

    return NULL;
}
