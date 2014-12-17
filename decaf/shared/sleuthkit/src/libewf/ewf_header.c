/*
 * EWF header section specification
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

#include "ewf_compress.h"
#include "ewf_header.h"

/* Allocates memory for a new efw header struct
 */
EWF_HEADER *ewf_header_alloc( size_t size )
{
	size_t header_size = sizeof( EWF_HEADER ) * size;
	EWF_HEADER *header = (EWF_HEADER *) malloc( header_size );

	if( header == NULL )
	{
		LIBEWF_FATAL_PRINT( "ewf_header_alloc: unable to allocate ewf_header\n" );
	}
	memset( header, 0, header_size );

	return( header );
}

/* Frees memory of a header
 */
void ewf_header_free( EWF_HEADER *header )
{
	if( header == NULL )
	{
		LIBEWF_FATAL_PRINT( "ewf_header_free: invalid header.\n" );
	}
	free( header );
}

/* Uncompresses the header
 */
EWF_HEADER *ewf_header_uncompress( EWF_HEADER *compressed_header, uint32_t *size )
{
	EWF_HEADER *header;
	int result;
	uint32_t compressed_size = *size;

	/* Make sure the target buffer for the uncompressed data is large enough
	 */
	*size *= 16;

	if( compressed_header == NULL )
	{
		LIBEWF_FATAL_PRINT( "ewf_header_uncompress: invalid compressed_header.\n" );
	}
	header = ewf_header_alloc( (size_t) *size );
	result = ewf_uncompress( (uint8_t *) header, size, (uint8_t *) compressed_header, compressed_size );

	if( result != Z_OK )
	{
		LIBEWF_FATAL_PRINT( "ewf_header_uncompress: unable to uncompress header.\n" );
	}
	return( header );
}

/* Compresses the header
 */
EWF_HEADER *ewf_header_compress( EWF_HEADER *header, uint32_t *size, int8_t compression_level )
{
	EWF_HEADER *compressed_header;
	int result;
	uint32_t uncompressed_size = *size;

	/* Make sure the target buffer for the uncompressed data is large enough
	 */
	*size *= 16;

	if( header == NULL )
	{
		LIBEWF_FATAL_PRINT( "ewf_header_compress: invalid header.\n" );
	}
	compressed_header = ewf_header_alloc( (size_t) *size );
	result            = ewf_compress( (uint8_t *) compressed_header, size, (uint8_t *) header, uncompressed_size, compression_level );

	if( result != Z_OK )
	{
		LIBEWF_FATAL_PRINT( "ewf_header_compress: unable to compress header.\n" );
	}
	return( compressed_header );
}

/* Reads the header from a file descriptor
 */
EWF_HEADER *ewf_header_read( int file_descriptor, size_t *size )
{
	EWF_HEADER *header;
	EWF_HEADER *compressed_header = ewf_header_alloc( *size );

	ssize_t count = read( file_descriptor, compressed_header, *size );

	if( count < *size )
	{
		free( compressed_header );

		LIBEWF_FATAL_PRINT( "ewf_header_read: unable to read ewf_header\n" );
	}
	header = ewf_header_uncompress( compressed_header, (uint32_t *) size );

	return( header );
}

/* Writes the header to a file descriptor
 * Returns a -1 on error, the amount of bytes written on success
 */
ssize_t ewf_header_write( EWF_HEADER *header, int file_descriptor, size_t size )
{
	ssize_t count;

	if( header == NULL )
	{
		LIBEWF_VERBOSE_PRINT( "ewf_header_write: invalid header.\n" );

		return( -1 );
	}
	count = write( file_descriptor, header, size );

	if( count < size )
	{
		return( -1 );
	}
	return( count );
}

/* Print the header data to a stream
 */
void ewf_header_fprint( FILE *stream, EWF_HEADER *header )
{
	if ( ( stream != NULL ) && ( header != NULL ) )
	{
		fprintf( stream, "%s", (char *) header );
	}
}

