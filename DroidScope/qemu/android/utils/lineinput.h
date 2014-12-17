/* Copyright (C) 2011 The Android Open Source Project
**
** This software is licensed under the terms of the GNU General Public
** License version 2, as published by the Free Software Foundation, and
** may be copied, distributed, and modified under those terms.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
*/
#ifndef _ANDROID_UTILS_LINEINPUT_H
#define _ANDROID_UTILS_LINEINPUT_H

#include <stdio.h>

/* A LineInput is used to read input text, one line at a time,
 * into a temporary buffer owner by the LineInput object.
 */
typedef struct LineInput LineInput;

/* Create a LineInput object that reads from a FILE* object */
LineInput*  lineInput_newFromStdFile( FILE* file );

/* Read next line from input. The result is zero-terminated with
 * all newlines removed (\n, \r or \r\n) automatically.
 *
 * Returns NULL in case of error, or when the end of file is reached.
 * See lineInput_isEof() and lineInput_getError()
 *
 * The returned string is owned by the LineInput object and its
 * value will not persist any other call to any LineInput functions.
 */
const char* lineInput_getLine( LineInput* input );

/* Same as lineInput_getLine(), but also returns the line size into
 * '*pSize' to save you a strlen() call.
 */
const char* lineInput_getLineAndSize( LineInput* input, size_t *pSize );

/* Returns the number of the last line read by lineInput_getLine */
int lineInput_getLineNumber( LineInput* input );

/* Returns TRUE iff the end of file was reached */
int lineInput_isEof( LineInput* input );

/* Return the error condition of a LineInput object.
 * These are standard errno code for the last operation.
 * Note: EOF corresponds to 0 here.
 */
int lineInput_getError( LineInput* input );

/* Free a LineInput object. */
void lineInput_free( LineInput* input );

#endif /* _ANDROID_UTILS_LINEINPUT_H */
