/*
 * EWF table section specification
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

#include "libewf_endian.h"
#include "notify.h"

#include "ewf_crc.h"
#include "ewf_table.h"

/* Allocates memory for a new efw table struct
 */
EWF_TABLE *ewf_table_alloc( void )
{
	EWF_TABLE *table = (EWF_TABLE *) malloc( EWF_TABLE_SIZE );

	if( table == NULL )
	{
		LIBEWF_FATAL_PRINT( "ewf_table_alloc: unable to allocate ewf_table.\n" );
	}
	memset( table, 0, EWF_TABLE_SIZE );

	return( table );
}

/* Allocates memory for a buffer of ewf table offsets 
 */
EWF_TABLE_OFFSET *ewf_table_offsets_alloc( uint32_t amount )
{
	size_t size               = EWF_TABLE_OFFSET_SIZE * amount;
	EWF_TABLE_OFFSET *offsets = (EWF_TABLE_OFFSET *) malloc( size );

	if( offsets == NULL )
	{
		LIBEWF_FATAL_PRINT( "ewf_table_offsets_alloc: unable to allocate offsets.\n" );
	}
	memset( offsets, 0, size );

	return( offsets );
}

/* Frees memory of a table
 */
void ewf_table_free( EWF_TABLE *table )
{
	if( table == NULL )
	{
		LIBEWF_FATAL_PRINT( "ewf_table_free: invalid table.\n" );
	}
	free( table );
}

/* Frees memory of a buffer of ewf table offsets
 */
void ewf_table_offsets_free( EWF_TABLE_OFFSET *offsets )
{
	if( offsets == NULL )
	{
		LIBEWF_FATAL_PRINT( "ewf_table_offsets_free: invalid offsets.\n" );
	}
	free( offsets );
}

/* Reads the table from a file descriptor
 */
EWF_TABLE *ewf_table_read( int file_descriptor )
{
	EWF_TABLE *table = ewf_table_alloc();

	ssize_t count = read( file_descriptor, table, EWF_TABLE_SIZE );

	if( count < EWF_TABLE_SIZE )
	{
		ewf_table_free( table );

		LIBEWF_FATAL_PRINT( "ewf_table_read: unable to read ewf_table.\n" );
	}
	return( table );
}

/* Reads the ewf table offsets from a file descriptor
 */
EWF_TABLE_OFFSET *ewf_table_offsets_read( int file_descriptor, uint32_t amount )
{
	EWF_TABLE_OFFSET *offsets = ewf_table_offsets_alloc( amount );

	size_t size   = EWF_TABLE_OFFSET_SIZE * amount;
	ssize_t count = read( file_descriptor, offsets, size );

	if( count < size )
	{
		ewf_table_offsets_free( offsets );

		LIBEWF_FATAL_PRINT( "ewf_table_offsets_read: unable to read offsets.\n" );
	}
	return( offsets );
}

/* Writes the table to a file descriptor
 * Returns a -1 on error, the amount of bytes written on success
 */
ssize_t ewf_table_write( EWF_TABLE *table, int file_descriptor )
{
	EWF_CRC crc;
	ssize_t count;
	size_t size = EWF_TABLE_SIZE;

	if( table == NULL )
	{
		LIBEWF_VERBOSE_PRINT( "ewf_table_write: invalid table.\n" );

		return( -1 );
	}
	crc = ewf_crc( (void *) table, ( size - EWF_CRC_SIZE ), 1 );

	revert_32bit( crc, table->crc );

	count = write( file_descriptor, table, size );

	if( count < size )
	{
		return( -1 );
	}
	return( count );
}

/* Writes the offsets to a file descriptor
 * Returns a -1 on error, the amount of bytes written on success
 */
ssize_t ewf_table_offsets_write( EWF_TABLE_OFFSET *offsets, int file_descriptor, uint32_t amount )
{
	EWF_CRC crc;
	ssize_t count;
	ssize_t crc_count;
	size_t size = EWF_TABLE_OFFSET_SIZE * amount;

	if( offsets == NULL )
	{
		LIBEWF_VERBOSE_PRINT( "ewf_table_offsets_write: invalid offsets.\n" );

		return( -1 );
	}
	count = write( file_descriptor, offsets, size );

	if( count < size )
	{
		return( -1 );
	}
	crc       = ewf_crc( offsets, size, 1 );
	crc_count = ewf_crc_write( crc, file_descriptor );

	return( count + crc_count );
}

