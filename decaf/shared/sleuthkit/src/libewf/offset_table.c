/*
 * libewf offset table
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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "notify.h"

#include "offset_table.h"

/* Allocates memory for a new offset table struct
 */
LIBEWF_OFFSET_TABLE *libewf_offset_table_alloc( void )
{
	LIBEWF_OFFSET_TABLE *offset_table = (LIBEWF_OFFSET_TABLE *) malloc( LIBEWF_OFFSET_TABLE_SIZE );

	if( offset_table == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_offset_table_alloc: unable to allocate offset_table\n" );
	}
        offset_table->amount          = 0;
        offset_table->last            = 0;
        offset_table->file_descriptor = NULL;
        offset_table->compressed      = NULL;
        offset_table->offset          = NULL;
        offset_table->size            = NULL;

	return( offset_table );
}

/* Allocates memory for the dynamic file descriptor, offset and size array
 */
LIBEWF_OFFSET_TABLE *libewf_offset_table_values_alloc( LIBEWF_OFFSET_TABLE *offset_table, uint32_t size )
{
	if( offset_table == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_offset_table_values_alloc: invalid offset_table\n" );
	}
	offset_table->file_descriptor = (int *) malloc( size * LIBEWF_OFFSET_TABLE_FILE_DESCRIPTOR_SIZE );

	if( offset_table->file_descriptor == NULL )
	{
		LIBEWF_FATAL_PRINT( "offset_table_values_alloc: unable to allocate file descriptor\n" );
	}
	memset( offset_table->file_descriptor, -1, ( size * LIBEWF_OFFSET_TABLE_FILE_DESCRIPTOR_SIZE ) );

	offset_table->compressed = (uint8_t *) malloc( size * LIBEWF_OFFSET_TABLE_COMPRESSED_SIZE );

	if( offset_table->compressed == NULL )
	{
		LIBEWF_FATAL_PRINT( "offset_table_values_alloc: unable to allocate compressed\n" );
	}
	memset( offset_table->compressed, 0, ( size * LIBEWF_OFFSET_TABLE_COMPRESSED_SIZE ) );

	offset_table->offset = (uint64_t *) malloc( size * LIBEWF_OFFSET_TABLE_OFFSET_SIZE );

	if( offset_table->offset == NULL )
	{
		libewf_offset_table_free( offset_table );

		LIBEWF_FATAL_PRINT( "offset_table_values_alloc: unable to allocate offset\n" );
	}
	memset( offset_table->offset, 0, ( size * LIBEWF_OFFSET_TABLE_OFFSET_SIZE ) );

	offset_table->size = (uint64_t *) malloc( size * LIBEWF_OFFSET_TABLE_SIZE_SIZE );

	if( offset_table->size == NULL )
	{
		libewf_offset_table_free( offset_table );

		LIBEWF_FATAL_PRINT( "offset_table_values_alloc: unable to allocate size\n" );
	}
	memset( offset_table->size, 0, ( size * LIBEWF_OFFSET_TABLE_SIZE_SIZE ) );

	offset_table->amount = size;

	return( offset_table );
}

/* Reallocates memory for the dynamic file descriptor, offset and size array
 */
LIBEWF_OFFSET_TABLE *libewf_offset_table_values_realloc( LIBEWF_OFFSET_TABLE *offset_table, uint32_t size )
{
	uint32_t offset;

	if( offset_table == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_offset_table_values_realloc: invalid offset_table\n" );
	}
	offset = offset_table->amount;

	offset_table->file_descriptor = (int *) realloc( offset_table->file_descriptor, ( size * LIBEWF_OFFSET_TABLE_FILE_DESCRIPTOR_SIZE ) );

	if( offset_table->file_descriptor == NULL )
	{
		LIBEWF_FATAL_PRINT( "offset_table_values_realloc: unable to allocate file_descriptor\n" );
	}
	memset( ( offset_table->file_descriptor + offset ), -1, ( ( size - offset ) * LIBEWF_OFFSET_TABLE_FILE_DESCRIPTOR_SIZE ) );

	offset_table->compressed = (uint8_t *) realloc( offset_table->compressed, ( size * LIBEWF_OFFSET_TABLE_COMPRESSED_SIZE ) );

	if( offset_table->compressed == NULL )
	{
		LIBEWF_FATAL_PRINT( "offset_table_values_realloc: unable to allocate compressed\n" );
	}
	memset( ( offset_table->compressed + offset ), -1, ( ( size - offset ) * LIBEWF_OFFSET_TABLE_COMPRESSED_SIZE ) );

	offset_table->offset = (uint64_t *) realloc( offset_table->offset, ( size * LIBEWF_OFFSET_TABLE_OFFSET_SIZE ) );

	if( offset_table->offset == NULL )
	{
		libewf_offset_table_free( offset_table );

		LIBEWF_FATAL_PRINT( "offset_table_values_realloc: unable to allocate offset\n" );
	}
	memset( ( offset_table->offset + offset ), 0, ( ( size - offset ) * LIBEWF_OFFSET_TABLE_OFFSET_SIZE ) );

	offset_table->size = (uint64_t *) realloc( offset_table->size, ( size * LIBEWF_OFFSET_TABLE_SIZE_SIZE ) );

	if( offset_table->size == NULL )
	{
		libewf_offset_table_free( offset_table );

		LIBEWF_FATAL_PRINT( "offset_table_values_realloc: unable to allocate size\n" );
	}
	memset( ( offset_table->size + offset ), 0, ( ( size - offset ) * LIBEWF_OFFSET_TABLE_SIZE_SIZE ) );

	offset_table->amount = size;

	return( offset_table );
}

/* Frees memory of a offset table struct including elements
 */
void libewf_offset_table_free( LIBEWF_OFFSET_TABLE *offset_table )
{
	free( offset_table->file_descriptor );
	free( offset_table->offset );
	free( offset_table->size );

	free( offset_table );
}

/* Sets the values for a specific offset
 */
LIBEWF_OFFSET_TABLE *libewf_offset_table_set_values( LIBEWF_OFFSET_TABLE *offset_table, uint32_t chunk, int file_descriptor, uint8_t compressed, uint64_t offset, uint64_t size )
{
	if( offset_table == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_offset_table_set_values: invalid offset_table\n" );
	}
	if( chunk > offset_table->amount )
	{
		LIBEWF_VERBOSE_PRINT( "libewf_offset_table_set_values: allocating additional offset_table entries\n" );

		offset_table = libewf_offset_table_values_realloc( offset_table, chunk );
	}
	offset_table->file_descriptor[ chunk ] = file_descriptor;
	offset_table->compressed[ chunk ]      = compressed;
	offset_table->offset[ chunk ]          = offset;
	offset_table->size[ chunk ]            = size;

	return( offset_table );
}

