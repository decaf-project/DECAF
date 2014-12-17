/*
 * ewfverify
 * Verifies the integrity of the media data within the EWF file
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
#include <string.h>

#include "libewf.h"

#include "ewf_md5hash.h"

/* Prints the executable usage information
 */
void usage( void )
{
	fprintf( stderr, "Usage: ewfverify [ -hvV ] ewf_files\n" );

	fprintf( stderr, "\t-h: shows this help\n" );
	fprintf( stderr, "\t-v: verbose output to stderr\n" );
	fprintf( stderr, "\t-V: print version\n" );

	exit( EXIT_FAILURE );
}

/* Prints the executable version information
 */
void version( void )
{
	fprintf( stderr, "ewfverify version: %s\n", LIBEWF_VERSION );

	exit( EXIT_SUCCESS );
}

/* The main program
 */
int main( int argc, const char **argv )
{
	LIBEWF_HANDLE *handle;
	char *stored_md5hash_string;
	char *calculated_md5hash_string;
	int match_failed;

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
	handle                    = libewf_open( &argv[ optind ], ( argc - optind ), LIBEWF_OPEN_READ );
	stored_md5hash_string     = libewf_data_md5hash( handle );
	calculated_md5hash_string = libewf_calculate_md5hash( handle );

	libewf_close( handle );

	if( stored_md5hash_string == NULL )
	{
		stored_md5hash_string = "N/A";
	}
	fprintf( stderr, "MD5 hash stored in file:\t%s\n", stored_md5hash_string );
	fprintf( stderr, "MD5 hash calculated over data:\t%s\n", calculated_md5hash_string );

	match_failed = strncmp( stored_md5hash_string, calculated_md5hash_string, 32 ) != 0;

	if( match_failed )
	{
		fprintf( stderr, "\newfverify: FAILURE\n" );
	}
	else
	{
		fprintf( stderr, "\newfverify: SUCCESS\n" );
	}
	/* No need to free statical defined string N/A
	 */
	if( strlen( stored_md5hash_string ) > 3 )
	{
		free( stored_md5hash_string );
	}
	free( calculated_md5hash_string );

	return( match_failed );
}

