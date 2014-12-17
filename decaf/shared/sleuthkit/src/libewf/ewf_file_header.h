/*
 * EWF file header specification
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

#ifndef _EWFFILEHEADER_H
#define _EWFFILEHEADER_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t ewf_file_signature[];
extern uint8_t lwf_file_signature[];

typedef struct ewf_file_header EWF_FILE_HEADER;

struct ewf_file_header
{
	/* The EWF file signature (magic header)
	 * consists of 8 bytes containing
         * EVF 0x09 0x0d 0x0a 0xff 0x00
	 */
	uint8_t signature[8];

	/* The fields start
	 * consists of 1 byte (8 bit) containing
	 * 0x01
	 */ 
	uint8_t fields_start;

	/* The fields segment number
	 * consists of 2 bytes (16 bits) containing
	 */ 
	uint8_t fields_segment[2];

	/* The fields end
	 * consists of 2 bytes (16 bits) containing
	 * 0x00 0x00
	 */ 
	uint8_t fields_end[2];

} __attribute__((packed));

#define EWF_FILE_HEADER_SIZE sizeof( EWF_FILE_HEADER )

EWF_FILE_HEADER *ewf_file_header_alloc( void );
void ewf_file_header_free( EWF_FILE_HEADER *file_header );
uint8_t ewf_file_header_check_signature( uint8_t *signature );
EWF_FILE_HEADER *ewf_file_header_read( int file_descriptor );
ssize_t ewf_file_header_write( EWF_FILE_HEADER *file_header, int file_descriptor );

#ifdef __cplusplus
}
#endif

#endif

