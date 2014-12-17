/*
 * EWF CRC handling
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

#include "libewf_endian.h"
#include "notify.h"

#include "ewf_crc.h"

/* Calculates the EWF CRC
 * The original algorithm was taken from the ASR data web site
 * When calling this function to start a new CRC "previous_key" should be 1
 */
EWF_CRC ewf_crc( void *buffer, ssize_t buffer_size, uint32_t previous_key )
{
	uint8_t *cbuffer  = (uint8_t *) buffer;
	uint32_t b        = previous_key & 0xffff;
	uint32_t d        = ( previous_key >> 16 ) & 0xffff;
	uint64_t iterator = 0;

	while( iterator < buffer_size )
	{
		b += cbuffer[ iterator ];
		d += b;
    
		if( ( iterator != 0 ) && ( ( iterator % 0x15b0 == 0 ) || ( iterator == buffer_size - 1 ) ) )
		{
			b = b % 0xfff1;
			d = d % 0xfff1;
		}
		iterator++;
	}
	return( ( d << 16 ) | b );
}

/* Reads the CRC from a file descriptor
 */
EWF_CRC ewf_crc_read( int file_descriptor )
{
	uint8_t crc_bytes[4];
	EWF_CRC crc;

	ssize_t count = read( file_descriptor, crc_bytes, EWF_CRC_SIZE );

	if( count < EWF_CRC_SIZE )
	{
		LIBEWF_FATAL_PRINT( "ewf_crc_read: unable to read crc.\n" );
	}
	crc = convert_32bit( crc_bytes );

	return( crc );
}

/* Writes the CRC to a file descriptor
 * Returns a -1 on error, the amount of bytes written on success
 */
ssize_t ewf_crc_write( EWF_CRC crc, int file_descriptor )
{
	uint8_t crc_bytes[4];
	ssize_t count;
	size_t size = EWF_CRC_SIZE;

	revert_32bit( crc, crc_bytes );

	count = write( file_descriptor, crc_bytes, size );

	if( count < size )
	{
		return( -1 );
	}
	return( count );
}

