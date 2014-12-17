/*
 * EWF sectors section specification
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

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "notify.h"

#include "ewf_compress.h"
#include "ewf_sectors.h"

/* Allocates memory for a new efw sectors_chunk 
 */
EWF_SECTORS_CHUNK *ewf_sectors_chunk_alloc( size_t size )
{
	size_t sectors_chunk_size        = sizeof( EWF_SECTORS_CHUNK ) * size;
	EWF_SECTORS_CHUNK *sectors_chunk = (EWF_SECTORS_CHUNK *) malloc( sectors_chunk_size );

	if( sectors_chunk == NULL )
	{
		LIBEWF_FATAL_PRINT( "ewf_sectors_chunk_alloc: unable to allocate sectors_chunk\n" );
	}
	memset( sectors_chunk, 0, size );

	return( sectors_chunk );
}

/* Reallocates memory for a efw sectors_chunk
 */
EWF_SECTORS_CHUNK *ewf_sectors_chunk_realloc( EWF_SECTORS_CHUNK *sectors_chunk, size_t size )
{
	size_t sectors_chunk_size = sizeof( EWF_SECTORS_CHUNK ) * size;

	if( sectors_chunk == NULL )
	{
		LIBEWF_FATAL_PRINT( "ewf_sectors_chunk_realloc: invalid sectors_chunk\n" );
	}
	sectors_chunk = (EWF_SECTORS_CHUNK *) realloc( (void *) sectors_chunk, sectors_chunk_size );

	if( sectors_chunk == NULL )
	{
		LIBEWF_FATAL_PRINT( "ewf_sectors_chunk_realloc: unable to allocate sectors_chunk\n" );
	}
	memset( sectors_chunk, 0, sectors_chunk_size );

	return( sectors_chunk );
}

/* Wipes memory for a ewf sectors_chunk
 */
EWF_SECTORS_CHUNK *ewf_sectors_chunk_wipe( EWF_SECTORS_CHUNK *sectors_chunk, size_t size )
{
	size_t sectors_chunk_size = sizeof( EWF_SECTORS_CHUNK ) * size;

	if( sectors_chunk == NULL )
	{
		LIBEWF_FATAL_PRINT( "ewf_sectors_chunk_realloc: invalid sectors_chunk\n" );
	}
	memset( sectors_chunk, 0, sectors_chunk_size );

	return( sectors_chunk );
}

/* Frees memory of a sectors_chunk
 */
void ewf_sectors_chunk_free( EWF_SECTORS_CHUNK *sectors_chunk )
{
	if( sectors_chunk == NULL )
	{
		LIBEWF_FATAL_PRINT( "ewf_sectors_chunk_free: invalid sectors_chunk.\n" );
	}
	free( sectors_chunk );
}

/* Uncompresses the sectors_chunk
 */
int ewf_sectors_chunk_uncompress( EWF_SECTORS_CHUNK *sectors_chunk, uint32_t *size, EWF_SECTORS_CHUNK *compressed_sectors_chunk, uint32_t compressed_size )
{
	return( ewf_uncompress( sectors_chunk, size, compressed_sectors_chunk, compressed_size ) );
}

/* Compresses the sectors_chunk
 */
int ewf_sectors_chunk_compress( EWF_SECTORS_CHUNK *compressed_sectors_chunk, uint32_t *compressed_size, EWF_SECTORS_CHUNK *sectors_chunk, uint32_t size, int8_t compression_level )
{
	return( ewf_compress( compressed_sectors_chunk, compressed_size, sectors_chunk, size, compression_level ) );
}

/* Reads the sectors_chunk from a file descriptor into a buffer
 */
ssize_t ewf_sectors_chunk_read( EWF_SECTORS_CHUNK *sectors_chunk, int file_descriptor, off_t offset, size_t size )
{
	ssize_t count;

	if( sectors_chunk == NULL )
	{
		LIBEWF_VERBOSE_PRINT( "ewf_sectors_chunk_read: invalid sectors_chunk.\n" );

		return( -1 );
	}
	if( lseek( file_descriptor, offset, SEEK_SET ) < 0 )
	{
		LIBEWF_FATAL_PRINT( "ewf_sectors_chunk_read: cannot find offset: %ld.\n", offset );
	}
	count = read( file_descriptor, sectors_chunk, size );

	if( count < size )
	{
		LIBEWF_FATAL_PRINT( "ewf_sectors_chunk_read: unable to read sectors_chunk.\n" );
	}
	return( count );
}

/* Writes the sectors_chunk to a file descriptor
 * Returns a -1 on error, the amount of bytes written on success
 */
ssize_t ewf_sectors_chunk_write( EWF_SECTORS_CHUNK *sectors_chunk, int file_descriptor, size_t size )
{
	ssize_t count;

	if( sectors_chunk == NULL )
	{
		LIBEWF_VERBOSE_PRINT( "ewf_sectors_chunk_write: invalid sectors_chunk.\n" );

		return( -1 );
	}
	count = write( file_descriptor, sectors_chunk, size );

	if( count < size )
	{
		return( -1 );
	}
	return( count );
}

/* Print the sectors_chunk data to a stream
 */
void ewf_sectors_chunk_fprint( FILE *stream, EWF_SECTORS_CHUNK *sectors_chunk )
{
	if ( ( stream != NULL ) && ( sectors_chunk != NULL ) )
	{
		fprintf( stream, "%s", sectors_chunk );
	}
}

