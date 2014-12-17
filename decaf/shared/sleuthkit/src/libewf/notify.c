/*
 * libewf notification
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

#include "definitions.h"
#include "notify.h"

#ifdef __STDC__

#include <stdarg.h>

#define VARARGS( function, type, argument )	function( type argument, ... )
#define VASTART( argument_list, type, name )	va_start( argument_list, name )
#define VAEND( argument_list )			va_end( argument_list )

#else

#include <varargs.h>

#define VARARGS( function, type, argument )	function( va_alist ) va_dcl
#define VASTART( argument_list, type, name )	{ type name; va_start( argument_list ); name = va_arg( argument_list, type )
#define VAEND( argument_list )			va_end( argument_list ); }

#endif

#include "notify.h"

uint8_t libewf_verbose           = 0;
uint8_t libewf_warning_non_fatal = 0;

/* Print a remark on stderr and continue
 */
void VARARGS( libewf_verbose_print, char *, format )
{
	if( libewf_verbose != 0 )
	{
		va_list argument_list;

		VASTART( argument_list, char *, format );

		vfprintf( stderr, format, argument_list );

		VAEND( argument_list );
	}
}

/* Print error on stderr, does not terminate
 */
void VARARGS( libewf_warning_print, char *, format )
{
	va_list argument_list;

	VASTART( argument_list, char *, format );

	vfprintf( stderr, format, argument_list );

	VAEND( argument_list );

	if( libewf_warning_non_fatal != 0 )
	{
		exit( EXIT_FAILURE );
	}
}

/* Print a fatal error on stderr, and terminate
 */
void VARARGS( libewf_fatal_print, char *, format )
{
	va_list argument_list;

	VASTART( argument_list, char *, format );

	vfprintf( stderr, format, argument_list );

	VAEND( argument_list );

	exit( EXIT_FAILURE );
}

/* Prints a dump of data
 */
void libewf_dump_data( uint8_t *data, size_t size )
{
	uint32_t iterator = 0;

	while( iterator < size )
	{
		if( iterator % 16 == 0 )
		{
			fprintf( stderr, "%.8" PRIx32 ": ", iterator );
		}
		fprintf( stderr, "%.2x ", (unsigned char) *( data + iterator++ ) );

		if( iterator % 16 == 0 )
		{
			fprintf( stderr, "\n" );
		}
		else if( iterator % 8 == 0 )
		{
			fprintf( stderr, "  " );
		}
	}
	if( iterator % 16 != 0 )
	{
		fprintf( stderr, "\n" );
	}
	fprintf( stderr, "\n" );

	iterator = 0;

	while( iterator < size )
	{
		if( iterator % 32 == 0 )
		{
			fprintf( stderr, "%.8" PRIx32 ": ", iterator );
		}
		fprintf( stderr, "%c ", (unsigned char) *( data + iterator++ ) );

		if( iterator % 32 == 0 )
		{
			fprintf( stderr, "\n" );
		}
		else if( iterator % 8 == 0 )
		{
			fprintf( stderr, "  " );
		}
	}
	if( iterator % 32 != 0 )
	{
		fprintf( stderr, "\n" );
	}
	fprintf( stderr, "\n" );
}

