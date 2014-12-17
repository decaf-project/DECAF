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
 * Contains the UI-side implementation of the "core-ui-control" service that is
 * part of the UI control protocol. Here we handle UI control commands received
 * from the Core.
 */

#include "android/utils/debug.h"
#include "android/utils/panic.h"
#include "android/protocol/core-connection.h"
#include "android/protocol/attach-ui-impl.h"

/* Descriptor for the UI-side of the "attach-ui" service. */
typedef struct AttachUIImpl {
    /* Address of the Core's console socket. */
    SockAddress     console_socket;

    /* Core connection established for this service. */
    CoreConnection* core_connection;

    /* Socket descriptor for the UI service. */
    int             sock;
} AttachUIImpl;

/* One and only one AttachUIImpl instance. */
static AttachUIImpl  _attachUiImpl;

SockAddress*
attachUiImpl_get_console_socket(void)
{
    return &_attachUiImpl.console_socket;
}

int
attachUiImpl_create(SockAddress* console_socket)
{
    char* handshake = NULL;

    _attachUiImpl.console_socket = *console_socket;

    // Connect to the core-ui-control service.
    _attachUiImpl.core_connection =
        core_connection_create_and_switch(console_socket, "attach-UI",
                                          &handshake);
    if (_attachUiImpl.core_connection == NULL) {
        derror("Unable to connect to the attach-UI service: %s\n",
               errno_str);
        return -1;
    }

    fprintf(stdout, "UI is now attached to the core at %s.",
            sock_address_to_string(console_socket));
    if (handshake != NULL) {
        if (handshake[0] != '\0') {
            fprintf(stdout, " Handshake: %s", handshake);
        }
        free(handshake);
    }
    fprintf(stdout, "\n");

    return 0;
}

void
attachUiImpl_destroy(void)
{
    if (_attachUiImpl.core_connection != NULL) {
        core_connection_close(_attachUiImpl.core_connection);
        core_connection_free(_attachUiImpl.core_connection);
        _attachUiImpl.core_connection = NULL;
    }
}
