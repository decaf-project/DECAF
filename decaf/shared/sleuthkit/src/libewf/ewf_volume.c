/*
 * EWF volume section specification
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
#include "ewf_volume.h"

/* Allocates memory for a new efw volume struct
 */
EWF_VOLUME *ewf_volume_alloc( void )
{
	EWF_VOLUME *volume = (EWF_VOLUME *) malloc( EWF_VOLUME_SIZE );

	if( volume == NULL )
	{
		LIBEWF_FATAL_PRINT( "ewf_volume_alloc: unable to allocate ewf_volume\n" );
	}
	memset( volume, 0, EWF_VOLUME_SIZE );

	volume->unknown3[0]          = 1;
	volume->sectors_per_chunk[0] = 64;

	/* Set the bytes per sector to 512
	 * 512/256 = 2
	 */
	volume->bytes_per_sector[1] = 2;

	return( volume );
}

/* Frees memory of a volume
 */
void ewf_volume_free( EWF_VOLUME *volume )
{
	if( volume == NULL )
	{
		LIBEWF_FATAL_PRINT( "ewf_volume_free: invalid volume.\n" );
	}
	free( volume );
}

/* Reads the volume from a file descriptor
 */
EWF_VOLUME *ewf_volume_read( int file_descriptor )
{
	EWF_VOLUME *volume = ewf_volume_alloc();

	ssize_t count = read( file_descriptor, volume, EWF_VOLUME_SIZE );

	if( count < EWF_VOLUME_SIZE )
	{
		ewf_volume_free( volume );

		LIBEWF_FATAL_PRINT( "ewf_volume_read: unable to read ewf_volume\n" );
	}
	return( volume );
}

/* Writes the volume to a file descriptor
 * Returns a -1 on error, the amount of bytes written on success
 */
ssize_t ewf_volume_write( EWF_VOLUME *volume, int file_descriptor )
{
	EWF_CRC crc;
	ssize_t count;
	size_t size = EWF_VOLUME_SIZE;

	if( volume == NULL )
	{
		LIBEWF_VERBOSE_PRINT( "ewf_volume_write: invalid volume.\n" );

		return( -1 );
	}
	crc = ewf_crc( (void *) volume, ( size - EWF_CRC_SIZE ), 1 );

	revert_32bit( crc, volume->crc );

	count = write( file_descriptor, volume, size );

	if( count < size )
	{
		return( -1 );
	}
	return( count );
}

/* Calculates the chunck size = sectors per chunk * bytes per sector
 */
uint32_t ewf_volume_calculate_chunk_size( EWF_VOLUME *volume )
{
	uint32_t sectors_per_chunk = convert_32bit( volume->sectors_per_chunk );
	uint32_t bytes_per_sector  = convert_32bit( volume->bytes_per_sector );

	return( sectors_per_chunk * bytes_per_sector );
}

