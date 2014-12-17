/*
 * libewf header values
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

#ifndef _LIBEWF_HEADERVALUES_H
#define _LIBEWF_HEADERVALUES_H

#include <unistd.h>

#include "ewf_header.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LIBEWF_FORMAT_UNKNOWN 0x00
#define LIBEWF_FORMAT_ENCASE1 0x01
#define LIBEWF_FORMAT_ENCASE2 0x02
#define LIBEWF_FORMAT_ENCASE3 0x03
#define LIBEWF_FORMAT_ENCASE4 0x04
#define LIBEWF_FORMAT_ENCASE5 0x05
#define LIBEWF_FORMAT_FTK     0x0F

#define LIBEWF_DATEFORMAT_DAYMONTH 0x01
#define LIBEWF_DATEFORMAT_MONTHDAY 0x02

#define LIBEWF_COMPRESSIONTYPE_NONE "n"
#define LIBEWF_COMPRESSIONTYPE_FAST "f"
#define LIBEWF_COMPRESSIONTYPE_BEST "b"

typedef struct libewf_header_values LIBEWF_HEADER_VALUES;

struct libewf_header_values
{
	/* Case number
	 */
	char *case_number;

	/* Description
	 */
	char *description;

	/* Examiner name
	 */
	char *examiner_name;

	/* Evidence number
	 */
	char *evidence_number;

	/* Notes
	 */
	char *notes;

	/* Acquiry date
	 */
	char *acquiry_date;

	/* System date
	 */
	char *system_date;

	/* Acquiry operating system
	 */
	char *acquiry_operating_system;

	/* Acquiry software version
	 */
	char *acquiry_software_version;

	/* Password
	 */
	char *password;

	/* Compression type
	 */
	char *compression_type;

	/* Unknown dc
	 */
	char *unknown_dc;
};

#define LIBEWF_HEADER_VALUES_SIZE sizeof( LIBEWF_HEADER_VALUES )

LIBEWF_HEADER_VALUES *libewf_header_values_alloc( void );
char *libewf_header_value_string_alloc( uint32_t amount );
void libewf_header_values_free( LIBEWF_HEADER_VALUES *header_values );

char **libewf_split_string( char *string, int delimiter, uint32_t *amount );
char *libewf_convert_date_header_value( char *header_value, uint8_t date_format );
char *libewf_generate_date_header_value( void );
char *libewf_convert_date_header2_value( char *header_value, uint8_t date_format );
char *libewf_generate_date_header2_value( void );
char *libewf_header_values_set_value( char* header_value, char *value );

LIBEWF_HEADER_VALUES *libewf_header_values_parse_header( EWF_HEADER *header, uint8_t date_format );
void libewf_header_values_fprint( FILE *stream, LIBEWF_HEADER_VALUES *header_values );

EWF_HEADER *libewf_header_values_generate_header_string_encase3( LIBEWF_HEADER_VALUES *header_values, uint8_t compression_level );
EWF_HEADER *libewf_header_values_generate_header_string_encase4( LIBEWF_HEADER_VALUES *header_values );
EWF_HEADER *libewf_header_values_generate_header2_string_encase5( LIBEWF_HEADER_VALUES *header_values );
EWF_HEADER *libewf_header_values_generate_header_string_ftk( LIBEWF_HEADER_VALUES *header_values, uint8_t compression_level );

#ifdef __cplusplus
}
#endif

#endif

