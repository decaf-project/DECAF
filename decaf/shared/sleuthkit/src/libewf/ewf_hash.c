/*
 * EWF hash section specification
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
#include "ewf_file_header.h"
#include "ewf_hash.h"

/* Allocates memory for a new efw hash struct
 */
EWF_HASH *ewf_hash_alloc( void )
{
	EWF_HASH *hash = (EWF_HASH *) malloc( EWF_HASH_SIZE );

	if( hash == NULL )
	{
		LIBEWF_FATAL_PRINT( "ewf_hash_alloc: unable to allocate ewf_hash.\n" );
	}
	memset( hash, 0, EWF_HASH_SIZE );

	memcpy( (uint8_t *) hash->signature, (uint8_t *) ewf_file_signature, 8 );

	return( hash );
}

/* Frees memory of a hash
 */
void ewf_hash_free( EWF_HASH *hash )
{
	if( hash == NULL )
	{
		LIBEWF_FATAL_PRINT( "ewf_hash_free: invalid hash.\n" );
	}
	free( hash );
}

/* Reads the hash from a file descriptor
 */
EWF_HASH *ewf_hash_read( int file_descriptor )
{
	EWF_HASH *hash = ewf_hash_alloc();

	ssize_t count = read( file_descriptor, hash, EWF_HASH_SIZE );

	if( count < EWF_HASH_SIZE )
	{
		ewf_hash_free( hash );

		LIBEWF_FATAL_PRINT( "ewf_hash_read: unable to read ewf_hash.\n" );
	}
	return( hash );
}

/* Writes the hash to a file descriptor
 * Returns a -1 on error, the amount of bytes written on success
 */
ssize_t ewf_hash_write( EWF_HASH *hash, int file_descriptor )
{
	EWF_CRC crc;
	ssize_t count;
	size_t size = EWF_HASH_SIZE;

	if( hash == NULL )
	{
		LIBEWF_VERBOSE_PRINT( "ewf_hash_write: invalid hash.\n" );

		return( -1 );
	}
	crc = ewf_crc( (void *) hash, ( size - EWF_CRC_SIZE ), 1 );

	revert_32bit( crc, hash->crc );

	count = write( file_descriptor, hash, size );

	if( count < size )
	{
		return( -1 );
	}
	return( count );
}

