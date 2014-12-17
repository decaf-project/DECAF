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

#ifndef _ANDROID_PROTOCOL_CORE_COMMANDS_H
#define _ANDROID_PROTOCOL_CORE_COMMANDS_H

/*
 * Contains declarations related to the UI control commands sent by the UI and
 * handled by the Core.
 */

#include "android/hw-sensors.h"
#include "android/protocol/ui-common.h"

/* Sets coarse orientation. */
#define AUICMD_SET_COARSE_ORIENTATION       1

/* Toggles the network. */
#define AUICMD_TOGGLE_NETWORK               2

/* Starts / stops the tracing. */
#define AUICMD_TRACE_CONTROL                3

/* Checks if network is disabled. */
#define AUICMD_CHK_NETWORK_DISABLED         4

/* Gets network speed. */
#define AUICMD_GET_NETSPEED                 5

/* Gets network delays */
#define AUICMD_GET_NETDELAY                 6

/* Gets path to a QEMU file on local host. */
#define AUICMD_GET_QEMU_PATH                7

/* Gets LCD density. */
#define AUICMD_GET_LCD_DENSITY              8

/* Formats AUICMD_SET_COARSE_ORIENTATION UI control command parameters. */
typedef struct UICmdSetCoarseOrientation {
    AndroidCoarseOrientation    orient;
} UICmdSetCoarseOrientation;

/* Formats AUICMD_TRACE_CONTROL UI control command parameters. */
typedef struct UICmdTraceControl {
    int start;
} UICmdTraceControl;

/* Formats AUICMD_GET_NETSPEED UI control command parameters. */
typedef struct UICmdGetNetSpeed {
    int index;
} UICmdGetNetSpeed;

/* Formats AUICMD_GET_NETSPEED UI control command response.
 * Instances of this structure contains content of the NetworkSpeed structure,
 * including actual "name" and "display" strings. */
typedef struct UICmdGetNetSpeedResp {
    int     upload;
    int     download;
    /* Zero-terminated NetworkSpeed's "name" strings starts here. The "display"
     * string begins inside this structure, right after the "name"'s
     * zero-terminator. */
    char    name[0];
} UICmdGetNetSpeedResp;

/* Formats AUICMD_GET_NETDELAY UI control command parameters. */
typedef struct UICmdGetNetDelay {
    int index;
} UICmdGetNetDelay;

/* Formats AUICMD_GET_NETDELAY UI control command response.
 * Instances of this structure contains content of the NetworkLatency structure,
 * including actual "name" and "display" strings. */
typedef struct UICmdGetNetDelayResp {
    int     min_ms;
    int     max_ms;
    /* Zero-terminated NetworkLatency's "name" strings starts here. The "display"
     * string begins inside this structure, right after the "name"'s
     * zero-terminator. */
    char    name[0];
} UICmdGetNetDelayResp;

/* Formats AUICMD_GET_QEMU_PATH UI control command parameters. */
typedef struct UICmdGetQemuPath {
    int     type;
    char    filename[0];
} UICmdGetQemuPath;

/* Formats AUICMD_GET_QEMU_PATH UI control command response. */
typedef struct UICmdGetQemuPathResp {
    /* Queried qemu path begins here. */
    char    path[0];
} UICmdGetQemuPathResp;

#endif /* _ANDROID_PROTOCOL_CORE_COMMANDS_H */
