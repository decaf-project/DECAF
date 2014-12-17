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
#include <errno.h>
#include "android/utils/system.h"
#include "android/utils/assert.h"
#include "android/utils/lineinput.h"

struct LineInput {
    char*   line;
    size_t  line_size;
    int     line_num;
    int     error;
    int     eof;

    struct {
        FILE*  file;
    } std;

    char    line0[128];
};

/* Error codes returned by the internal line reading function(s) */
enum {
    LINEINPUT_ERROR = -1,
    LINEINPUT_EOF = -2,
};


static LineInput*
_lineInput_new( void )
{
    LineInput*  input;

    ANEW0(input);
    input->line      = input->line0;
    input->line_size = sizeof(input->line0);

    return input;
}

/* Create a LineInput object that reads from a FILE* object */
LineInput*
lineInput_newFromStdFile( FILE* file )
{
    LineInput* input = _lineInput_new();

    input->std.file = file;
    return input;
}

/* Grow the line buffer a bit */
static void
_lineInput_grow( LineInput* input )
{
    char*  line;

    input->line_size += input->line_size >> 1;
    line = input->line;
    if (line == input->line0)
        line = NULL;

    AARRAY_RENEW(line, input->line_size);
    input->line = line;
}

/* Forward declaration */
static int _lineInput_getLineFromStdFile( LineInput* input, FILE* file );

const char*
lineInput_getLine( LineInput* input )
{
    return lineInput_getLineAndSize(input, NULL);
}

const char*
lineInput_getLineAndSize( LineInput* input, size_t *pSize )
{
    int ret;

    /* be safe */
    if (pSize)
        *pSize = 0;

    /* check parameters */
    if (input == NULL) {
        errno = EINVAL;
        return NULL;
    }

    /* check state */
    if (input->error) {
        return NULL;
    }
    if (input->eof) {
        return NULL;
    }

    ret = _lineInput_getLineFromStdFile(input, input->std.file);
    if (ret >= 0) {
        input->line_num += 1;
        if (pSize != NULL) {
            *pSize = ret;
            return input->line;
        }
        return input->line;
    }
    if (ret == LINEINPUT_EOF) {
        input->line_num += 1;
        input->eof = 1;
        return NULL;
    }
    if (ret == LINEINPUT_ERROR) {
        input->error = errno;
        return NULL;
    }
    AASSERT_UNREACHED();
    return NULL;
}

/* Returns the number of the last line read by lineInput_getLine */
int
lineInput_getLineNumber( LineInput* input )
{
    return input->line_num;
}

/* Returns TRUE iff the end of file was reached */
int
lineInput_isEof( LineInput* input )
{
    return (input->eof != 0);
}

/* Return the error condition of a LineInput object.
 * These are standard errno code for the last operation.
 * Note: EOF corresponds to 0 here.
 */
int
lineInput_getError( LineInput* input )
{
    return input->error;
}

void
lineInput_free( LineInput* input )
{
    if (input != NULL) {
        if (input->line != NULL) {
            if (input->line != input->line0)
                AFREE(input->line);
            input->line = NULL;
            input->line_size = 0;
        }
        AFREE(input);
    }
}


/* Internal function used to read a new line from a FILE* using fgets().
 * We assume that this is more efficient than calling fgetc() in a loop.
 *
 * Return length of line, or either LINEINPUT_EOF / LINEINPUT_ERROR
 */
static int
_lineInput_getLineFromStdFile( LineInput* input, FILE* file )
{
    int   offset = 0;
    char* p;

    input->line[0] = '\0';

    for (;;) {
        char* buffer = input->line + offset;
        int   avail  = input->line_size - offset;

        if (!fgets(buffer, avail, file)) {
            /* We either reached the end of file or an i/o error occured.
             * If we already read line data, just return it this time.
             */
            if (offset > 0) {
                return offset;
            }
            goto INPUT_ERROR;
        }

        /* Find the terminating zero */
        p = memchr(buffer, '\0', avail);
        AASSERT(p != NULL);

        if (p == buffer) {
            /* This happens when the file has an embedded '\0', treat it
             * as an eof, or bad things usually happen after that. */
            input->eof = 1;
            if (offset > 0)
                return offset;
            else
                return LINEINPUT_EOF;
        }

        if (p[-1] != '\n' && p[-1] != '\r') {
            /* This happens when the line is longer than our current buffer,
            * so grow its size and try again. */
            offset = p - input->line;
            _lineInput_grow(input);
            continue;
        }

        break;
    }

    /* Get rid of trailing newline(s). Consider: \n, \r, and \r\n */
    if (p[-1] == '\n') {
        p -= 1;
        if (p > input->line && p[-1] == '\r') {
            p -= 1;
        }
        p[0] = '\0';
    }
    else if (p[-1] == '\r') {
        p -= 1;
        p[0] = '\0';
    }

    /* We did it */
    return (p - input->line);

INPUT_ERROR:
    if (feof(file)) {
        input->eof = 1;
        return LINEINPUT_EOF;
    }
    input->error = errno;
    return LINEINPUT_ERROR;
}

