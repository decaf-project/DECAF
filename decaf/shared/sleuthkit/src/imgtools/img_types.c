/*
** img_types
** The Sleuth Kit 
**
** $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
**
** Identify the type of image file being used
**
** Brian Carrier [carrier@sleuthkit.org]
** Copyright (c) 2006 Brian Carrier.  All rights reserved 
**
** This software is distributed under the Common Public License 1.0
*/

#include "img_tools.h"

typedef struct {
    char *name;
    uint8_t code;
    char *comment;
} IMG_TYPES;

/* The table used to parse input strings 
 * - in order of expected usage
 */
IMG_TYPES img_open_table[] = {
    {"raw", RAW_SING, "raw (dd)"},
    {"aff", AFF_AFF, "Advanced Forensic Format"},
    {"afd", AFF_AFD, "AFF Multiple File"},
    {"afm", AFF_AFM, "AFF with external metadata"},
    {"ewf", EWF_EWF, "Expert Witness format (encase)"},
    {"split", RAW_SPLIT, "Split raw files"},
    {"qemu", QEMU_IMG, "QEMU image"},
    {0},
};


uint8_t
img_parse_type(const char *str)
{
    IMG_TYPES *sp;

    for (sp = img_open_table; sp->name; sp++) {
	if (strcmp(str, sp->name) == 0) {
	    return sp->code;
	}
    }
    return UNSUPP_IMG;
}


/* Used by the usage functions to display supported types */
void
img_print_types(FILE * hFile)
{
    IMG_TYPES *sp;
    fprintf(hFile, "Supported image format types:\n");
    for (sp = img_open_table; sp->name; sp++)
	fprintf(hFile, "\t%s (%s)\n", sp->name, sp->comment);
}

char *
img_get_type(uint8_t ftype)
{
    IMG_TYPES *sp;
    for (sp = img_open_table; sp->name; sp++)
	if (sp->code == ftype)
	    return sp->name;

    return NULL;
}
