/*
 * EWF MD5 hash specification
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

#include <sys/types.h>

#include "notify.h"

#include "ewf_md5hash.h"

/* Allocates memory for a new efw md5 hash
 */
EWF_MD5HASH *ewf_md5hash_alloc( void )
{
	EWF_MD5HASH *md5hash = (EWF_MD5HASH *) malloc( EWF_MD5HASH_SIZE );

	if( md5hash == NULL )
	{
		LIBEWF_FATAL_PRINT( "ewf_md5hash_alloc: unable to allocate ewf_md5hash\n" );
	}
	memset( md5hash, 0, EWF_MD5HASH_SIZE );

	return( md5hash );
}

/* Frees memory of a md5 hash
 */
void ewf_md5hash_free( EWF_MD5HASH *md5hash )
{
	if( md5hash == NULL )
	{
		LIBEWF_FATAL_PRINT( "ewf_md5hash_free: invalid md5hash.\n" );
	}
	free( md5hash );
}

/* Writes the MD5 hash to a file descriptor
 * Returns a -1 on error, the amount of bytes written on success
 */
ssize_t ewf_md5hash_write( EWF_MD5HASH *md5hash, int file_descriptor )
{
	ssize_t count;

	if( md5hash == NULL )
	{
		LIBEWF_VERBOSE_PRINT( "ewf_md5hash_write: invalid md5hash.\n" );

		return( -1 );
	}
	count = write( file_descriptor, md5hash, EWF_MD5HASH_SIZE );

	if( count < EWF_MD5HASH_SIZE )
	{
		return( -1 );
	}
	return( count );
}

/* Converts the md5 hash to a printable string
 */
char *ewf_md5hash_to_string( EWF_MD5HASH *md5hash )
{
	/* an md5 hash consists of 32 characters */
	char *string       = (char *) malloc( 33 * sizeof( char ) );
	uint64_t iterator = 0;

	if( string == NULL )
	{
		LIBEWF_FATAL_PRINT( "evf_md5hash_to_string: unable to allocate string\n" );
	}
	for( iterator = 0; iterator < 16; iterator++ )
	{
		unsigned char md5char = *( md5hash + iterator );

		snprintf( &string[ iterator * 2 ], 3, "%02x", md5char );
	}
	return( string );
}

