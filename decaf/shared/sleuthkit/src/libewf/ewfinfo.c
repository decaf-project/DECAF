/*
 * ewfinfo
 * Shows information stored in an EWF file, like the header
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
#include <inttypes.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "libewf_endian.h"
#include "header_values.h"
#include "libewf.h"

/* Prints the executable usage information
 */
void usage( void )
{
	fprintf( stderr, "Usage: ewfinfo [ -dhimvV ] ewf_files\n" );

	fprintf( stderr, "\t-d: use a alternative date format month/day instead of day/month\n" );
	fprintf( stderr, "\t-e: only show EWF read error information\n" );
	fprintf( stderr, "\t-h: shows this help\n" );
	fprintf( stderr, "\t-i: only show EWF acquiry information\n" );
	fprintf( stderr, "\t-m: only show EWF media information\n" );
	fprintf( stderr, "\t-v: verbose output to stderr\n" );
	fprintf( stderr, "\t-V: print version\n" );

	exit( EXIT_FAILURE );
}

/* Prints the executable version information
 */
void version( void )
{
	fprintf( stderr, "ewfinfo version: %s\n", LIBEWF_VERSION );

	exit( EXIT_SUCCESS );
}

/* The main program
 */
int main( int argc, const char **argv )
{
	LIBEWF_HANDLE *handle                = NULL;
	LIBEWF_HEADER_VALUES *header_values  = NULL;
	LIBEWF_HEADER_VALUES *header2_values = NULL;
	int option                           = 0;
	int date_format                      = LIBEWF_DATEFORMAT_DAYMONTH;
	uint32_t iterator                    = 0;
	char info_option                     = 'a';

	while( ( option = getopt( argc, (char **) argv, "dhimvV" ) ) > 0 )
	{
		switch( option )
		{
			case '?':
			default:
				fprintf( stderr, "Invalid argument: %s\n", argv[ optind ] );
				usage();

			case 'd':
				date_format = LIBEWF_DATEFORMAT_MONTHDAY;
				break;

			case 'e':
				if( info_option != 'a' )
				{
					fprintf( stderr, "Conflicting options: %c and %c\n", option, info_option );
					usage();
				}
				info_option = 'e';
				break;

			case 'h':
				usage();

			case 'i':
				if( info_option != 'a' )
				{
					fprintf( stderr, "Conflicting options: %c and %c\n", option, info_option );
					usage();
				}
				info_option = 'i';
				break;

			case 'm':
				if( info_option != 'a' )
				{
					fprintf( stderr, "Conflicting options: %c and %c\n", option, info_option );
					usage();
				}
				info_option = 'm';
				break;

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
	handle = libewf_open( &argv[ optind ], ( argc - optind ), LIBEWF_OPEN_READ );

	/* Determine the file format
	 */
	if( handle->header != NULL )
	{
		header_values = libewf_header_values_parse_header( handle->header, date_format );

		if( header_values->acquiry_software_version != NULL )
		{
			if( handle->header[ 1 ] == '\r' )
			{
				if( header_values->acquiry_software_version[ 0 ] == '5' )
				{
					handle->format = LIBEWF_FORMAT_ENCASE5;
				}
				else if( header_values->acquiry_software_version[ 0 ] == '4' )
				{
					handle->format = LIBEWF_FORMAT_ENCASE4;
				}
				else if( header_values->acquiry_software_version[ 0 ] == '3' )
				{
					handle->format = LIBEWF_FORMAT_ENCASE3;
				}
				else if( header_values->acquiry_software_version[ 0 ] == '2' )
				{
					handle->format = LIBEWF_FORMAT_ENCASE2;
				}
				else if( header_values->acquiry_software_version[ 0 ] == '1' )
				{
					handle->format = LIBEWF_FORMAT_ENCASE1;
				}
			}
			else if( handle->header[ 1 ] == '\n' )
			{
				handle->format = LIBEWF_FORMAT_FTK;
			}
		}
	}
	if( handle->header2 != NULL )
	{
		header2_values = libewf_header_values_parse_header( handle->header2, date_format );

		if( handle->header2[ 0 ] == '3' )
		{
			handle->format = LIBEWF_FORMAT_ENCASE5;
		}
		else if( handle->header2[ 0 ] == '1' )
		{
			handle->format = LIBEWF_FORMAT_ENCASE4;
		}
	}
	if( libewf_verbose == 1 )
	{
		switch( handle->format )
		{
			case LIBEWF_FORMAT_FTK:
				fprintf( stdout, "File format:\t\t\tFTK Imager\n\n" );
				break;

			case LIBEWF_FORMAT_ENCASE1:
				fprintf( stdout, "File format:\t\t\tEncase 1\n\n" );
				break;

			case LIBEWF_FORMAT_ENCASE2:
				fprintf( stdout, "File format:\t\t\tEncase 2\n\n" );
				break;

			case LIBEWF_FORMAT_ENCASE3:
				fprintf( stdout, "File format:\t\t\tEncase 3\n\n" );
				break;

			case LIBEWF_FORMAT_ENCASE4:
				fprintf( stdout, "File format:\t\t\tEncase 4\n\n" );
				break;

			case LIBEWF_FORMAT_ENCASE5:
				fprintf( stdout, "File format:\t\t\tEncase 5\n\n" );
				break;

			case LIBEWF_FORMAT_UNKNOWN:
			default:
				fprintf( stdout, "File format:\t\t\tunknown\n\n" );
				break;

		}
	}
	if( ( info_option == 'a' ) || ( info_option == 'i' ) )
	{
		fprintf( stdout, "Acquiry information\n" );

		if( header2_values != NULL )
		{
			libewf_header_values_fprint( stdout, header2_values );
		}
		else if( header_values != NULL )
		{
			libewf_header_values_fprint( stdout, header_values );
		}
		else
		{
			fprintf( stdout, "\tNo information found in file.\n" );
		}
		fprintf( stdout, "\n" );

		libewf_header_values_free( header_values );
	}
	if( ( info_option == 'a' ) || ( info_option == 'm' ) )
	{
		fprintf( stdout, "Media information\n" );

		if( handle->media_type == 0x0000001 )
		{
			fprintf( stdout, "\tMedia type:\t\tremovable disk\n" );
		}
		else if( handle->media_type == 0x0000300 )
		{
			fprintf( stdout, "\tMedia type:\t\tfixed disk\n" );
		}
		else
		{
			fprintf( stdout, "\tMedia type:\t\tunknown\n" );
		}
		fprintf( stdout, "\tAmount of sectors:\t%" PRIu32 "\n", handle->sector_count );

		if( handle->format == LIBEWF_FORMAT_ENCASE5 )
		{
			if( handle->compression_level == EWF_COMPRESSION_NONE )
			{
				fprintf( stdout, "\tCompression type:\tnone compression\n" );
			}
			else if( handle->compression_level == EWF_COMPRESSION_FAST )
			{
				fprintf( stdout, "\tCompression type:\tgood (fast) compression\n" );
			}
			else if( handle->compression_level == EWF_COMPRESSION_BEST )
			{
				fprintf( stdout, "\tCompression type:\tbest compression\n" );
			}
			else
			{
				fprintf( stdout, "\tCompression type:\tunknown compression\n" );
			}
			fprintf( stdout, "\tGUID:\t\t\t%.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x\n",
				handle->guid[ 0 ], handle->guid[ 1 ], handle->guid[ 2 ], handle->guid[ 3 ],
				handle->guid[ 4 ], handle->guid[ 5 ], handle->guid[ 6 ], handle->guid[ 7 ],
				handle->guid[ 8 ], handle->guid[ 9 ], handle->guid[ 10 ], handle->guid[ 11 ],
				handle->guid[ 12 ], handle->guid[ 13 ], handle->guid[ 14 ], handle->guid[ 15 ]
			);
		}
		fprintf( stdout, "\n" );
	}
	if( ( info_option == 'a' ) || ( info_option == 'e' ) )
	{
		if( handle->error2_error_count > 0 )
		{
			fprintf( stdout, "Read errors during acquiry:\n" );
			fprintf( stdout, "\ttotal amount: %" PRIu64 "\n", handle->error2_error_count );
			
			for( iterator = 0; iterator < handle->error2_error_count; iterator++ )
			{
				uint32_t sector       = convert_32bit( handle->error2_sectors[ iterator ].sector );
				uint32_t sector_count = convert_32bit( handle->error2_sectors[ iterator ].sector_count );

				fprintf( stdout, "\tin sector(s): %" PRIu32 " - %" PRIu32 " amount: %" PRIu32 "\n", sector, ( sector + sector_count ), sector_count );
			}
			fprintf( stdout, "\n" );
		}
	}
	libewf_close( handle );

	return( EXIT_SUCCESS );
}

