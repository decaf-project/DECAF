/* Copyright (C) 2010 The Android Open Source Project
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

/*
 * Contains extension to android display (see android/display.h|c) that is used
 * by the core to communicate display changes to the attached UI
 */

#ifndef _ANDROID_DISPLAY_CORE_H
#define _ANDROID_DISPLAY_CORE_H

#include "console.h"

/*
 * Initializes one and only one instance of a core display.
 * Only used to register a dummy display change listener that
 * will trigger a timer.
 *  ds - Display state to use for the core display.
 */
extern void coredisplay_init(DisplayState* ds);

#endif /* _ANDROID_DISPLAY_CORE_H */
