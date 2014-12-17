/*
 * libewf section list
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

#ifndef _SECTIONLIST_H
#define _SECTIONLIST_H

#include "definitions.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct libewf_section_list_entry LIBEWF_SECTION_LIST_ENTRY;

struct libewf_section_list_entry
{
	/* The file descriptor of the section
	 */
	int file_descriptor;

	/* The start offset of the section
	 */
	uint64_t start_offset;

	/* The end offset of the section
	 */
	uint64_t end_offset;

	/* The next section list entry
	 */
	LIBEWF_SECTION_LIST_ENTRY *next;
};

#define LIBEWF_SECTION_LIST_ENTRY_SIZE sizeof( LIBEWF_SECTION_LIST_ENTRY )

typedef struct libewf_section_list LIBEWF_SECTION_LIST;

struct libewf_section_list
{
	LIBEWF_SECTION_LIST_ENTRY *first, *last;
};

#define LIBEWF_SECTION_LIST_SIZE sizeof( LIBEWF_SECTION_LIST )

LIBEWF_SECTION_LIST_ENTRY *libewf_section_list_entry_alloc( void );
LIBEWF_SECTION_LIST *libewf_section_list_alloc( void );
void libewf_section_list_free( LIBEWF_SECTION_LIST *section_list );
LIBEWF_SECTION_LIST *libewf_section_list_append( LIBEWF_SECTION_LIST *section_list, int file_descriptor, uint32_t start_offset, uint32_t end_offset );

#ifdef __cplusplus
}
#endif

#endif

