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

/** \file
 * Handling for collections of lines of text.
 *
 * The #confline structure provides for storage of a list of lines of text.
 */

/** The maximum length of the text content of a #confline node. */
#define MAX_CONFLINE 1024

/** A linked list representing the lines of text from a configuration file. */
struct confline
{
	struct confline *next; /**< The next entry in the list, or NULL for the end of the list. */
	char *line; /**< The NUL-terminated line of text. */
};

/** Create a #confline representing the given line of text.
 * @param [in] line The line of text.
 * @return A new #confline containing a copy of the given text.
 */
struct confline *create_confline(const char *line);

/** Destroy a #confline and its copy of the contained text.
 * @param [in] cl The #confline to free.
 */
void destroy_confline(struct confline *cl);

/** Destroy a list of #confline nodes.
 * #destroy_confline will be called on each node in the list.
 * @param [in] cl_list The head of the list of #confline entries to free.
 */
void destroy_confline_list(struct confline *cl_list);

/** Read the contents of a stream into a #confline list.
 * @param [in] fptr The stream to read.
 * @return The head of the #confline list representing all of the lines of
 * 	the file.
 */
struct confline *read_conflines(FILE *fptr);

/** Append a #confline to an existing #confline list.
 * @param [in] cl_list The existing list to which the given line is to be
 * 	appended.  May be NULL if creating a new list.
 * @param [in] cl The #confline to append.
 * @return The new head of the #confline list.  This will be the same as
 * 	cl_list unless cl_list is NULL, in which case cl becomes the head
 * 	of a new list.
 */
struct confline *append_confline(struct confline *cl_list, struct confline *cl);

