/*
 * EWF section start (header) specification
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
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "definitions.h"
#include "libewf_endian.h"
#include "notify.h"

#include "ewf_section.h"
#include "ewf_crc.h"

/* Allocates memory for a new efw section struct
 */
EWF_SECTION *ewf_section_alloc( void )
{
	EWF_SECTION *section = (EWF_SECTION *) malloc( EWF_SECTION_SIZE );

	if( section == NULL )
	{
		LIBEWF_FATAL_PRINT( "ewf_section_alloc: unable to allocate ewf_section\n" );
	}
	memset( section, 0, EWF_SECTION_SIZE );

	return( section );
}

/* Frees memory of a section
 */
void ewf_section_free( EWF_SECTION *section )
{
	if( section == NULL )
	{
		LIBEWF_FATAL_PRINT( "ewf_section_free: invalid section.\n" );
	}
	free( section );
}

/* Reads the section from a file descriptor
 */
EWF_SECTION *ewf_section_read( int file_descriptor )
{
	EWF_SECTION *section = ewf_section_alloc();

	ssize_t count = read( file_descriptor, section, EWF_SECTION_SIZE );

	if( count < EWF_SECTION_SIZE )
	{
		ewf_section_free( section );

		LIBEWF_FATAL_PRINT( "ewf_section_read: unable to read ewf_section\n" );
	}
	return( section );
}

/* Writes a section to a file descriptor
 * Returns a -1 on error, the amount of bytes written on success
 */
ssize_t ewf_section_write( EWF_SECTION *section, int file_descriptor )
{
	EWF_CRC crc;
	ssize_t count;
	size_t size = EWF_SECTION_SIZE;

	if( section == NULL )
	{
		LIBEWF_VERBOSE_PRINT( "ewf_section_write: invalid section.\n" );

		return( -1 );
	}
	crc = ewf_crc( (void *) section, ( size - EWF_CRC_SIZE ), 1 );

	revert_32bit( crc, section->crc );

	count = write( file_descriptor, section, size );

	if( count < size )
	{
		return( -1 );
	}
	return( count );
}

/* Test if section is of a certain type
 */
uint8_t ewf_section_is_type( EWF_SECTION *section, const char *type )
{
	return( memcmp( section->type, type, strlen( type ) ) == 0 );
}

/* Test if section is of type header
 */
uint8_t ewf_section_is_type_header( EWF_SECTION *section )
{
	return( ewf_section_is_type( section, "header" ) );
}

/* Test if section is of type header2
 */
uint8_t ewf_section_is_type_header2( EWF_SECTION *section )
{
	return( ewf_section_is_type( section, "header2" ) );
}

/* Test if section is of type volume
 */
uint8_t ewf_section_is_type_volume( EWF_SECTION *section )
{
	return( ewf_section_is_type( section, "volume" ) );
}

/* Test if section is of type disk
 */
uint8_t ewf_section_is_type_disk( EWF_SECTION *section )
{
	return( ewf_section_is_type( section, "disk" ) );
}

/* Test if section is of type table
 */
uint8_t ewf_section_is_type_table( EWF_SECTION *section )
{
	return( ewf_section_is_type( section, "table" ) );
}

/* Test if section is of type table2
 */
uint8_t ewf_section_is_type_table2( EWF_SECTION *section )
{
	return( ewf_section_is_type( section, "table2" ) );
}

/* Test if section is of type sectors
 */
uint8_t ewf_section_is_type_sectors( EWF_SECTION *section )
{
	return( ewf_section_is_type( section, "sectors" ) );
}

/* Test if section is of type hash
 */
uint8_t ewf_section_is_type_hash( EWF_SECTION *section )
{
	return( ewf_section_is_type( section, "hash" ) );
}

/* Test if section is of type done
 */
uint8_t ewf_section_is_type_done( EWF_SECTION *section )
{
	return( ewf_section_is_type( section, "done" ) );
}

/* Test if section is of type next
 */
uint8_t ewf_section_is_type_next( EWF_SECTION *section )
{
	return( ewf_section_is_type( section, "next" ) );
}

/* Test if section is of type data
 */
uint8_t ewf_section_is_type_data( EWF_SECTION *section )
{
	return( ewf_section_is_type( section, "data" ) );
}

/* Test if section is of type error2
 */
uint8_t ewf_section_is_type_error2( EWF_SECTION *section )
{
	return( ewf_section_is_type( section, "error2" ) );
}

/* Print the section data to a stream
 */
void ewf_section_fprint( FILE *stream, EWF_SECTION *section )
{
	if ( ( stream != NULL ) && ( section != NULL ) )
	{
		EWF_CRC calculated_crc = ewf_crc( (void *) section, ( EWF_SECTION_SIZE - EWF_CRC_SIZE ), 1 );
		EWF_CRC stored_crc     = convert_32bit( section->crc );

		uint64_t next = convert_64bit( section->next );
		uint64_t size = convert_64bit( section->size );

		fprintf( stream, "Section:\n" );
		fprintf( stream, "type: %s\n", section->type );
		fprintf( stream, "next: %" PRIu64 "\n", next );
		fprintf( stream, "size: %" PRIu64 "\n", size );
		fprintf( stream, "crc: %" PRIu32 " ( %" PRIu32 " )\n", stored_crc, calculated_crc );
		fprintf( stream, "\n" );
	}
}

