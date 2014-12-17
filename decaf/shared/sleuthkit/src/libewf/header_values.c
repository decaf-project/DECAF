/*
 * libewf header values
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

#include <unistd.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "definitions.h"
#include "notify.h"

#include "ewf_compress.h"
#include "header_values.h"

/* Allocates memory for a new header values struct
 */
LIBEWF_HEADER_VALUES *libewf_header_values_alloc( void )
{
	LIBEWF_HEADER_VALUES *header_values = (LIBEWF_HEADER_VALUES *) malloc( LIBEWF_HEADER_VALUES_SIZE );

	if( header_values == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_header_values_alloc: unable to allocate ewf_header_values\n" );
	}
	header_values->case_number               = NULL;
	header_values->description               = NULL;
	header_values->examiner_name             = NULL;
	header_values->evidence_number           = NULL;
	header_values->notes                     = NULL;
	header_values->acquiry_date              = NULL;
	header_values->system_date               = NULL;
	header_values->acquiry_operating_system  = NULL;
	header_values->acquiry_software_version  = NULL;
	header_values->password                  = NULL;
	header_values->compression_type          = NULL;
	header_values->unknown_dc                = NULL;

	return( header_values );
}

/* Allocates memory for a header value string
 */
char *libewf_header_value_string_alloc( uint32_t amount )
{
	char *header_value_string = (char *) malloc( sizeof( char ) * amount );

	if( header_value_string == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_header_value_string_alloc: unable to allocate header value string\n" );
	}
	return( header_value_string );
}

/* Frees memory of a header values struct including elements
 */
void libewf_header_values_free( LIBEWF_HEADER_VALUES *header_values )
{
	if( header_values == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_header_values_free: invalid header values\n" );
	}
	if( header_values->case_number != NULL )
	{
		free( header_values->case_number );
	}
	if( header_values->description != NULL )
	{
		free( header_values->description );
	}
	if( header_values->examiner_name != NULL )
	{
		free( header_values->examiner_name );
	}
	if( header_values->evidence_number != NULL )
	{
		free( header_values->evidence_number );
	}
	if( header_values->notes != NULL )
	{
		free( header_values->notes );
	}
	if( header_values->acquiry_date != NULL )
	{
		free( header_values->acquiry_date );
	}
	if( header_values->system_date != NULL )
	{
		free( header_values->system_date );
	}
	if( header_values->acquiry_operating_system != NULL )
	{
		free( header_values->acquiry_operating_system );
	}
	if( header_values->acquiry_software_version != NULL )
	{
		free( header_values->acquiry_software_version );
	}
	if( header_values->password != NULL )
	{
		free( header_values->password );
	}
	if( header_values->compression_type != NULL )
	{
		free( header_values->compression_type );
	}
	if( header_values->unknown_dc != NULL )
	{
		free( header_values->unknown_dc );
	}
	free( header_values );
}

/* Split a string into elements using a delimiter character
 */
char **libewf_split_string( char *string, int delimiter, uint32_t *amount )
{
	char **lines = NULL;

	if( string != NULL )
	{
		uint32_t iterator = 0;
		size_t line_size   = 0;
		size_t string_size = strlen( string );
		char *line_start   = string;
		char *line_end     = string;
		char *string_end   = &string[ string_size ];

		while( 1 )
		{
			line_end = strchr( line_start, delimiter );

			iterator++;

			if( line_end == NULL )
			{
				break;
			}
			if( line_end == line_start )
			{
				line_start += 1;
			}
			else if( line_end != string )
			{
				line_start = line_end + 1;
			}
		}
		*amount = iterator;
		lines   = (char **) malloc( sizeof( char * ) * *amount );

		if( lines == NULL )
		{
			LIBEWF_FATAL_PRINT( "libewf_split_string: unable to allocate dynamic array lines.\n" );
		}
		line_start = string;
		line_end   = string;
		iterator   = 0;

		while( iterator < *amount )
		{
			if( line_end != string )
			{
				line_start = line_end + 1;
			}
			line_end = strchr( line_start, delimiter );

			/* Check for last value
			 */
			if( line_end == NULL )
			{
				line_size = string_end - line_start;
			}
			else
			{
				line_size = line_end - line_start;
			}
			lines[ iterator ] = libewf_header_value_string_alloc( line_size + 1 );

			if( line_size > 0 )
			{
				memcpy( (char *) lines[ iterator ], (char *) line_start, line_size );
			}
			lines[ iterator ][ line_size ] = '\0';

			iterator++;
		}
	}
	return( lines );
}

/* Convert date string within a header value
 */
char *libewf_convert_date_header_value( char *header_value, uint8_t date_format )
{
	char **date_elements;
	char *date_string;
	uint32_t date_element_count    = 0;
	uint32_t date_element_iterator = 0;

	if( header_value == NULL )
	{
		return( NULL );
	}
	date_elements = libewf_split_string( header_value, ' ', &date_element_count );

	if( date_element_count != 6 )
	{
		return( NULL );
	}
	date_string = libewf_header_value_string_alloc( 20 );

	if( date_format == LIBEWF_DATEFORMAT_MONTHDAY )
	{
		if( strlen( date_elements[ 1 ] ) == 1 )
		{
			date_string[ 0 ] = '0';
			date_string[ 1 ] = date_elements[ 1 ][ 0 ];
		}
		else
		{
			date_string[ 0 ] = date_elements[ 1 ][ 0 ];
			date_string[ 1 ] = date_elements[ 1 ][ 1 ];
		}
		date_string[ 2 ] = '/';

		if( strlen( date_elements[ 2 ] ) == 1 )
		{
			date_string[ 3 ] = '0';
			date_string[ 4 ] = date_elements[ 2 ][ 0 ];
		}
		else
		{
			date_string[ 3 ] = date_elements[ 2 ][ 0 ];
			date_string[ 4 ] = date_elements[ 2 ][ 1 ];
		}
	}
	else if( date_format == LIBEWF_DATEFORMAT_DAYMONTH )
	{
		if( strlen( date_elements[ 2 ] ) == 1 )
		{
			date_string[ 0 ] = '0';
			date_string[ 1 ] = date_elements[ 2 ][ 0 ];
		}
		else
		{
			date_string[ 0 ] = date_elements[ 2 ][ 0 ];
			date_string[ 1 ] = date_elements[ 2 ][ 1 ];
		}
		date_string[ 2 ] = '/';

		if( strlen( date_elements[ 1 ] ) == 1 )
		{
			date_string[ 3 ] = '0';
			date_string[ 4 ] = date_elements[ 1 ][ 0 ];
		}
		else
		{
			date_string[ 3 ] = date_elements[ 1 ][ 0 ];
			date_string[ 4 ] = date_elements[ 1 ][ 1 ];
		}
	}
	date_string[ 5  ] = '/';
	date_string[ 6  ] = date_elements[ 0 ][ 0 ];
	date_string[ 7  ] = date_elements[ 0 ][ 1 ];
	date_string[ 8  ] = date_elements[ 0 ][ 2 ];
	date_string[ 9  ] = date_elements[ 0 ][ 3 ];
	date_string[ 10 ] = ' ';

	if( strlen( date_elements[ 3 ] ) == 1 )
	{
		date_string[ 11 ] = '0';
		date_string[ 12 ] = date_elements[ 3 ][ 0 ];
	}
	else
	{
		date_string[ 11 ] = date_elements[ 3 ][ 0 ];
		date_string[ 12 ] = date_elements[ 3 ][ 1 ];
	}
	date_string[ 13 ] = ':';

	if( strlen( date_elements[ 4 ] ) == 1 )
	{
		date_string[ 14 ] = '0';
		date_string[ 15 ] = date_elements[ 4 ][ 0 ];
	}
	else
	{
		date_string[ 14 ] = date_elements[ 4 ][ 0 ];
		date_string[ 15 ] = date_elements[ 4 ][ 1 ];
	}
	date_string[ 16 ] = ':';

	if( strlen( date_elements[ 5 ] ) == 1 )
	{
		date_string[ 17 ] = '0';
		date_string[ 18 ] = date_elements[ 5 ][ 0 ];
	}
	else
	{
		date_string[ 17 ] = date_elements[ 5 ][ 0 ];
		date_string[ 18 ] = date_elements[ 5 ][ 1 ];
	}
	date_string[ 19 ] = '\0';

	while( date_element_iterator < date_element_count )
	{
		free( date_elements[ date_element_iterator ] );

		date_element_iterator++;
	}
	return( date_string );
}

/* Generate date string within a header value
 */
char *libewf_generate_date_header_value( void )
{
	char *date_string        = libewf_header_value_string_alloc( 20 );
	time_t timestamp         = time( NULL );
	struct tm *time_elements = localtime( &timestamp );

	snprintf( date_string, 20, "%4d %d %d %d %d %d",
		( time_elements->tm_year + 1900 ), ( time_elements->tm_mon + 1 ), time_elements->tm_mday,
		time_elements->tm_hour, time_elements->tm_min, time_elements->tm_sec );

	return( date_string );
}

/* Convert date string within a header2 value
 */
char *libewf_convert_date_header2_value( char *header_value, uint8_t date_format )
{
	char *date_string;
	time_t timestamp;
	struct tm *time_elements;
	size_t result = 0;

	if( header_value == NULL )
	{
		return( NULL );
	}
	date_string   = libewf_header_value_string_alloc( 20 );
	timestamp     = atoll( header_value );
	time_elements = localtime( &timestamp );

	if( date_format == LIBEWF_DATEFORMAT_MONTHDAY )
	{
		result = strftime( date_string, 20, "%m/%d/%Y %H:%M:%S", time_elements );
	}
	else if( date_format == LIBEWF_DATEFORMAT_DAYMONTH )
	{
		result = strftime( date_string, 20, "%d/%m/%Y %H:%M:%S", time_elements );
	}
	if( result == 0 )
	{
		free( date_string );

		return( NULL );
	}
	return( date_string );
}

/* Generate date string within a header2 value
 */
char *libewf_generate_date_header2_value( void )
{
	char *date_string = libewf_header_value_string_alloc( 20 );
	time_t timestamp  = time( NULL );

	snprintf( date_string, 20, "%" PRIu32, (uint32_t) timestamp );

	return( date_string );
}

/* Set a header value
 */
char *libewf_header_values_set_value( char* header_value, char *value )
{
	uint32_t string_size;

	if( header_value != NULL )
	{
		free( header_value );
	}
	string_size = strlen( value );

	/* Don't bother with empty values
	 */
	if( string_size <= 0 )
	{
		return( NULL );
	}
	/* 1 additional byte required one for the end of string
	 */
	header_value = libewf_header_value_string_alloc( string_size + 1 );

	memcpy( (char *) header_value, (char *) value, string_size );

	header_value[ string_size ] = '\0';

	return( header_value );
}

/* Parse an EWF header for the values
 */
LIBEWF_HEADER_VALUES *libewf_header_values_parse_header( EWF_HEADER *header, uint8_t date_format )
{
	LIBEWF_HEADER_VALUES *header_values;
	char **lines;
	char **types;
	char **values;
	uint32_t line_count  = 0;
	uint32_t type_count  = 0;
	uint32_t value_count = 0;
	uint32_t iterator    = 0;

	if( header == NULL )
	{
		return( NULL );
	}
	header_values = libewf_header_values_alloc();
	lines         = libewf_split_string( (char *) header, '\n', &line_count );
	types         = libewf_split_string( lines[ 2 ], '\t', &type_count );
	values        = libewf_split_string( lines[ 3 ], '\t', &value_count );

	while( iterator < line_count )
	{
		free( lines[ iterator ] );

		iterator++;
	}
	free( lines );

	iterator = 0;

	while( iterator < type_count )
	{
		if( strncmp( types[ iterator ], "av", 2 ) == 0 )
		{
			header_values->acquiry_software_version = libewf_header_values_set_value( header_values->acquiry_software_version, values[ iterator ] );
		}
		else if( strncmp( types[ iterator ], "ov", 2 ) == 0 )
		{
			header_values->acquiry_operating_system = libewf_header_values_set_value( header_values->acquiry_operating_system, values[ iterator ] );
		}
		else if( strncmp( types[ iterator ], "dc", 2 ) == 0 )
		{
			header_values->unknown_dc = libewf_header_values_set_value( header_values->unknown_dc, values[ iterator ] );
		}
		else if( ( strncmp( types[ iterator ], "m", 1 ) == 0 ) || ( strncmp( types[ iterator ], "u", 1 ) == 0 ) )
		{
			char *date_string = NULL;

			/* If the date string contains spaces it's in the old header
			 * format otherwise is in new header2 format
			 */
			if( strchr( values[ iterator ] , ' ' ) != NULL )
			{
				date_string = libewf_convert_date_header_value( values[ iterator ], date_format );
			}
			else
			{
				date_string = libewf_convert_date_header2_value( values[ iterator ], date_format );
			}

			if( strncmp( types[ iterator ], "m", 1 ) == 0 )
			{
				header_values->acquiry_date = libewf_header_values_set_value( header_values->acquiry_date, date_string );
			}
			else if( strncmp( types[ iterator ], "u", 1 ) == 0 )
			{
				header_values->system_date = libewf_header_values_set_value( header_values->system_date, date_string );
			}
			free( date_string );
		}
		else if( strncmp( types[ iterator ], "p", 1 ) == 0 )
		{
			uint32_t hash_length = strlen( values[ iterator ] );

			if( hash_length <= 0 )
			{
			}
			else if( ( hash_length == 1 ) && ( values[ iterator ][ 0 ] == '0' ) )
			{
			}
			else
			{
				header_values->password = libewf_header_values_set_value( header_values->password, values[ iterator ] );
			}
		}
		else if( strncmp( types[ iterator ], "a", 1 ) == 0 )
		{
			header_values->description = libewf_header_values_set_value( header_values->description, values[ iterator ] );
		}
		else if( strncmp( types[ iterator ], "c", 1 ) == 0 )
		{
			header_values->case_number = libewf_header_values_set_value( header_values->case_number, values[ iterator ] );
		}
		else if( strncmp( types[ iterator ], "n", 1 ) == 0 )
		{
			header_values->evidence_number = libewf_header_values_set_value( header_values->evidence_number, values[ iterator ] );
		}
		else if( strncmp( types[ iterator ], "e", 1 ) == 0 )
		{
			header_values->examiner_name = libewf_header_values_set_value( header_values->examiner_name, values[ iterator ] );
		}
		else if( strncmp( types[ iterator ], "t", 1 ) == 0 )
		{
			header_values->notes = libewf_header_values_set_value( header_values->notes, values[ iterator ] );
		}
		else if( strncmp( types[ iterator ], "r", 1 ) == 0 )
		{
			header_values->compression_type = libewf_header_values_set_value( header_values->compression_type, values[ iterator ] );
		}
		else
		{
			LIBEWF_VERBOSE_PRINT( "libewf_header_values_parse_header: unsupported type: %d with value: %s\n", types[ iterator ], values[ iterator ] );
		}
		iterator++;
	}
	iterator = 0;

	while( iterator < type_count )
	{
		free( types[ iterator ] );

		iterator++;
	}
	free( types );

	iterator = 0;

	while( iterator < value_count )
	{
		free( values[ iterator ] );

		iterator++;
	}
	free( values );

	return( header_values );
}

/* Print the header values to a stream
 */
void libewf_header_values_fprint( FILE *stream, LIBEWF_HEADER_VALUES *header_values )
{
	if ( ( stream == NULL ) || ( header_values == NULL ) )
	{
		LIBEWF_FATAL_PRINT( "libewf_header_values_fprint: invalid stream or header values.\n" );
	}
	if( header_values->case_number != NULL )
	{
		fprintf( stream, "\tCase number:\t\t%s\n", header_values->case_number );
	}
	if( header_values->description != NULL )
	{
		fprintf( stream, "\tDescription:\t\t%s\n", header_values->description );
	}
	if( header_values->examiner_name != NULL )
	{
		fprintf( stream, "\tExaminer name:\t\t%s\n", header_values->examiner_name );
	}
	if( header_values->evidence_number != NULL )
	{
		fprintf( stream, "\tEvidence number:\t%s\n", header_values->evidence_number );
	}
	if( header_values->notes != NULL )
	{
		fprintf( stream, "\tNotes:\t\t\t%s\n", header_values->notes );
	}
	if( header_values->acquiry_date != NULL )
	{
		fprintf( stream, "\tAcquiry date:\t\t%s\n", header_values->acquiry_date );
	}
	if( header_values->system_date != NULL )
	{
		fprintf( stream, "\tSystem date:\t\t%s\n", header_values->system_date );
	}
	if( header_values->acquiry_operating_system != NULL )
	{
		fprintf( stream, "\tOperating system used:\t%s\n", header_values->acquiry_operating_system );
	}
	if( header_values->acquiry_software_version != NULL )
	{
		fprintf( stream, "\tSoftware used:\t\t%s\n", header_values->acquiry_software_version );
	}
	if( header_values->password != NULL )
	{
		fprintf( stream, "\tPassword:\t\t(hash: %s)\n", header_values->password );
	}
	else
	{
		fprintf( stream, "\tPassword:\t\tN/A\n" );
	}
	if( header_values->compression_type != NULL )
	{
		if( strncmp( header_values->compression_type, LIBEWF_COMPRESSIONTYPE_NONE, 1 ) == 0 )
		{
			fprintf( stream, "\tCompression type:\tno compression\n" );
		}
		else if( strncmp( header_values->compression_type, LIBEWF_COMPRESSIONTYPE_FAST, 1 ) == 0 )
		{
			fprintf( stream, "\tCompression type:\tgood (fast) compression\n" );
		}
		else if( strncmp( header_values->compression_type, LIBEWF_COMPRESSIONTYPE_BEST, 1 ) == 0 )
		{
			fprintf( stream, "\tCompression type:\tbest compression\n" );
		}
		else
		{
			fprintf( stream, "\tCompression type:\tunknown compression\n" );
		}
	}
	if( header_values->unknown_dc != NULL )
	{
		fprintf( stream, "\tUnknown value dc:\t%s\n", header_values->unknown_dc );
	}
}

/* Generate an Encase3 header
 */
EWF_HEADER *libewf_header_values_generate_header_string_encase3( LIBEWF_HEADER_VALUES *header_values, uint8_t compression_level )
{
	EWF_HEADER *header;
	char *header_string_head       = "1\r\nmain\r\nc\tn\ta\te\tt\tav\tov\tm\tu\tp\tr\r\n";
	char *header_string_tail       = "\r\n\r\n";
	char *case_number              = "";
	char *description              = "";
	char *examiner_name            = "";
	char *evidence_number          = "";
	char *notes                    = "";
	char *system_date              = "";
	char *acquiry_date             = "";
	char *acquiry_operating_system = "";
	char *acquiry_software_version = "";
	char *password_hash            = "";
	char *compression_type         = "";

	size_t size = strlen( header_string_head );

	if( header_values == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_header_values_generate_header_string_encase3: invalid header values.\n" );
	}
	if( header_values->case_number != NULL )
	{
		size       += strlen( header_values->case_number );
		case_number = header_values->case_number;
	}
	if( header_values->description != NULL )
	{
		size       += strlen( header_values->description );
		description = header_values->description;
	}
	if( header_values->examiner_name != NULL )
	{
		size         += strlen( header_values->examiner_name );
		examiner_name = header_values->examiner_name;
	}
	if( header_values->evidence_number != NULL )
	{
		size           += strlen( header_values->evidence_number );
		evidence_number = header_values->evidence_number;
	}
	if( header_values->notes != NULL )
	{
		size += strlen( header_values->notes );
		notes = header_values->notes;
	}
	if( header_values->acquiry_date != NULL )
	{
		size        += strlen( header_values->acquiry_date );
		acquiry_date = header_values->acquiry_date;
	}
	else
	{
		acquiry_date = libewf_generate_date_header_value();
		size        += strlen( acquiry_date );
	}
	if( header_values->system_date != NULL )
	{
		size       += strlen( header_values->system_date );
		system_date = header_values->system_date;
	}
	else
	{
		system_date = libewf_generate_date_header_value();
		size       += strlen( system_date );
	}
	if( header_values->acquiry_operating_system != NULL )
	{
		size                    += strlen( header_values->acquiry_operating_system );
		acquiry_operating_system = header_values->acquiry_operating_system;
	}
	if( header_values->acquiry_software_version != NULL )
	{
		size                    += strlen( header_values->acquiry_software_version );
		acquiry_software_version = header_values->acquiry_software_version;
	}
	if( header_values->password != NULL )
	{
		size         += strlen( header_values->password );
		password_hash = header_values->password;
	}
	else
	{
		size         += 1;
		password_hash = "0";
	}
	if( header_values->compression_type != NULL )
	{
		size            += strlen( header_values->compression_type );
		compression_type = header_values->compression_type;
	}
	else
	{
		if( compression_level == EWF_COMPRESSION_NONE )
		{
			compression_type = LIBEWF_COMPRESSIONTYPE_NONE;
		}
		else if( compression_level == EWF_COMPRESSION_FAST )
		{
			compression_type = LIBEWF_COMPRESSIONTYPE_FAST;
		}
		else if( compression_level == EWF_COMPRESSION_BEST )
		{
			compression_type = LIBEWF_COMPRESSIONTYPE_BEST;
		}
		else
		{
			LIBEWF_FATAL_PRINT( "libewf_header_values_generate_header_string_encase3: compression level not supported.\n" );
		}
		size += strlen( compression_type );
	}
	size += strlen( header_string_tail );

	/* allow for 10x \t and 1x \0
	 */
	size += 11;

	header = ewf_header_alloc( size );

	snprintf( (char *) header, size, "%s%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s%s",
		header_string_head, case_number, evidence_number, description, examiner_name,
		notes, acquiry_software_version, acquiry_operating_system, acquiry_date,
		system_date, password_hash, compression_type, header_string_tail );

	/* Make sure the string is terminated
	 */
	header[ size ] = '\0';

	if( header_values->acquiry_date != NULL )
	{
		free( acquiry_date );
	}
	if( header_values->system_date != NULL )
	{
		free( system_date );
	}
	return( header );
}

/* Generate an Encase4 header
 */
EWF_HEADER *libewf_header_values_generate_header_string_encase4( LIBEWF_HEADER_VALUES *header_values )
{
	EWF_HEADER *header;
	char *header_string_head       = "1\r\nmain\r\nc\tn\ta\te\tt\tav\tov\tm\tu\tp\r\n";
	char *header_string_tail       = "\r\n\r\n";
	char *case_number              = "";
	char *description              = "";
	char *examiner_name            = "";
	char *evidence_number          = "";
	char *notes                    = "";
	char *system_date              = "";
	char *acquiry_date             = "";
	char *acquiry_operating_system = "";
	char *acquiry_software_version = "";
	char *password_hash            = "";

	size_t size = strlen( header_string_head );

	if( header_values == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_header_values_generate_header_string_encase4: invalid header values.\n" );
	}
	if( header_values->case_number != NULL )
	{
		size       += strlen( header_values->case_number );
		case_number = header_values->case_number;
	}
	if( header_values->description != NULL )
	{
		size       += strlen( header_values->description );
		description = header_values->description;
	}
	if( header_values->examiner_name != NULL )
	{
		size         += strlen( header_values->examiner_name );
		examiner_name = header_values->examiner_name;
	}
	if( header_values->evidence_number != NULL )
	{
		size           += strlen( header_values->evidence_number );
		evidence_number = header_values->evidence_number;
	}
	if( header_values->notes != NULL )
	{
		size += strlen( header_values->notes );
		notes = header_values->notes;
	}
	if( header_values->acquiry_date != NULL )
	{
		size        += strlen( header_values->acquiry_date );
		acquiry_date = header_values->acquiry_date;
	}
	else
	{
		acquiry_date = libewf_generate_date_header2_value();
		size        += strlen( acquiry_date );
	}
	if( header_values->system_date != NULL )
	{
		size       += strlen( header_values->system_date );
		system_date = header_values->system_date;
	}
	else
	{
		system_date = libewf_generate_date_header2_value();
		size       += strlen( system_date );
	}
	if( header_values->acquiry_operating_system != NULL )
	{
		size                    += strlen( header_values->acquiry_operating_system );
		acquiry_operating_system = header_values->acquiry_operating_system;
	}
	if( header_values->acquiry_software_version != NULL )
	{
		size                    += strlen( header_values->acquiry_software_version );
		acquiry_software_version = header_values->acquiry_software_version;
	}
	if( header_values->password != NULL )
	{
		size         += strlen( header_values->password );
		password_hash = header_values->password;
	}
	size += strlen( header_string_tail );

	/* allow for 9x \t and 1x \0
	 */
	size += 10;

	header = ewf_header_alloc( size );

	snprintf( (char *) header, size, "%s%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s%s",
		header_string_head, description, case_number, evidence_number, examiner_name,
		notes, acquiry_software_version, acquiry_operating_system, acquiry_date,
		system_date, password_hash, header_string_tail );

	/* Make sure the string is terminated
	 */
	header[ size ] = '\0';

	if( header_values->acquiry_date != NULL )
	{
		free( acquiry_date );
	}
	if( header_values->system_date != NULL )
	{
		free( system_date );
	}
	return( header );
}

/* Generate an Encase5 header2
 */
EWF_HEADER *libewf_header_values_generate_header2_string_encase5( LIBEWF_HEADER_VALUES *header_values )
{
	EWF_HEADER *header;
	char *header_string_main       = "3\r\nmain\r\na\tc\tn\te\tt\tav\tov\tm\tu\tp\tdc\r\n";
	char *header_string_tail       = "\r\n\r\n";
	char *header_string_srce       = "srce\r\n0\t1\r\np\tn\tid\tev\ttb\tlo\tpo\tah\tgu\taq\r\n0\t0\t\t\t\t\t\t\t\t\r\n\t\t\t\t\t-1\t-1\t\t\t\r\n\r\n";
	char *header_string_sub        = "sub\r\n0\t1\r\np\tn\tid\tnu\tco\tgu\r\n0\t0\t\t\t\t\r\n\t\t\t\t1\t\r\n\r\n";
	char *case_number              = "";
	char *description              = "";
	char *examiner_name            = "";
	char *evidence_number          = "";
	char *notes                    = "";
	char *system_date              = "";
	char *acquiry_date             = "";
	char *acquiry_operating_system = "";
	char *acquiry_software_version = "";
	char *password_hash            = "";
	char *unknown_dc               = "";

	size_t size  = strlen( header_string_main );
	size        += strlen( header_string_srce );
	size        += strlen( header_string_sub );

	if( header_values == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_header_values_generate_header_string_encase5: invalid header values.\n" );
	}
	if( header_values->case_number != NULL )
	{
		size       += strlen( header_values->case_number );
		case_number = header_values->case_number;
	}
	if( header_values->description != NULL )
	{
		size       += strlen( header_values->description );
		description = header_values->description;
	}
	if( header_values->examiner_name != NULL )
	{
		size         += strlen( header_values->examiner_name );
		examiner_name = header_values->examiner_name;
	}
	if( header_values->evidence_number != NULL )
	{
		size           += strlen( header_values->evidence_number );
		evidence_number = header_values->evidence_number;
	}
	if( header_values->notes != NULL )
	{
		size += strlen( header_values->notes );
		notes = header_values->notes;
	}
	if( header_values->acquiry_date != NULL )
	{
		size        += strlen( header_values->acquiry_date );
		acquiry_date = header_values->acquiry_date;
	}
	else
	{
		acquiry_date = libewf_generate_date_header2_value();
		size        += strlen( acquiry_date );
	}
	if( header_values->system_date != NULL )
	{
		size       += strlen( header_values->system_date );
		system_date = header_values->system_date;
	}
	else
	{
		system_date = libewf_generate_date_header2_value();
		size       += strlen( system_date );
	}
	if( header_values->acquiry_operating_system != NULL )
	{
		size                    += strlen( header_values->acquiry_operating_system );
		acquiry_operating_system = header_values->acquiry_operating_system;
	}
	if( header_values->acquiry_software_version != NULL )
	{
		size                    += strlen( header_values->acquiry_software_version );
		acquiry_software_version = header_values->acquiry_software_version;
	}
	if( header_values->password != NULL )
	{
		size         += strlen( header_values->password );
		password_hash = header_values->password;
	}
	if( header_values->unknown_dc != NULL )
	{
		size      += strlen( header_values->unknown_dc );
		unknown_dc = header_values->unknown_dc;
	}
	size += strlen( header_string_tail );

	/* allow for 10x \t and 1x \0
	 */
	size += 11;

	header = ewf_header_alloc( size );

	snprintf( (char *) header, size, "%s%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s%s%s%s",
		header_string_main, case_number, evidence_number, description, examiner_name,
		notes, acquiry_software_version, acquiry_operating_system, acquiry_date,
		system_date, password_hash, unknown_dc, header_string_tail,
		header_string_srce, header_string_sub );

	/* Make sure the string is terminated
	 */
	header[ size ] = '\0';

	if( header_values->acquiry_date != NULL )
	{
		free( acquiry_date );
	}
	if( header_values->system_date != NULL )
	{
		free( system_date );
	}
	return( header );
}

/* Generate an FTK Imager header
 */
EWF_HEADER *libewf_header_values_generate_header_string_ftk( LIBEWF_HEADER_VALUES *header_values, uint8_t compression_level )
{
	EWF_HEADER *header;
	char *header_string_head       = "1\nmain\nc\tn\ta\te\tt\tav\tov\tm\tu\tp\tr\n";
	char *header_string_tail       = "\n\n";
	char *case_number              = "";
	char *description              = "";
	char *examiner_name            = "";
	char *evidence_number          = "";
	char *notes                    = "";
	char *system_date              = "";
	char *acquiry_date             = "";
	char *acquiry_operating_system = "";
	char *acquiry_software_version = "";
	char *password_hash            = "";
	char *compression_type         = "";

	size_t size = strlen( header_string_head );

	if( header_values == NULL )
	{
		LIBEWF_FATAL_PRINT( "libewf_header_values_generate_header_string_ftk: invalid header values.\n" );
	}
	if( header_values->case_number != NULL )
	{
		size       += strlen( header_values->case_number );
		case_number = header_values->case_number;
	}
	if( header_values->description != NULL )
	{
		size       += strlen( header_values->description );
		description = header_values->description;
	}
	if( header_values->examiner_name != NULL )
	{
		size         += strlen( header_values->examiner_name );
		examiner_name = header_values->examiner_name;
	}
	if( header_values->evidence_number != NULL )
	{
		size           += strlen( header_values->evidence_number );
		evidence_number = header_values->evidence_number;
	}
	if( header_values->notes != NULL )
	{
		size += strlen( header_values->notes );
		notes = header_values->notes;
	}
	if( header_values->acquiry_date != NULL )
	{
		size        += strlen( header_values->acquiry_date );
		acquiry_date = header_values->acquiry_date;
	}
	else
	{
		acquiry_date = libewf_generate_date_header_value();
		size        += strlen( acquiry_date );
	}
	if( header_values->system_date != NULL )
	{
		size       += strlen( header_values->system_date );
		system_date = header_values->system_date;
	}
	else
	{
		system_date = libewf_generate_date_header_value();
		size       += strlen( system_date );
	}
	if( header_values->acquiry_operating_system != NULL )
	{
		size                    += strlen( header_values->acquiry_operating_system );
		acquiry_operating_system = header_values->acquiry_operating_system;
	}
	if( header_values->acquiry_software_version != NULL )
	{
		size                    += strlen( header_values->acquiry_software_version );
		acquiry_software_version = header_values->acquiry_software_version;
	}
	if( header_values->password != NULL )
	{
		size         += strlen( header_values->password );
		password_hash = header_values->password;
	}
	else
	{
		size         += 1;
		password_hash = "0";
	}
	if( header_values->compression_type != NULL )
	{
		size            += strlen( header_values->compression_type );
		compression_type = header_values->compression_type;
	}
	else
	{
		if( compression_level == EWF_COMPRESSION_NONE )
		{
			compression_type = LIBEWF_COMPRESSIONTYPE_NONE;
		}
		else if( compression_level == EWF_COMPRESSION_FAST )
		{
			compression_type = LIBEWF_COMPRESSIONTYPE_FAST;
		}
		else if( compression_level == EWF_COMPRESSION_BEST )
		{
			compression_type = LIBEWF_COMPRESSIONTYPE_BEST;
		}
		else
		{
			LIBEWF_FATAL_PRINT( "libewf_header_values_generate_header_string_ftk: compression level not supported.\n" );
		}
		size += strlen( compression_type );
	}
	size += strlen( header_string_tail );

	/* allow for 10x \t and 1x \0
	 */
	size += 11;

	header = ewf_header_alloc( size );

	snprintf( (char *) header, size, "%s%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s%s",
		header_string_head, case_number, evidence_number, description, examiner_name,
		notes, acquiry_software_version, acquiry_operating_system, acquiry_date,
		system_date, password_hash, compression_type, header_string_tail );

	/* Make sure the string is terminated
	 */
	header[ size ] = '\0';

	if( header_values->acquiry_date != NULL )
	{
		free( acquiry_date );
	}
	if( header_values->system_date != NULL )
	{
		free( system_date );
	}
	return( header );
}

