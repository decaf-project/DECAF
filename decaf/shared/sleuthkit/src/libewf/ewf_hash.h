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

#ifndef _EWFHASH_H
#define _EWFHASH_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ewf_hash EWF_HASH;

struct ewf_hash
{
	/* The MD5 hash of the aquired data
	 * consists of 16 bytes
	 */
	uint8_t md5hash[16];

	/* TODO: unknown find out
	 * only in Encase5 ?
	 */
	uint8_t unknown1[4];

	/* TODO: unknown find out
	 * in EWF format from Encase4 differentiates when a password or none is set
	 * does not equal the CRC over the media data
	 * but looks like a CRC value nontheless
	 */
	uint8_t unknown2[4];

	/* The EWF file signature (magic header)
	 * consists of 8 bytes containing
         * EVF 0x09 0x0d 0x0a 0xff 0x00
	 * holds for Encase 5
	 * the value is empty in Encase 3
	 * the value does contain different data in Encase 4: 10 25 17 01 54 72 53 00 - always this value ?
	 */
	uint8_t signature[8];

	/* The section crc of all (previous) hash data
	 * consits of 4 bytes (32 bits)
	 * starts with offset 76
	 */
	uint8_t crc[4];

} __attribute__((packed));

#define EWF_HASH_SIZE sizeof( EWF_HASH )

EWF_HASH *ewf_hash_alloc( void );
void ewf_hash_free( EWF_HASH *hash );
EWF_HASH *ewf_hash_read( int file_descriptor );
ssize_t ewf_hash_write( EWF_HASH *hash, int file_descriptor );

#ifdef __cplusplus
}
#endif

#endif

