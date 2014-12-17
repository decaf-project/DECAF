/*
 * libewf handle
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
#include <zlib.h>

#include "notify.h"

#include "ewf_crc.h"
#include "ewf_data.h"
#include "ewf_file_header.h"
#include "ewf_md5hash.h"
#include "ewf_sectors.h"
#include "handle.h"
#include "header_values.h"
#include "offset_table.h"
#include "segment_table.h"

/* Allocates memory for a new handle struct
 */
LIBEWF_HANDLE *libewf_handle_alloc( uint32_t segment_amount )
{
	LIBEWF_HANDLE *handle = (LIBEWF_HANDLE *) malloc( LIBEWF_HANDLE_SIZE );

	if( handle == NULL )
	{
		LIBEWF_FATAL_PRINT( "ewf_handle_alloc: unable to allocate ewf_handle\n" );
	}
	/* 32k is the default chunk size
	 */
	handle->chunk_size              = EWF_MINIMUM_CHUNK_SIZE;
	handle->sectors_per_chunk       = 64;
	handle->bytes_per_sector        = 512;
	handle->chunk_count             = 0;
	handle->sector_count            = 0;
	handle->input_file_size         = 0;
	/* The default maximimum size is about the size of a 64 minutes CDROM
	 */
	handle->ewf_file_size           = 650000000;
	handle->chunks_per_file         = ( 650000000 - EWF_FILE_HEADER_SIZE - EWF_DATA_SIZE ) / EWF_MINIMUM_CHUNK_SIZE;
	handle->segment_table           = libewf_segment_table_alloc( segment_amount );
	handle->offset_table            = libewf_offset_table_alloc();
	handle->secondary_offset_table  = libewf_offset_table_alloc();
	handle->error2_error_count      = 0;
	handle->error2_sectors          = NULL;
	handle->header                  = NULL;
	handle->header2                 = NULL;
	handle->md5hash                 = NULL;
	handle->chunk_crc               = 0;
	handle->compression_used        = 0;
	handle->alternative_read_method = 0;
	handle->compression_level       = Z_NO_COMPRESSION;
	handle->format                  = LIBEWF_FORMAT_UNKNOWN;
	handle->index_build             = 0;
	handle->cached_chunk            = 0;
	handle->cached_data_size        = 0;
	handle->media_type              = 0;

	handle = libewf_handle_cache_alloc( handle, handle->chunk_size );

	return( handle );
}

/* Allocates memory for the handle cache
 */
LIBEWF_HANDLE *libewf_handle_cache_alloc( LIBEWF_HANDLE *handle, uint32_t size )
{
	if( handle == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_handle_cache_alloc: invalid handle\n" );
	}
	handle->raw_data     = ewf_sectors_chunk_alloc( size );
	handle->chunk_data   = ewf_sectors_chunk_alloc( size + EWF_CRC_SIZE );
	handle->cached_chunk = -1;

	return( handle );
}

/* Reallocates and wipes memory for the handle cache
 */
LIBEWF_HANDLE *libewf_handle_cache_realloc( LIBEWF_HANDLE *handle, uint32_t size )
{
	if( handle == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_handle_cache_realloc: invalid handle\n" );
	}
	handle->raw_data     = ewf_sectors_chunk_realloc( handle->raw_data, size );
	handle->chunk_data   = ewf_sectors_chunk_realloc( handle->chunk_data, ( size + EWF_CRC_SIZE ) );
	handle->cached_chunk = -1;

	return( handle );
}

/* Wipes memory for the handle cache
 */
LIBEWF_HANDLE *libewf_handle_cache_wipe( LIBEWF_HANDLE *handle, uint32_t size )
{
	if( handle == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_handle_cache_wipe: invalid handle\n" );
	}
	handle->raw_data     = ewf_sectors_chunk_wipe( handle->raw_data, size );
	handle->chunk_data   = ewf_sectors_chunk_wipe( handle->chunk_data, ( size + EWF_CRC_SIZE ) );
	handle->cached_chunk = -1;

	return( handle );
}

/* Frees memory of a handle struct including elements
 */
void libewf_handle_free( LIBEWF_HANDLE *handle )
{
	if( handle == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_handle_free: invalid handle\n" );
	}
	libewf_segment_table_free( handle->segment_table );
	libewf_offset_table_free( handle->offset_table );
	libewf_offset_table_free( handle->secondary_offset_table );

	if( handle->error2_sectors != NULL )
	{
		ewf_error2_sectors_free( handle->error2_sectors );
	}
	if( handle->header != NULL )
	{
		ewf_header_free( handle->header );
	}
	if( handle->header2 != NULL )
	{
		ewf_header_free( handle->header2 );
	}
	if( handle->md5hash != NULL )
	{
		ewf_md5hash_free( handle->md5hash );
	}
	free( handle->raw_data );
	free( handle->chunk_data );

	free( handle );
}

/* Check if the header value is set
 * Return 0 if not set, 1 if set
 */
uint8_t libewf_handle_is_set_header( LIBEWF_HANDLE *handle )
{
	if( handle == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_handle_is_set_header: invalid handle\n" );
	}
	return( handle->header != NULL );
}

/* Check if the header2 value is set
 * Return 0 if not set, 1 if set
 */
uint8_t libewf_handle_is_set_header2( LIBEWF_HANDLE *handle )
{
	if( handle == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_handle_is_set_header2: invalid handle\n" );
	}
	return( handle->header2 != NULL );
}

/* Sets the header
 */
void libewf_handle_set_header( LIBEWF_HANDLE *handle, EWF_HEADER *header )
{
	if( handle == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_handle_set_header: invalid handle\n" );
	}
	handle->header = header;
}

/* Sets the header2
 */
void libewf_handle_set_header2( LIBEWF_HANDLE *handle, EWF_HEADER *header2 )
{
	if( handle == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_handle_set_header2: invalid handle\n" );
	}
	handle->header2 = header2;
}

/* Sets the MD5 hash value
 */
void libewf_handle_set_md5hash( LIBEWF_HANDLE *handle, EWF_MD5HASH *md5hash )
{
	if( handle == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_handle_set_md5hash: invalid handle\n" );
	}
	handle->md5hash = ewf_md5hash_alloc();

	memcpy( (uint8_t *) handle->md5hash, (uint8_t *) md5hash, EWF_MD5HASH_SIZE );
}

