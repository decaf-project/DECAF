/* -*- linux-c -*- */
/*
    This file is part of llconf2

    Copyright (C) 2004-2007  Oliver Kurth <oku@debian.org>

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

#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "strutils.h"

char *dup_next_word(const char **pp)
{
	const char *p = *pp;
	char *q, tmpbuf[1024];
  
	q = tmpbuf;
	while(*p && isspace(*p)) p++; /* eat spaces */
	while(*p && !isspace(*p) && q < tmpbuf + sizeof(tmpbuf)-1) *q++ = *p++;
	*q = 0;
	*pp = p;
  
	return strdup(tmpbuf);
}

char *dup_next_word_b(const char **pp, char *buf, int n)
{
	const char *p = *pp;
	char *q;
  
	q = buf;
	while(*p && isspace(*p)) p++; /* eat spaces */
	while(*p && !isspace(*p) && q < buf+n) *q++ = *p++;
	*q = 0;
	*pp = p;
  
	return buf;
}

char *dup_next_quoted(const char **pp, char qchar)
{
	const char *p = *pp;
	char *q, tmpbuf[1024];
  
	q = tmpbuf;
	while(*p && isspace(*p)) p++; /* eat spaces */
	if(*p != qchar)
		return NULL;
	p++;
	while(*p && (*p != qchar || p[-1] == '\\') && q < tmpbuf + sizeof(tmpbuf)-1) *q++ = *p++;
	p++;
	*q = 0;
	*pp = p;
  
	return strdup(tmpbuf);
}

char *dup_next_quoted_b(const char **pp, char *buf, int n, char qchar)
{
	const char *p = *pp;
	char *q;
  
	q = buf;
	while(*p && isspace(*p)) p++; /* eat spaces */
	if(*p != qchar)
		return NULL;
	p++;
	while(*p && (*p != qchar || p[-1] == '\\') && q < buf+n) *q++ = *p++;
	p++;
	*q = 0;
	*pp = p;
  
	return buf;
}

char *dup_next_line(const char **pp)
{
	const char *p = *pp;
	char *q, tmpbuf[1024];
  
	q = tmpbuf;
	while(*p && (*p != '\n') && q < tmpbuf + sizeof(tmpbuf)-1) *q++ = *p++;
	*q = 0;
	*pp = p;
  
	return strdup(tmpbuf);
}

char *dup_next_line_b(const char **pp, char *buf, int n)
{
	const char *p = *pp;
	char *q;
  
	q = buf;
	while(*p && (*p != '\n') && q < buf+n) *q++ = *p++;
	*q = 0;
	*pp = p;
  
	return buf;
}

char *dup_line_until(const char **pp, char until)
{
	const char *p = *pp;
	char *q, tmpbuf[1024];
  
	q = tmpbuf;
	while(*p && (*p != '\n') && (*p != until) && q < tmpbuf + sizeof(tmpbuf)-1) *q++ = *p++;
	*q = 0;
	*pp = p;
  
	return strdup(tmpbuf);
}

char *dup_line_until_b(const char **pp, char until, char *buf, int n)
{
	const char *p = *pp;
	char *q;
  
	q = buf;
	while(*p && (*p != '\n') && (*p != until) && q < buf+n) *q++ = *p++;
	*q = 0;
	*pp = p;
  
	return buf;
}

/* quote a string witch qchar, and escape any ocurence of qchar with a backslash */
char *dup_quote_string(const char *string, char qchar)
{
	const char *p;
	char *q, *qstring;
	int n;

	/* calculate space needed:
	   strlen(string) + 2 quotes + number of quotes + null char */
	p = string;
	n = 3;
	while(*p){
		if(*p == qchar) n++;
		n++;
		p++;
	}

	qstring = (char *)malloc(n);
	q = qstring;
	p = string;

	*q++ = qchar;
	while(*p){
		if(*p == qchar)
			*q++ = '\\';
		*q++ = *p++;
	}
	*q++ = qchar;
	*q = 0;

	return qstring;
}

/* do the reverse of dup_quote_string: */
char *dup_unquote_string(const char *qstring, char qchar)
{
	char *string, *q;
	const char *p;

	string = (char *)malloc(strlen(qstring)+1);

	p = qstring;
	if(*p == qchar) p++;
	q = string;
	while(*p){
		if(!(*p == '\\' && *(p+1) == qchar))
			*q++ = *p++;
		else
			p++;
	}
	if((q > string) && (q[-1] == qchar)) q--;
	*q = 0;

	return string;
}

/* unquote only if really quoted */
char *dup_unquote_string_ifquoted(const char *qstring, char qchar)
{
	if(qstring[0] == qchar)
		return dup_unquote_string(qstring, qchar);
	else
		return strdup(qstring);
}

void cp_spaces(const char **pp, char **pq, int n)
{
	const char *p = *pp;
	char *q = *pq, *q0 = *pq;

	while(*p && isspace(*p) && q < q0 + n)
		*q++ = *p++;

	*pp = p;
	*pq = q;
}

void cp_word(const char **pp, char **pq, int n)
{
	const char *p = *pp;
	char *q = *pq, *q0 = *pq;

	while(*p && !isspace(*p) && q < q0 + n)
		*q++ = *p++;

	*pp = p;
	*pq = q;
}

void cp_quoted(const char **pp, char **pq, int n)
{
	const char *p = *pp;
	char *q = *pq, *q0 = *pq, qchar;

	qchar = *p;
	*q++ = *p++;

	while(*p && (*p != qchar || p[-1] == '\\') && q < q0 + n)
		*q++ = *p++;

	if(*p == qchar)
		*q++ = *p++;

	*pp = p;
	*pq = q;
}
	
void cp_quoted_ifquoted(const char **pp, char **pq, int n, char qchar)
{
	const char *p = *pp;

	if(*p == qchar)
		cp_quoted(pp, pq, n);
	else
		cp_word(pp, pq, n);
}


void skip_spaces(const char **pp)
{
	const char *p = *pp;

	while(*p && isspace(*p))
		p++;

	*pp = p;
}

void skip_word(const char **pp)
{
	const char *p = *pp;

	while(*p && !isspace(*p))
		p++;

	*pp = p;
}

void skip_quoted(const char **pp)
{
	const char *p = *pp;
	char qchar;

	qchar = *p;
	p++;

	while(*p && (*p != qchar || p[-1] == '\\'))
		p++;

	if(*p == qchar)
		p++;

	*pp = p;
}
	
void skip_quoted_ifquoted(const char **pp, char qchar)
{
	const char *p = *pp;

	if(*p == qchar)
		skip_quoted(pp);
	else
		skip_word(pp);
}

/* Join str1 and str2 and return the new string. Allocates memory for the result string. */

char *strjoin(const char *str1, const char *str2)
{
	if(!str1 && !str2) return NULL;
	if(!str2) return strdup(str1);
	if(!str1) return strdup(str2);

	char *s = malloc(strlen(str1)+strlen(str2)+1);
	strcpy(s, str1);
	strcat(s, str2);
	return s;
}
