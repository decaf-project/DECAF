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

#ifndef _ANDROID_PROTOCOL_CORE_COMMANDS_API_H
#define _ANDROID_PROTOCOL_CORE_COMMANDS_API_H

/*
 * Contains the API for calling into the Core with UI control commands.
 */

#include "android/android.h"
#include "android/hw-sensors.h"

/* Instructs the Core to change the coarse orientation.
 * Return:
 *  0 on success, or < 0 on failure.
 */
extern int corecmd_set_coarse_orientation(AndroidCoarseOrientation orient);

/* Toggles the network in the Core.
 * Return:
 *  0 on success, or < 0 on failure.
 */
extern int corecmd_toggle_network();

/* Starts or stops tracing in the Core.
 * Param:
 *  start - Starts (> 0), or stops (== 0) tracing.
 * Return:
 *  0 on success, or < 0 on failure.
 */
extern int corecmd_trace_control(int start);

/* Checks if network is disabled in the Core.
 * Return:
 *  0 if network is enabled, 1 if it is disabled, or < 0 on failure.
 */
extern int corecmd_is_network_disabled();

/* Requests a NetworkSpeed instance from the Core.
 * Param:
 *  index - Index of an entry in the NetworkSpeed array.
 *  netspeed - Upon success contains allocated and initialized NetworkSpeed
 *      instance for the given index. Note that strings addressed by "name" and
 *      "display" fileds in the returned NetworkSpeed instance are containd
 *      inside the buffer allocated for the returned NetworkSpeed instance.
 *      Caller of this routine must eventually free the buffer returned in this
 *      parameter.
 * Return:
 *  0 on success, or < 0 on failure.
 */
extern int corecmd_get_netspeed(int index, NetworkSpeed** netspeed);

/* Requests a NetworkLatency instance from the Core.
 * Param:
 *  index - Index of an entry in the NetworkLatency array.
 *  netdelay - Upon success contains allocated and initialized NetworkLatency
 *      instance for the given index. Note that strings addressed by "name" and
 *      "display" fileds in the returned NetworkLatency instance are containd
 *      inside the buffer allocated for the returned NetworkLatency instance.
 *      Caller of this routine must eventually free the buffer returned in this
 *      parameter.
 * Return:
 *  0 on success, or < 0 on failure.
 */
extern int corecmd_get_netdelay(int index, NetworkLatency** netdelay);

/* Requests a QEMU file path from the Core.
 * Param:
 *  type, filename - Request parameters that define the file for which path is
 *  requested.
 * Return:
 *  0 on success, or < 0 on failure.
 */
extern int corecmd_get_qemu_path(int type,
                                 const char* filename,
                                 char* path,
                                 size_t path_buf_size);

/* Gets LCD density property from the core properties.
 * Return:
 *  LCD density on success, or < 0 on failure.
 */
extern int corecmd_get_hw_lcd_density(void);

#endif /* _ANDROID_PROTOCOL_CORE_COMMANDS_API_H */
