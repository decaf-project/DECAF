/*
 * libewf file handling
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

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <zlib.h>

#include "libewf_endian.h"
#include "notify.h"
#include "md5.h"

#include "ewf_compress.h"
#include "ewf_crc.h"
#include "ewf_md5hash.h"
#include "ewf_file_header.h"
#include "ewf_header.h"
#include "ewf_section.h"
#include "ewf_volume.h"
#include "ewf_table.h"
#include "file.h"
#include "file_read.h"
#include "handle.h"
#include "section_list.h"
#include "offset_table.h"
#include "segment_table.h"

/* Detects if a file is an EWF file
 * Returns 1 if true, 0 otherwise
 */
uint8_t libewf_check_file_signature( const char *filename )
{
	uint8_t signature[ 8 ];
	int file_descriptor;

	if( filename == NULL )
	{
		return( 0 );
	}
	file_descriptor = open( filename, O_RDONLY );

	if( file_descriptor < 0 )
	{
		LIBEWF_FATAL_PRINT( "libewf_check_file_signature: unable to open file: %s: %m", filename );
	}
	if( read( file_descriptor, signature, 8 ) != 8 )
	{
		LIBEWF_FATAL_PRINT( "libewf_check_file_signature: unable to read signature from file: %s: %m", filename );
	}
	close( file_descriptor );

	return( ewf_file_header_check_signature( signature ) );
}

/* Opens an EWF file
 * For reading files should contain all filenames that make up an EWF image
 * For writing files should contain the base of the filename, extentions like .e01 will be automatically added
 */
LIBEWF_HANDLE *libewf_open( const char **filenames, uint32_t file_amount, uint8_t flags )
{
	uint32_t iterator;
	LIBEWF_HANDLE *handle = NULL;

	if( flags == LIBEWF_OPEN_READ )
	{
		/* 1 additional entry required because
		 * entry [ 0 ] is not used for reading
		 */
		handle = libewf_handle_alloc( file_amount + 1 );

		for( iterator = 0; iterator < file_amount; iterator++ )
		{
			libewf_open_read( handle, filenames[ iterator ] );
		}
		handle = libewf_build_index( handle );
	}
	else if( flags == LIBEWF_OPEN_WRITE )
	{
		/* Allocate 2 entries
		 * entry [ 0 ] is used for the base filename
		 */
		handle = libewf_handle_alloc( 2 );

		libewf_open_write( handle, filenames[ 0 ] );
	}
	else
	{
		LIBEWF_FATAL_PRINT( "libewf_open: unsupported flags.\n" );
	}
	LIBEWF_VERBOSE_PRINT( "libewf_open: open successful.\n" );

	return( handle );
}

/* Opens an EWF file for reading
 */
LIBEWF_HANDLE *libewf_open_read( LIBEWF_HANDLE *handle, const char *filename )
{
	int file_descriptor;
	EWF_FILE_HEADER *file_header;
	uint16_t fields_segment;

	if( handle == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_open_read: incorrect handle.\n" );
	}
	file_descriptor = open( filename, O_RDONLY );

	if( file_descriptor == -1 )
	{
		LIBEWF_FATAL_PRINT( "libewf_open: unable to open file: %s\n", filename );
	}
	file_header = ewf_file_header_read( file_descriptor );

	if( file_header == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_open_read: incorrect file header in: %s.\n", filename );
	}
	fields_segment = convert_16bit( file_header->fields_segment );

	LIBEWF_VERBOSE_PRINT( "libewf_open_read: added segment file: %s with file descriptor: %d with segment number: %" PRIu16 ".\n", filename, file_descriptor, fields_segment );

	handle->segment_table = libewf_segment_table_set_values( handle->segment_table, fields_segment, filename, file_descriptor );

	ewf_file_header_free( file_header );

	LIBEWF_VERBOSE_PRINT( "libewf_open_read: open successful.\n" );

	return( handle );
}

/* Opens an EWF file for writing
 */
LIBEWF_HANDLE *libewf_open_write( LIBEWF_HANDLE *handle, const char *filename )
{
	LIBEWF_FATAL_PRINT( "libewf_open_write: write is not supported yet.\n" );

	return( handle );
}

/* Builds the index from the files
 */
LIBEWF_HANDLE *libewf_build_index( LIBEWF_HANDLE *handle )
{
	EWF_SECTION *last_section = NULL;
	uint32_t segment          = 0;

	if( handle == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_build_index: incorrect handle\n" );
	}
	if( handle->index_build != 0 )
	{
		LIBEWF_FATAL_PRINT( "libewf_build_index: index has already been build\n" );
	}
	for( segment = 1; segment < handle->segment_table->amount; segment++ )
	{
		LIBEWF_VERBOSE_PRINT( "libewf_build_index: building index for segment: %" PRIu32 "\n", segment );

		last_section = libewf_sections_read_segment( handle, segment );
	}
	/* Check to see if we are done */
	if( ( last_section == NULL ) || ( !ewf_section_is_type_done( last_section ) ) )
	{
		LIBEWF_FATAL_PRINT( "libewf_build_index: no ending section, cannot find the last segment file\n" );
	}
	ewf_section_free( last_section );

	handle->index_build = 1;

	LIBEWF_VERBOSE_PRINT( "libewf_build_index: index successful build.\n" );

	return( handle );
}

/* Closes the EWF handle
 */
void libewf_close( LIBEWF_HANDLE *handle )
{
	uint32_t iterator;

	if( handle == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_close: incorrect handle\n" );
	}
	for( iterator = 0; iterator < handle->segment_table->amount; iterator++ )
	{
		if( handle->segment_table->file_descriptor[ iterator ] > 0 )
		{
			close( handle->segment_table->file_descriptor[ iterator ] );
		}
	}
	libewf_handle_free( handle );
}

/* Returns the size of the contained data
 */
uint64_t libewf_data_size( LIBEWF_HANDLE *handle )
{
	if( handle == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_data_size: incorrect handle\n" );
	}
	if( handle->index_build == 0 )
	{
		LIBEWF_FATAL_PRINT( "libewf_data_size: index was not build.\n" );
	}
	return( handle->offset_table->amount * handle->chunk_size );
}

/* Returns a printable string of the stored md5 hash
 * Returns an NULL if no hash was stores in the file
 */
char *libewf_data_md5hash( LIBEWF_HANDLE *handle )
{
	if( handle == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_data_md5hash: incorrect handle\n" );
	}
	if( handle->index_build == 0 )
	{
		LIBEWF_FATAL_PRINT( "libewf_data_md5hash: index was not build.\n" );
	}
	if( handle->md5hash == NULL )
	{
		return( NULL );
	}
	return( ewf_md5hash_to_string( handle->md5hash ) );
}

/* Calculates the MD5 hash and compares this with the MD5 hash stored in the image
 */
char *libewf_calculate_md5hash( LIBEWF_HANDLE *handle )
{
	char *data;
	char *calculated_md5hash_string;
	uint64_t iterator;
	EWF_MD5HASH *calculated_md5hash;
	LIBEWF_MD5_CTX md5;

	if( handle == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_check_md5hash: incorrect handle\n" );
	}
	if( handle->index_build == 0 )
	{
		LIBEWF_FATAL_PRINT( "libewf_check_md5hash: index was not build.\n" );
	}
	data = (char *) malloc( handle->chunk_size );

	if( data == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_check_md5hash: unable to allocate data\n" );
	}
	LIBEWF_MD5_INIT( &md5 );

	for( iterator = 0; iterator <= handle->offset_table->last; iterator++ )
	{
		uint64_t offset = iterator * handle->chunk_size;
		int64_t count   = libewf_read_random( handle, data, handle->chunk_size, offset );

		LIBEWF_MD5_UPDATE( &md5, data, count );
  	}
	free( data ) ;

	calculated_md5hash = ewf_md5hash_alloc();

  	LIBEWF_MD5_FINAL( calculated_md5hash, &md5 );

	calculated_md5hash_string = ewf_md5hash_to_string( calculated_md5hash );

	ewf_md5hash_free( calculated_md5hash );

	return( calculated_md5hash_string );
}

