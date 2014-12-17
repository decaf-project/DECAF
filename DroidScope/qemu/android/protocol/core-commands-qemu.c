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
 * Contains implementation of the API for calling into the Core with the UI
 * control commands for standalone (monolithic) emulator.
 */

#include "android/android.h"
#include "android/globals.h"
#include "android/hw-sensors.h"
#include "telephony/modem_driver.h"
#include "android-trace.h"
#include "audio/audio.h"
#include "android/protocol/core-commands-api.h"

/* Implemented in vl-android.c */
extern char* qemu_find_file(int type, const char* filename);

int
corecmd_set_coarse_orientation(AndroidCoarseOrientation orient)
{
    android_sensors_set_coarse_orientation(orient);
    return 0;
}

int
corecmd_toggle_network()
{
    qemu_net_disable = !qemu_net_disable;
    if (android_modem) {
        amodem_set_data_registration(
                android_modem,
        qemu_net_disable ? A_REGISTRATION_UNREGISTERED
            : A_REGISTRATION_HOME);
    }
    return 0;
}

int corecmd_trace_control(int start)
{
    if (start) {
        start_tracing();
    } else {
        stop_tracing();
    }
    return 0;
}

int corecmd_is_network_disabled()
{
    return qemu_net_disable;
}

int
corecmd_get_netspeed(int index, NetworkSpeed** netspeed)
{
    if (index >= android_netspeeds_count ||
        android_netspeeds[index].name == NULL) {
        return -1;
    }
    *netspeed = (NetworkSpeed*)malloc(sizeof(NetworkSpeed));
    memcpy(*netspeed, &android_netspeeds[index], sizeof(NetworkSpeed));
    return 0;
}

int
corecmd_get_netdelay(int index, NetworkLatency** netdelay)
{
    if (index >= android_netdelays_count ||
        android_netdelays[index].name == NULL) {
        return -1;
    }
    *netdelay = (NetworkLatency*)malloc(sizeof(NetworkLatency));
    memcpy(*netdelay, &android_netdelays[index], sizeof(NetworkLatency));
    return 0;
}

int
corecmd_get_qemu_path(int type,
                      const char* filename,
                      char* path,
                      size_t path_buf_size)
{
    char* filepath = qemu_find_file(type, filename);
    if (filepath == NULL) {
        return -1;
    }
    strncpy(path, filepath, path_buf_size);
    path[path_buf_size - 1] = '\0';
    qemu_free(filepath);
    return 0;
}

int
corecmd_get_hw_lcd_density(void)
{
    return android_hw->hw_lcd_density;
}
