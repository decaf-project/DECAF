/* Copyright (C) 2007-2008 The Android Open Source Project
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
#ifndef _ANDROID_UTILS_TIMEZONE_H
#define _ANDROID_UTILS_TIMEZONE_H

/* try to set the default host timezone, returns 0 on success, or -1 if
 * 'tzname' is not in zoneinfo format (e.g. Area/Location)
 */
extern int  timezone_set( const char*  tzname );

/* append the current host "zoneinfo" timezone name to a given buffer. note
 * that this is something like "America/Los_Angeles", and not the human-friendly "PST"
 * this is required by the Android emulated system...
 *
 * the implementation of this function is really tricky and is OS-dependent
 * on Unix systems, it needs to cater to the TZ environment variable, uhhh
 *
 * if TZ is defined to something like "CET" or "PST", this will return the name "Unknown/Unknown"
 */
extern char*  bufprint_zoneinfo_timezone( char*  buffer, char*  end );

#endif /* _ANDROID_UTILS_TIMEZONE_H */
