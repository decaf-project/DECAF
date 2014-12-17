/*
 * libewf segment table
 *
 * Copyright (c) 2006, Joachim Metz <forensics@hoffmannbv.nl>,
 * Hoffmann Investigations. All rights reserved.
 *
 * This code is derrived from information and software contributed by
 * - Expert Witness Compression Format specification by Andrew Rosen
 *   (http://www.arsdata.com/SMART/whitepaper.html)
 * - libevf from PyFlag by Michael Cohen
 *   (http://pyflag.sourceforge.net/)
 * - Open SSL for the implementation of the MD5 hash algorithm
 * - Wietse Venema for error handling code
 *
 * Additional credits go to
 * - Robert Jan Mora for testing and other contribution
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of the creator, related organisations, nor the names of
 *   its contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 * - All advertising materials mentioning features or use of this software
 *   must acknowledge the contribution by people stated above.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER, COMPANY AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _LIBEWF_SEGMENTTABLE_H
#define _LIBEWF_SEGMENTTABLE_H

#include "definitions.h"

#include "section_list.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct libewf_segment_table LIBEWF_SEGMENT_TABLE;

struct libewf_segment_table
{
	/* The amount of segments in the table
	 * consists of 2 bytes (16 bits) containing
	 */
	uint32_t amount;

	/* A dynamic array containting the filenames
	 */
	char **filename;

	/* A dynamic array containting the file descriptors
	 */
	int *file_descriptor;

        /* A list of all the sections within a certain segment file
         */
        LIBEWF_SECTION_LIST **section_list;
};

#define LIBEWF_SEGMENT_TABLE_SIZE sizeof( LIBEWF_SEGMENT_TABLE )
#define LIBEWF_SEGMENT_TABLE_FILENAME_SIZE sizeof( char* )
#define LIBEWF_SEGMENT_TABLE_FILE_DESCRIPTOR_SIZE sizeof( int )
#define LIBEWF_SEGMENT_TABLE_SECTION_LIST_SIZE sizeof( LIBEWF_SECTION_LIST )

LIBEWF_SEGMENT_TABLE *libewf_segment_table_alloc( uint32_t initial_table_size );
LIBEWF_SEGMENT_TABLE *libewf_segment_table_values_alloc( LIBEWF_SEGMENT_TABLE *segment_table, uint32_t size );
LIBEWF_SEGMENT_TABLE *libewf_segment_table_values_realloc( LIBEWF_SEGMENT_TABLE *segment_table, uint32_t size );
void libewf_segment_table_free( LIBEWF_SEGMENT_TABLE *segment_table );
LIBEWF_SEGMENT_TABLE *libewf_segment_table_set_values( LIBEWF_SEGMENT_TABLE *segment_table, uint32_t segment, const char *filename, int file_descriptor );
uint8_t libewf_segment_table_values_is_set( LIBEWF_SEGMENT_TABLE *segment_table, uint32_t segment );
char *libewf_segment_table_get_filename( LIBEWF_SEGMENT_TABLE *segment_table, uint32_t segment );
int libewf_segment_table_get_file_descriptor( LIBEWF_SEGMENT_TABLE *segment_table, uint32_t segment );

#ifdef __cplusplus
}
#endif

#endif

