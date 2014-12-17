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
 * Contains implementation of routines and that are used in the course
 * of the emulator's core initialization.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "qemu-common.h"
#include "sockets.h"
#include "android/android.h"
#include "android/core-init-utils.h"
#include "android/utils/bufprint.h"

extern char* android_op_ui_port;

/* Sends core initialization status message back to the UI that started this
 * core process.
 * Param:
 *  msg - Message to send. On success, message must begin with "ok:", followed
 *      by "port=<console port number>". On initialization failure, message must
 *      begin with "ko:", followed by a string describing the reason of failure.
 */
static void
android_core_send_init_response(const char* msg)
{
    int fd;
    int ui_port;

    if (android_op_ui_port == NULL) {
        // No socket - no reply.
        return;
    }

    ui_port = atoi(android_op_ui_port);
    if (ui_port >= 0) {
        // At this point UI always starts the core on the same workstation.
        fd = socket_loopback_client(ui_port, SOCKET_STREAM);
        if (fd == -1) {
            fprintf(stderr, "Unable to create UI socket client for port %s: %s\n",
                    android_op_ui_port, errno_str);
            return;
        }
        socket_send(fd, msg, strlen(msg) + 1);
        socket_close(fd);
    } else {
        fprintf(stderr, "Invalid -ui-port parameter: %s\n", android_op_ui_port);
    }
}

void
android_core_init_completed(void)
{
    char msg[32];
    snprintf(msg, sizeof(msg), "ok:port=%d", android_base_port);
    android_core_send_init_response(msg);
}

void
android_core_init_failure(const char* fmt, ...)
{
    va_list  args;
    char msg[4096];

    // Format "ko" message to send back to the UI.
    snprintf(msg, sizeof(msg), "ko:");

    va_start(args, fmt);
    vbufprint(msg + strlen(msg), msg + sizeof(msg), fmt, args);
    va_end(args);

    // Send message back to the UI, print it to the error stdout, and exit the
    // process.
    android_core_send_init_response(msg);
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

void
android_core_init_exit(int exit_status)
{
    char msg[32];
    // Build "ok" message with the exit status, and send it back to the UI.
    snprintf(msg, sizeof(msg), "ok:status=%d", exit_status);
    android_core_send_init_response(msg);
    exit(exit_status);
}
