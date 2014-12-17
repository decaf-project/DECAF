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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "definitions.h"

#include "notify.h"

#include "section_list.h"
#include "segment_table.h"

/* Allocates memory for a new file list struct
 */
LIBEWF_SEGMENT_TABLE *libewf_segment_table_alloc( uint32_t initial_table_size )
{
	LIBEWF_SEGMENT_TABLE *segment_table = (LIBEWF_SEGMENT_TABLE *) malloc( LIBEWF_SEGMENT_TABLE_SIZE );

	if( segment_table == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_segment_table_alloc: unable to allocate segment_table\n" );
	}
	/* Segment has an offset of 1 so an additional values entry is needed
	 */
	segment_table = libewf_segment_table_values_alloc( segment_table, initial_table_size );

	return( segment_table );
}

/* Allocates memory for the dynamic filename and file descriptor array
 */
LIBEWF_SEGMENT_TABLE *libewf_segment_table_values_alloc( LIBEWF_SEGMENT_TABLE *segment_table, uint32_t size )
{
	uint32_t iterator = 0;

	if( segment_table == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_segment_table_values_alloc: invalid segment_table\n" );
	}
	segment_table->filename = (char **) malloc( size * LIBEWF_SEGMENT_TABLE_FILENAME_SIZE );

	if( segment_table->filename == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_segment_table_values_alloc: unable to allocate dynamic filename array\n" );
	}
	memset( segment_table->filename, (int) NULL, ( size * LIBEWF_SEGMENT_TABLE_FILENAME_SIZE ) );

	segment_table->file_descriptor = (int *) malloc( size * LIBEWF_SEGMENT_TABLE_FILE_DESCRIPTOR_SIZE );

	if( segment_table->file_descriptor == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_segment_table_values_alloc: unable to allocate dynamic file descriptor array\n" );
	}
	memset( segment_table->file_descriptor, -1, ( size * LIBEWF_SEGMENT_TABLE_FILE_DESCRIPTOR_SIZE ) );

	segment_table->section_list = (LIBEWF_SECTION_LIST **) malloc( size * LIBEWF_SEGMENT_TABLE_SECTION_LIST_SIZE );

	if( segment_table->section_list == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_segment_table_values_alloc: unable to allocate dynamic section list array\n" );
	}
	memset( segment_table->section_list, (int) NULL, ( size * LIBEWF_SEGMENT_TABLE_SECTION_LIST_SIZE ) );

	for( iterator = 0; iterator < size; iterator++ )
	{
		if( segment_table->section_list[ iterator ] == NULL )
		{
			segment_table->section_list[ iterator ] = libewf_section_list_alloc();
		}
	}
	segment_table->amount = size;

	return( segment_table );
}

/* Reallocates memory for the dynamic filename and file descriptor array
 */
LIBEWF_SEGMENT_TABLE *libewf_segment_table_values_realloc( LIBEWF_SEGMENT_TABLE *segment_table, uint32_t size )
{
	uint32_t offset;
	uint32_t iterator;

	if( segment_table == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_segment_table_values_realloc: invalid segment_table\n" );
	}
	offset = segment_table->amount;

	segment_table->filename = (char **) realloc( segment_table->filename, size * LIBEWF_SEGMENT_TABLE_FILENAME_SIZE );

	if( segment_table->filename == NULL )
	{
		LIBEWF_FATAL_PRINT( "segment_table_values_alloc: unable to allocate dynamic filename array\n" );
	}
	memset( ( segment_table->filename + offset ), (int) NULL, ( ( size - offset ) * LIBEWF_SEGMENT_TABLE_FILENAME_SIZE ) );

	segment_table->file_descriptor = (int *) realloc( segment_table->file_descriptor, size * LIBEWF_SEGMENT_TABLE_FILE_DESCRIPTOR_SIZE );

	if( segment_table->file_descriptor == NULL )
	{
		LIBEWF_FATAL_PRINT( "segment_table_file_descriptor_alloc: unable to allocate dynamic file descriptor array\n" );
	}
	memset( ( segment_table->file_descriptor + offset ), -1, ( ( size - offset ) * LIBEWF_SEGMENT_TABLE_FILE_DESCRIPTOR_SIZE ) );

	segment_table->section_list = (LIBEWF_SECTION_LIST **) realloc( segment_table->section_list, size * LIBEWF_SEGMENT_TABLE_SECTION_LIST_SIZE );

	if( segment_table->section_list == NULL )
	{
		LIBEWF_FATAL_PRINT( "segment_table_file_descriptor_alloc: unable to allocate dynamic section list array\n" );
	}
	memset( ( segment_table->section_list + offset ), (int) NULL, ( ( size - offset ) * LIBEWF_SEGMENT_TABLE_SECTION_LIST_SIZE ) );

	for( iterator = offset; iterator < size; iterator++ )
	{
		if( segment_table->section_list[ iterator ] == NULL )
		{
			segment_table->section_list[ iterator ] = libewf_section_list_alloc();
		}
	}
	segment_table->amount = size;

	return( segment_table );
}

/* Frees memory of a file list struct including elements
 */
void libewf_segment_table_free( LIBEWF_SEGMENT_TABLE *segment_table )
{
	uint64_t iterator;

	if( segment_table == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_segment_table_free: invalid segment_table\n" );
	}
	for( iterator = 0; iterator < segment_table->amount; iterator++ )
	{
		if( segment_table->filename[ iterator ] != NULL )
		{
			free( segment_table->filename[ iterator ] );
		}
		if( segment_table->section_list[ iterator ] != NULL )
		{
			libewf_section_list_free( segment_table->section_list[ iterator ] );
		}
	}
	free( segment_table->filename );
	free( segment_table->file_descriptor );
	free( segment_table );
}

/* Sets the values for a specific segment
 */
LIBEWF_SEGMENT_TABLE *libewf_segment_table_set_values( LIBEWF_SEGMENT_TABLE *segment_table, uint32_t segment, const char *filename, int file_descriptor )
{
	size_t filename_size;

	if( segment_table == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_segment_table_set_values: invalid segment_table\n" );
	}
	/* Check if additional entries should be allocated
	 */
	if( segment > segment_table->amount )
	{
		LIBEWF_VERBOSE_PRINT( "libewf_segment_table_set_values: allocating additional segment_table entries\n" );

		/* Segment has an offset of 1 so an additional values entry is needed
		 */
		segment_table = libewf_segment_table_values_realloc( segment_table, ( segment + 1 ) );
	}
	if( segment_table->filename[ segment ] != NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_segment_table_set_values: duplicate segments not supported: segment %d in %s was already specified in %s.\n", segment, filename, segment_table->filename[ segment ] );
	}
	filename_size = strlen( filename );

	/* One additional byte for the end of string character is needed
	 */
	segment_table->filename[ segment ] = (char *) malloc( sizeof( char ) * ( filename_size + 1 ) );

	if( segment_table->filename[ segment ] == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_segment_table_set_values: unable to allocate filename\n" );
	}
	memcpy( segment_table->filename[ segment ], filename, filename_size );

	/* Make sure the string is terminated
	 */
	segment_table->filename[ segment ][ filename_size ] = '\0';

	segment_table->file_descriptor[ segment ] = file_descriptor;

	return( segment_table );
}

/* Checks if a segment table entry is set
 * Return 0 when entry is not set
 */
uint8_t libewf_segment_table_values_is_set( LIBEWF_SEGMENT_TABLE *segment_table, uint32_t segment )
{
	if( segment_table == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_segment_table_values_is_set: invalid segment_table.\n" );
	}
	if( segment > segment_table->amount )
	{
		LIBEWF_FATAL_PRINT( "libewf_segment_table_values_is_set: segment out of range.\n" );
	}
	return( segment_table->filename[ segment ] != NULL );
}

/* Gets the filename of a certain segment
 */
char *libewf_segment_table_get_filename( LIBEWF_SEGMENT_TABLE *segment_table, uint32_t segment )
{
	if( segment_table == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_segment_table_get_filename: invalid segment_table.\n" );
	}
	if( segment > segment_table->amount )
	{
		LIBEWF_FATAL_PRINT( "libewf_segment_table_get_filename: segment out of range.\n" );
	}
	return( segment_table->filename[ segment ] );
}

/* Gets the file descriptor of a certain segment
 */
int libewf_segment_table_get_file_descriptor( LIBEWF_SEGMENT_TABLE *segment_table, uint32_t segment )
{
	if( segment_table == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_segment_table_get_file_descriptor: invalid segment_table.\n" );
	}
	if( segment > segment_table->amount )
	{
		LIBEWF_FATAL_PRINT( "libewf_segment_table_get_file_descriptor: segment out of range.\n" );
	}
	return( segment_table->file_descriptor[ segment ] );
}

