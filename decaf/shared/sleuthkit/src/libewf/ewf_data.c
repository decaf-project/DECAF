/*
 * EWF data section specification
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
#include "ewf_data.h"

/* Allocates memory for a new efw data struct
 */
EWF_DATA *ewf_data_alloc( void )
{
	EWF_DATA *data = (EWF_DATA *) malloc( EWF_DATA_SIZE );

	if( data == NULL )
	{
		LIBEWF_FATAL_PRINT( "ewf_data_alloc: unable to allocate ewf_data\n" );
	}
	memset( data, 0, EWF_DATA_SIZE );

	data->unknown3[0]          = 1;
	data->sectors_per_chunk[0] = 64;

	/* Set the bytes per sector to 512
	 * 512/256 = 2
	 */
	data->bytes_per_sector[1] = 2;

	return( data );
}

/* Frees memory of a data
 */
void ewf_data_free( EWF_DATA *data )
{
	if( data == NULL )
	{
		LIBEWF_FATAL_PRINT( "ewf_data_free: invalid data.\n" );
	}
	free( data );
}

/* Reads the data from a file descriptor
 */
EWF_DATA *ewf_data_read( int file_descriptor )
{
	EWF_DATA *data = ewf_data_alloc();

	ssize_t count = read( file_descriptor, data, EWF_DATA_SIZE );

	if( count < EWF_DATA_SIZE )
	{
		ewf_data_free( data );

		LIBEWF_FATAL_PRINT( "ewf_data_read: unable to read ewf_data\n" );
	}
	return( data );
}

/* Writes the data to a file descriptor
 * Returns a -1 on error, the amount of bytes written on success
 */
ssize_t ewf_data_write( EWF_DATA *data, int file_descriptor )
{
	EWF_CRC crc;
	ssize_t count;
	size_t size = EWF_DATA_SIZE;

	if( data == NULL )
	{
		LIBEWF_VERBOSE_PRINT( "ewf_data_write: invalid data.\n" );

		return( -1 );
	}
	crc = ewf_crc( (void *) data, ( size - EWF_CRC_SIZE ), 1 );

	revert_32bit( crc, data->crc );
	
	count = write( file_descriptor, data, size );

	if( count < size )
	{
		return( -1 );
	}
	return( count );
}

