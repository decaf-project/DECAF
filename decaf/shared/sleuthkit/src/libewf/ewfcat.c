/*
 * ewfcat
 * Cat's data from EWF files to stdout
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include "libewf.h"

void usage( void )
{
	fprintf( stderr, "Usage: ewfcat [ -hvV ] ewf_files\n" );

	fprintf( stderr, "\t-h: shows this help\n" );
	fprintf( stderr, "\t-v: verbose output to stderr\n" );
	fprintf( stderr, "\t-V: print version\n" );

	exit( EXIT_FAILURE );
}

void version( void )
{
	fprintf( stderr, "ewfcat version: 20060410\n" );

	exit( EXIT_SUCCESS );
}

int main(int argc, const char **argv)
{
	int option = 0;

	while( ( option = getopt( argc, (char **) argv, "hvV" ) ) > 0 )
	{
		switch( option )
		{
			case '?':
			default:
				fprintf( stderr, "Invalid argument: %s\n", argv[ optind ] );
				usage();

			case 'h':
				usage();

			case 'v':
				libewf_verbose = 1;
				break;

			case 'V':
				version();
		}
	}
	if( optind == argc )
	{
		fprintf( stderr, "Missing EWF image files.\n" );
		usage();
	}
	LIBEWF_HANDLE *handle = libewf_open( &argv[ optind ], ( argc - optind ), LIBEWF_OPEN_READ );
	LIBEWF_SSIZE_T count  = libewf_read_to_file_descriptor( handle, 1 );

	libewf_close( handle );

	fprintf( stderr, "Success: bytes written: %lli\n", count );

	return( 0 );
}

