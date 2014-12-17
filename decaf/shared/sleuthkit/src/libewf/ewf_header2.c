/*
 * EWF header2 section specification
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

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "notify.h"

#include "ewf_compress.h"
#include "ewf_header2.h"

/* Convert the UTF16 ewf header2 into an ASCII ewf header format
 */
EWF_HEADER *ewf_header2_convert_utf16_to_ascii( EWF_HEADER *utf16_header, size_t size_utf16 )
{
	/* The UTF16 header contains twice as much bytes
	 * needed for the ASCII header
	 * an additional byte required for end of string
	 * the UTF16 string should contain two bytes representing byte order
	 */
	size_t size_ascii        = ( size_utf16 / 2 ) + 1;
	EWF_HEADER *ascii_header = ewf_header_alloc( size_ascii );
	uint8_t byte_order       = 0;
	uint32_t utf16_iterator  = 2;
	uint32_t ascii_iterator  = 0;

	/* Check if UTF16 string is in big or little endian
	 */
	if( utf16_header[ 0 ] == 0xff && utf16_header[ 1 ] == 0xfe )
	{
		byte_order = 'l';
	}
	else if( utf16_header[ 0 ] == 0xfe && utf16_header[ 1 ] == 0xff )
	{
		byte_order = 'b';
	}
	else
	{
		LIBEWF_FATAL_PRINT( "ewf_header2_convert_utf16_to_ascii: no byte order in UTF16 string.\n" );
	}
	while( utf16_iterator < size_utf16 )
	{
		if( byte_order == 'b' )
		{
			if( utf16_header[ utf16_iterator ] == 0 )
			{
				ascii_header[ ascii_iterator ] = utf16_header[ utf16_iterator + 1 ];
			}
			else
			{
				/* Add a place holder character
				 */
				ascii_header[ ascii_iterator ] = '_';
			}
		}
		else if( byte_order == 'l' )
		{
			if( utf16_header[ utf16_iterator + 1 ] == 0 )
			{
				ascii_header[ ascii_iterator ] = utf16_header[ utf16_iterator ];
			}
			else
			{
				/* Add a place holder character
				 */
				ascii_header[ ascii_iterator ] = '_';
			}
		}
		utf16_iterator += 2;
		ascii_iterator += 1;
	}
	ascii_header[ ascii_iterator ] = 0;

	return( ascii_header );
}

/* Convert the ASCII ewf header into an UTF16 ewf header format
 */
EWF_HEADER *ewf_header2_convert_ascii_to_utf16( EWF_HEADER *ascii_header, size_t size_ascii )
{
	/* Two additional bytes required for end of string and 2 for the byte order indicator
	 */
	size_t size_utf16        = ( size_ascii * 2 ) + 4;
	EWF_HEADER *utf16_header = ewf_header_alloc( size_utf16 );
	uint32_t ascii_iterator  = 0;
	uint32_t utf16_iterator  = 2;

	/* Add the little endian byte order
	 */
	utf16_header[ 0 ] = 0xff;
	utf16_header[ 1 ] = 0xfe;

	while( ascii_iterator < size_ascii )
	{
		utf16_header[ utf16_iterator     ] = 0;
		utf16_header[ utf16_iterator + 1 ] = ascii_header[ ascii_iterator ];

		ascii_iterator += 1;
		utf16_iterator += 2;
	}
	utf16_header[ utf16_iterator     ] = 0;
	utf16_header[ utf16_iterator + 1 ] = 0;

	return( utf16_header );
}

/* Reads the header2 from a file descriptor
 */
EWF_HEADER *ewf_header2_read( int file_descriptor, size_t size )
{
	EWF_HEADER *uncompressed_header = ewf_header_read( file_descriptor, &size );
	EWF_HEADER *ascii_header        = ewf_header2_convert_utf16_to_ascii( uncompressed_header, size );

	ewf_header_free( uncompressed_header );

	return( ascii_header );
}

