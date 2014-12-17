/*
 * EWF error2 section specification
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
#include "ewf_error2.h"

/* Allocates memory for a new efw error2 struct
 */
EWF_ERROR2 *ewf_error2_alloc( void )
{
	EWF_ERROR2 *error2 = (EWF_ERROR2 *) malloc( EWF_ERROR2_SIZE );

	if( error2 == NULL )
	{
		LIBEWF_FATAL_PRINT( "ewf_error2_alloc: unable to allocate ewf_error2.\n" );
	}
	memset( error2, 0, EWF_ERROR2_SIZE );

	return( error2 );
}

/* Allocates memory for a buffer of ewf error2 sectors 
 */
EWF_ERROR2_SECTOR *ewf_error2_sectors_alloc( uint32_t amount )
{
	size_t size               = EWF_ERROR2_SECTOR_SIZE * amount;
	EWF_ERROR2_SECTOR *sectors = (EWF_ERROR2_SECTOR *) malloc( size );

	if( sectors == NULL )
	{
		LIBEWF_FATAL_PRINT( "ewf_error2_sectors_alloc: unable to allocate sectors.\n" );
	}
	memset( sectors, 0, size );

	return( sectors );
}

/* Frees memory of a error2
 */
void ewf_error2_free( EWF_ERROR2 *error2 )
{
	if( error2 == NULL )
	{
		LIBEWF_FATAL_PRINT( "ewf_error2_free: invalid error2.\n" );
	}
	free( error2 );
}

/* Frees memory of a buffer of ewf error2 sectors
 */
void ewf_error2_sectors_free( EWF_ERROR2_SECTOR *sectors )
{
	if( sectors == NULL )
	{
		LIBEWF_FATAL_PRINT( "ewf_error2_sectors_free: invalid sectors.\n" );
	}
	free( sectors );
}

/* Reads the error2 from a file descriptor
 */
EWF_ERROR2 *ewf_error2_read( int file_descriptor )
{
	EWF_ERROR2 *error2 = ewf_error2_alloc();

	ssize_t count = read( file_descriptor, error2, EWF_ERROR2_SIZE );

	if( count < EWF_ERROR2_SIZE )
	{
		ewf_error2_free( error2 );

		LIBEWF_FATAL_PRINT( "ewf_error2_read: unable to read ewf_error2.\n" );
	}
	return( error2 );
}

/* Reads the ewf error2 sectors from a file descriptor
 */
EWF_ERROR2_SECTOR *ewf_error2_sectors_read( int file_descriptor, uint32_t amount )
{
	EWF_ERROR2_SECTOR *sectors = ewf_error2_sectors_alloc( amount );

	size_t size   = EWF_ERROR2_SECTOR_SIZE * amount;
	ssize_t count = read( file_descriptor, sectors, size );

	if( count < size )
	{
		ewf_error2_sectors_free( sectors );

		LIBEWF_FATAL_PRINT( "ewf_error2_sectors_read: unable to read sectors.\n" );
	}
	return( sectors );
}

/* Writes the error2 to a file descriptor, without the first sector
 * this should be written by the sectors write function
 * Returns a -1 on error, the amount of bytes written on success
 */
ssize_t ewf_error2_write( EWF_ERROR2 *error2, int file_descriptor )
{
	EWF_CRC crc;
	ssize_t count;
	size_t size = EWF_ERROR2_SIZE;

	if( error2 == NULL )
	{
		LIBEWF_VERBOSE_PRINT( "ewf_error2_write: invalid error2.\n" );

		return( -1 );
	}
	crc = ewf_crc( (void *) error2, ( size - EWF_CRC_SIZE ), 1 );

	revert_32bit( crc, error2->crc );

	count = write( file_descriptor, error2, size );

	if( count < size )
	{
		return( -1 );
	}
	return( count );
}

/* Writes the sectors to a file descriptor
 * Returns a -1 on error, the amount of bytes written on success
 */
ssize_t ewf_error2_sectors_write( EWF_ERROR2_SECTOR *sectors, int file_descriptor, uint32_t amount )
{
	ssize_t count;
	size_t size = EWF_ERROR2_SECTOR_SIZE * amount;

	if( sectors == NULL )
	{
		LIBEWF_VERBOSE_PRINT( "ewf_error2_sectors_write: invalid sectors.\n" );

		return( -1 );
	}
	count = write( file_descriptor, sectors, size );

	if( count < size )
	{
		return( -1 );
	}
	return( count );
}

