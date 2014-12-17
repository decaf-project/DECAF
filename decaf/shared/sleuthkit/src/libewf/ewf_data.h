/*
 * EWF data section specification
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

#ifndef _EWFDATA_H
#define _EWFDATA_H

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ewf_data EWF_DATA;

struct ewf_data
{
	/* Unknown
	 * consists of 4 bytes (32 bits)
	 * value should be 0x00
	 */
	uint8_t unknown1[4];

	/* The amount of chunks
	 * consists of 4 bytes (32 bits)
	 */
	uint8_t chunk_count[4];

	/* The amount of sectors per chunks
	 * consists of 4 bytes (32 bits)
	 * value should be 64
	 */
	uint8_t sectors_per_chunk[4];

	/* The amount of bytes per chunks
	 * consists of 4 bytes (32 bits)
	 * value should be 512
	 */
	uint8_t bytes_per_sector[4];

	/* The amount of sectors
	 * consists of 4 bytes (32 bits)
	 */
	uint8_t sector_count[4];

	/* Unknown
	 * consists of 16 bytes
	 * contains 0x00
	 */
	uint8_t unknown2[16];

	/* Unknown
	 * consists of 4 bytes
	 * varies in data
	 */
	uint8_t unknown3[4];

	/* Unknown
	 * consists of 12 bytes
	 * contains 0x00
	 */
	uint8_t unknown4[12];

	/* Compression level (Encase 5 only)
	 * consists of 1 byte
	 * value is 0x00 for no compression, 0x01 for good compression, 0x02 for best compression
	 */
	uint8_t compression_level;

	/* Unknown
	 * consists of 3 bytes
	 * contains 0x00
	 */
	uint8_t unknown5[3];

	/* The error block size
	 * consists of 4 bytes (32 bits)
	 */
	uint8_t error_block_size[4];

	/* Unknown
	 * consists of 4 bytes
	 * contains 0x00
	 */
	uint8_t unknown6[4];

	/* The GUID (Encase 5 only)
	 * consists of 16 bytes
	 */
	uint8_t guid[16];

	/* Unknown
	 * consists of 963 bytes
	 * contains 0x00
	 */
	uint8_t unknown7[963];

	/* Reserved (signature)
	 * consists of 5 bytes
	 */
	uint8_t signature[5];

	/* The section crc of all (previous) data section data
	 * consits of 4 bytes (32 bits)
	 * starts with offset 76
	 */
	uint8_t crc[4];

} __attribute__((packed));

#define EWF_DATA_SIZE sizeof( EWF_DATA )

EWF_DATA *ewf_data_alloc( void );
void ewf_data_free( EWF_DATA *data );
EWF_DATA *ewf_data_read( int file_descriptor );
ssize_t ewf_data_write( EWF_DATA *data, int file_descriptor );

#ifdef __cplusplus
}
#endif

#endif

