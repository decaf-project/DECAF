/*
 * EWF section start (header) specification
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

#ifndef _EWFSECTION_H
#define _EWFSECTION_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ewf_section EWF_SECTION;

struct ewf_section
{
	/* The section type string
	 * consists of 16 bytes
	 */
	uint8_t type[16];

	/* The section offset to the next section
	 * consists of 8 bytes (64 bits)
	 */
	uint8_t next[8];

	/* The section size
	 * consists of 8 bytes (64 bits)
	 */
	uint8_t size[8];

	/* The section padding
	 * consists of 40 bytes
	 */
	uint8_t padding[40];

	/* The section crc of all (previous) section data
	 * consists of 4 bytes
	 */
	uint8_t crc[4];

} __attribute__((packed));

#define EWF_SECTION_SIZE sizeof( EWF_SECTION )

EWF_SECTION *ewf_section_alloc( void );
void ewf_section_free( EWF_SECTION *section );
EWF_SECTION *ewf_section_read( int file_descriptor );
ssize_t ewf_section_write( EWF_SECTION *section, int file_descriptor );

uint8_t ewf_section_is_type( EWF_SECTION *section, const char *type );
uint8_t ewf_section_is_type_header( EWF_SECTION *section );
uint8_t ewf_section_is_type_header2( EWF_SECTION *section );
uint8_t ewf_section_is_type_volume( EWF_SECTION *section );
uint8_t ewf_section_is_type_disk( EWF_SECTION *section );
uint8_t ewf_section_is_type_table( EWF_SECTION *section );
uint8_t ewf_section_is_type_table2( EWF_SECTION *section );
uint8_t ewf_section_is_type_sectors( EWF_SECTION *section );
uint8_t ewf_section_is_type_hash( EWF_SECTION *section );
uint8_t ewf_section_is_type_done( EWF_SECTION *section );
uint8_t ewf_section_is_type_next( EWF_SECTION *section );
uint8_t ewf_section_is_type_data( EWF_SECTION *section );
uint8_t ewf_section_is_type_error2( EWF_SECTION *section );

void ewf_section_fprint( FILE *stream, EWF_SECTION *section );

#ifdef __cplusplus
}
#endif

#endif

