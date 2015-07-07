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

#define cp_buf_until(dest, p, expr) \
  { \
    char *q = buf; \
    while(*p && (q < dest+sizeof(dest)-1) && (expr)) *q++ = *p++;	\
    *q = 0; \
  }

char *dup_next_word(const char **pp);
char *dup_next_word_b(const char **pp, char *buf, int n);
char *dup_next_quoted(const char **pp, char qchar);
char *dup_next_quoted_b(const char **pp, char *buf, int n, char qchar);
char *dup_next_line(const char **pp);
char *dup_next_line_b(const char **pp, char *buf, int n);
char *dup_line_until(const char **pp, char until);
char *dup_line_until_b(const char **pp, char until, char *buf, int n);

char *dup_quote_string(const char *string, char qchar);
char *dup_unquote_string(const char *qstring, char qchar);
char *dup_unquote_string_ifquoted(const char *qstring, char qchar);

/* these functions copy from *pp to *pq, but do not null terminate,
   and advance the pointers */
void cp_spaces(const char **pp, char **pq, int n);
void cp_word(const char **pp, char **pq, int n);
void cp_quoted(const char **pp, char **pq, int n);
void cp_quoted_ifquoted(const char **pp, char **pq, int n, char qchar);

/* these functions advance the pointer *pp */
void skip_spaces(const char **pp);
void skip_word(const char **pp);
void skip_quoted(const char **pp);
void skip_quoted_ifquoted(const char **pp, char qchar);

char *strjoin(const char *s1, const char *s2);

/* FIXME(if bored): use cp_ and skip_ functions in the dup_ functions to reduce code... */
