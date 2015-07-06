/* -*- linux-c -*- */
/*
    This file is part of llconf2

    Copyright (C) 2007  Darius Davis <darius.davis@avocent.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

void parse_error_at(const char const *buffer, int index, const char *warning_text, const char *text_at_position)
{
	int i, hpos = 0;
	int bufsize;
	char *_buffer;
	fprintf(stderr, "Warning: %s\n", warning_text);
	
	/* Count *back* over whitespace... */
	while (index && isspace(buffer[index - 1]))
		index--;
	
	/* How much do we want to print out? */
	bufsize = index;
	while(buffer[bufsize] && (buffer[bufsize] != '\n'))
		bufsize++;

	_buffer = strndup(buffer, bufsize);
		
	/* Find the horizontal position for the marker -- approximate, but pretty good. */
	for(i = 0; i < index; ++i)
		switch(buffer[i]){
		case '\n':
			hpos = 0;
			break;
		case '\t':
			hpos = (hpos + 8) % 8;
			break;
		default:
			hpos++;
		}

	fprintf(stderr, "%s\n"
		"%*s^-- %s\n",
		_buffer,
		hpos, "", text_at_position);
	
	free(_buffer);
}

void parse_error_at_expected(const char const *buffer, int index, const char *expected, const char *assumed)
{
	char *warning_text, *text_at_position;
	if(assumed)
		asprintf(&warning_text, "Assumed '%s' at marked location:", assumed);
	else
		warning_text = "Warning: Parser error at marked location:";
	
	asprintf(&text_at_position, "expected %s", expected);
	
	parse_error_at(buffer, index, warning_text, text_at_position);

	if(assumed) free(warning_text);
	free(text_at_position);
}
