/*
 * libewf handle
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

#ifndef _LIBEWF_HANDLE_H
#define _LIBEWF_HANDLE_H

#include <unistd.h>

#include "definitions.h"

#include "ewf_error2.h"
#include "ewf_header.h"
#include "ewf_header2.h"
#include "ewf_md5hash.h"
#include "ewf_sectors.h"
#include "offset_table.h"
#include "segment_table.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct libewf_handle LIBEWF_HANDLE;

struct libewf_handle
{
	/* The size of an individual chunk
	 */
	uint32_t chunk_size;

	/* The amount of sectors per chunk
	 */
	uint32_t sectors_per_chunk;

	/* The amount of bytes per sector
	 */
	uint32_t bytes_per_sector;

        /* The amount of chunks
         * consists of 4 bytes (32 bits)
         */
        uint32_t chunk_count;

        /* The amount of sectors
         * consists of 4 bytes (32 bits)
         */
        uint32_t sector_count;

	/* The maximum ewf file size
	 */
	uint64_t input_file_size;

	/* The maximum ewf file size
	 */
	uint64_t ewf_file_size;

	/* The maximum amount of chunks per file
	 */
	uint32_t chunks_per_file;

	/* The list of segment files
	 */
	LIBEWF_SEGMENT_TABLE *segment_table;

	/* The list of offsets within the segment files within the table sections
	 */
	LIBEWF_OFFSET_TABLE *offset_table;

	/* The list of offsets within the segment files within the table2 sections
	 */
	LIBEWF_OFFSET_TABLE *secondary_offset_table;

	/* The amount of the stored error2 sectors
	 */
	uint64_t error2_error_count;

	/* The stored error2 sectors
	 */
	EWF_ERROR2_SECTOR *error2_sectors;

	/* The stored header
	 */
	EWF_HEADER *header;

	/* The stored header2
	 */
	EWF_HEADER *header2;

	/* The stored MD5 hash of the data
	 */
	EWF_MD5HASH *md5hash;

	/* value to indicate if there is a CRC at the end of a chunk
	 */
	uint8_t chunk_crc;

	/* value to indicate if compression is used
	 */
	uint8_t compression_used;

	/* value to indicate if compression is used
	 */
	int8_t compression_level;

	/* value to indicate if the alternative read method
	 * should be used. The alternative read method does
	 * not use the MSB of an offset to determine if a 
	 * chunk is compressed but assumes a chunk is 
	 * compressed if its size is smaller than the
	 * defined chunk size
	 */
	uint8_t alternative_read_method;

	/* value to indicate which file format is used
	 */
	uint8_t format;

	/* value to indicate if the index has been build
	 */
	uint8_t index_build;

	/* A simple cache is implemented here to avoid having to read and decompress the
	 * same chunk while reading the data.
	 */
	EWF_SECTORS_CHUNK *raw_data;
	EWF_SECTORS_CHUNK *chunk_data;
	uint32_t cached_chunk;
	uint64_t cached_data_size;

	/* The media type
	 */
	uint32_t media_type;

	/* The GUID (Encase 5 only)
	 * consists of 16 bytes
	 */
	uint8_t guid[16];
};

#define LIBEWF_HANDLE_SIZE sizeof( LIBEWF_HANDLE )

LIBEWF_HANDLE *libewf_handle_alloc( uint32_t segment_amount );
LIBEWF_HANDLE *libewf_handle_cache_alloc( LIBEWF_HANDLE *handle, uint32_t size );
LIBEWF_HANDLE *libewf_handle_cache_realloc( LIBEWF_HANDLE *handle, uint32_t size );
LIBEWF_HANDLE *libewf_handle_cache_wipe( LIBEWF_HANDLE *handle, uint32_t size );
void libewf_handle_free( LIBEWF_HANDLE *handle );
uint8_t libewf_handle_is_set_header( LIBEWF_HANDLE *handle );
uint8_t libewf_handle_is_set_header2( LIBEWF_HANDLE *handle );
void libewf_handle_set_header( LIBEWF_HANDLE *handle, EWF_HEADER *header );
void libewf_handle_set_header2( LIBEWF_HANDLE *handle, EWF_HEADER *header2 );
void libewf_handle_set_md5hash( LIBEWF_HANDLE *handle, EWF_MD5HASH *md5hash );

#ifdef __cplusplus
}
#endif

#endif

