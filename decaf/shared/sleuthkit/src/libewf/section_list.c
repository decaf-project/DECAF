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

#include <stdio.h>
#include <stdlib.h>

#include "notify.h"

#include "section_list.h"

/* Allocates memory for a new section list entry struct
 */
LIBEWF_SECTION_LIST_ENTRY *libewf_section_list_entry_alloc( void )
{
	LIBEWF_SECTION_LIST_ENTRY *section_list_entry = (LIBEWF_SECTION_LIST_ENTRY *) malloc( LIBEWF_SECTION_LIST_ENTRY_SIZE );

	if( section_list_entry == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_section_list_entry_alloc: unable to allocate section_list_entry\n" );
	}
	section_list_entry->file_descriptor = -1;
	section_list_entry->start_offset    = 0;
	section_list_entry->end_offset      = 0;
	section_list_entry->next            = NULL;

	return( section_list_entry );
}

/* Allocates memory for a new section list struct
 */
LIBEWF_SECTION_LIST *libewf_section_list_alloc( void )
{
	LIBEWF_SECTION_LIST *section_list = (LIBEWF_SECTION_LIST *) malloc( LIBEWF_SECTION_LIST_SIZE );

	if( section_list == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_section_list_alloc: unable to allocate section_list\n" );
	}
	section_list->first = NULL;
	section_list->last = NULL;

	return( section_list );
}

/* Frees memory of a section list struct including elements
 */
void libewf_section_list_free( LIBEWF_SECTION_LIST *section_list )
{
	LIBEWF_SECTION_LIST_ENTRY *section_list_entry;

	if( section_list == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_section_list_free: invalid section list.\n" );
	}
	section_list_entry = section_list->first;

	while( section_list_entry != NULL )
	{
		LIBEWF_SECTION_LIST_ENTRY *current_section_list_entry = section_list_entry;

		section_list_entry = section_list_entry->next;

		free( current_section_list_entry );
	}
	free( section_list );
}

/* Append an entry to the section list
 */
LIBEWF_SECTION_LIST *libewf_section_list_append( LIBEWF_SECTION_LIST *section_list, int file_descriptor, uint32_t start_offset, uint32_t end_offset )
{
	LIBEWF_SECTION_LIST_ENTRY *section_list_entry;

	if( section_list == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_section_list_append: invalid section list.\n" );
	}
	section_list_entry = libewf_section_list_entry_alloc();

	section_list_entry->file_descriptor = file_descriptor;
	section_list_entry->start_offset    = start_offset;
	section_list_entry->end_offset      = end_offset;

	if( section_list->first == NULL )
	{
		section_list->first = section_list_entry;
	}
	if( section_list->last != NULL )
	{
		section_list->last->next = section_list_entry;
	}
	section_list->last = section_list_entry;

	return( section_list );
}
