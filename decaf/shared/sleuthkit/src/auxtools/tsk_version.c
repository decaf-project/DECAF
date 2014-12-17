/*
 * The Sleuth Kit
 * 
 * $Date: 2006-07-10 14:22:27 -0400 (Mon, 10 Jul 2006) $
 */
#include <stdlib.h>
#include <stdio.h>

void
print_version(FILE * hFile)
{
    char *str = "The Sleuth Kit";
#ifdef VER
    fprintf(hFile, "%s ver %s\n", str, VER);
#else
    fprintf(hFile, "%s\n", str);
#endif
    return;
}
