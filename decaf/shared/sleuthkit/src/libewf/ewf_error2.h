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

#ifndef _EWFERROR2_H
#define _EWFERROR2_H

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ewf_error2 EWF_ERROR2;
typedef struct ewf_error2_sector EWF_ERROR2_SECTOR;

struct ewf_error2
{
	/* The error count
	 * consists of 4 bytes (32 bits)
	 */
	uint8_t error_count[4];

	/* Unknown
	 * consists of 512 bytes
	 * value should be 0x00
	 */
	uint8_t unknown[512];

	/* The section crc of all (previous) error2 data
	 * consits of 4 bytes
	 * starts with sector 76
	 */
	uint8_t crc[4];

	/* The sector array
	 * consits of 8 bytes per sector
	 * as long as necessary
	 */

	/* The last sector is followed by a 4 byte CRC
	 */

} __attribute__((packed));

struct ewf_error2_sector
{
	/* A error2 sector
	 * consists of 4 bytes (32 bits)
	 */
	uint8_t sector[4];

	/* The sector count
	 * consists of 4 bytes (32 bits)
	 */
	uint8_t sector_count[4];

} __attribute__((packed));

#define EWF_ERROR2_SIZE sizeof( EWF_ERROR2 )
#define EWF_ERROR2_SECTOR_SIZE sizeof( EWF_ERROR2_SECTOR )

EWF_ERROR2 *ewf_error2_alloc( void );
EWF_ERROR2_SECTOR *ewf_error2_sectors_alloc( uint32_t amount );
void ewf_error2_free( EWF_ERROR2 *error2 );
void ewf_error2_sectors_free( EWF_ERROR2_SECTOR *sectors );
EWF_ERROR2 *ewf_error2_read( int file_descriptor );
EWF_ERROR2_SECTOR *ewf_error2_sectors_read( int file_descriptor, uint32_t amount );
ssize_t ewf_error2_sectors_write( EWF_ERROR2_SECTOR *sectors, int file_descriptor, uint32_t amount );
ssize_t ewf_error2_write( EWF_ERROR2 *error2, int file_descriptor );

#ifdef __cplusplus
}
#endif

#endif

