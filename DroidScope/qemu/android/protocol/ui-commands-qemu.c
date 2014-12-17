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
 * Contains implementation of the API for calling into the UI with the Core
 * control commands for standalone (monolithic) emulator.
 */

#include "android/android.h"
#include "android/hw-control.h"
#include "android/protocol/ui-commands-api.h"

/* Implemented in android/qemulator.c */
extern void android_emulator_set_window_scale(double scale, int is_dpi);

int
uicmd_set_window_scale(double scale, int is_dpi)
{
    android_emulator_set_window_scale(scale, is_dpi);
    return 0;
}

int
uicmd_set_brightness_change_callback(AndroidHwLightBrightnessCallback callback,
                                     void* opaque)
{
    AndroidHwControlFuncs  funcs;
    funcs.light_brightness = callback;
    android_hw_control_set(opaque, &funcs);
    return 0;
}
