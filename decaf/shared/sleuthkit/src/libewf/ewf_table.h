/*
 * EWF table section specification
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

#ifndef _EWFTABLE_H
#define _EWFTABLE_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EWF_OFFSET_COMPRESSED_READ_MASK  0x7fffffff
#define EWF_OFFSET_COMPRESSED_WRITE_MASK 0x80000000

typedef struct ewf_table EWF_TABLE;
typedef struct ewf_table_offset EWF_TABLE_OFFSET;

struct ewf_table
{
	/* Chunk count for the table
	 * consists of 4 bytes (32 bits)
	 */
	uint8_t chunk_count[4];

	/* Padding
	 * consists of 16 bytes
	 * value should be 0x00
	 */
	uint8_t padding[16];

	/* The section crc of all (previous) table data
	 * consits of 4 bytes
	 * starts with offset 76
	 */
	uint8_t crc[4];

	/* The offset array
	 * consits of 4 bytes per offset
	 * as long as necessary
	 * can contain 16375 entries per table
	 */

	/* The last offset is followed by a 4 byte CRC
	 */

} __attribute__((packed));

struct ewf_table_offset
{
	/* An offset
	 * consits of 4 bytes
	 */
	uint8_t offset[4];

} __attribute__((packed));

#define EWF_TABLE_SIZE sizeof( EWF_TABLE )
#define EWF_TABLE_OFFSET_SIZE sizeof( EWF_TABLE_OFFSET )

EWF_TABLE *ewf_table_alloc( void );
EWF_TABLE_OFFSET *ewf_table_offsets_alloc( uint32_t amount );
void ewf_table_free( EWF_TABLE *table );
void ewf_table_offsets_free( EWF_TABLE_OFFSET *offsets );
EWF_TABLE *ewf_table_read( int file_descriptor );
EWF_TABLE_OFFSET *ewf_table_offsets_read( int file_descriptor, uint32_t amount );
ssize_t ewf_table_offsets_write( EWF_TABLE_OFFSET *offsets, int file_descriptor, uint32_t amount );
ssize_t ewf_table_write( EWF_TABLE *table, int file_descriptor );

#ifdef __cplusplus
}
#endif

#endif

