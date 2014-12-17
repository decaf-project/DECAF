/* Copyright (C) 2009 The Android Open Source Project
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
#include "android/utils/assert.h"
#include "android/utils/panic.h"
#include <stdio.h>

typedef struct {
    const char*  file;
    long         lineno;
    const char*  function;
} AssertLoc;

AssertLoc*
_get_assert_loc(void)
{
    /* XXX: Use thread-local storage instead ? */
    static AssertLoc  loc[1];
    return loc;
}

void
_android_assert_loc( const char*  fileName,
                     long         fileLineno,
                     const char*  functionName )
{
    AssertLoc*  loc = _get_assert_loc();

    loc->file     = fileName;
    loc->lineno   = fileLineno;
    loc->function = functionName;
}

static void
_android_assert_log_default( const char*  fmt, va_list  args )
{
    vfprintf(stderr, fmt, args);
}

static AAssertLogFunc  _assert_log = _android_assert_log_default;

void  android_assert_fail(const char*  messageFmt, ...)
{
    AssertLoc*  loc = _get_assert_loc();
    va_list  args;

    va_start(args, messageFmt);
    _assert_log(messageFmt, args);
    va_end(args);

    android_panic("ASSERTION FAILURE (%s:%d) in %s\n", loc->file, loc->lineno, loc->function);
}

void  android_assert_registerLog( AAssertLogFunc  logger )
{
    if (logger == NULL)
        android_panic("Passing NULL to %s\n", __FUNCTION__);

    _assert_log = logger;
}
