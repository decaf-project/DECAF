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

#ifndef _ANDROID_PROTOCOL_UI_COMMANDS_API_H
#define _ANDROID_PROTOCOL_UI_COMMANDS_API_H

/*
 * Contains the API for calling into the UI with the Core control commands.
 */

/* Changes the scale of the emulator window at runtime.
 * Param:
 *  scale, is_dpi - New window scale parameters
 * Return:
 *  0 on success, or < 0 on failure.
 */
extern int uicmd_set_window_scale(double scale, int is_dpi);

/* This is temporary redeclaration for AndroidHwLightBrightnessFunc declared
 * in android/hw-control.h We redeclare it here in order to keep type
 * consistency between android_core_set_brightness_change_callback and
 * light_brightness field of AndroidHwControlFuncs structure.
 */
typedef void  (*AndroidHwLightBrightnessCallback)(void* opaque,
                                                  const char* light,
                                                  int  brightness);

/* Registers a UI callback to be called when brightness is changed by the core. */
extern int uicmd_set_brightness_change_callback(AndroidHwLightBrightnessCallback callback,
                                                void* opaque);

#endif /* _ANDROID_PROTOCOL_UI_COMMANDS_API_H */
