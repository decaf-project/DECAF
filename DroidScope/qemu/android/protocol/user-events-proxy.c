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

#include "user-events.h"
#include "console.h"
#include "android/looper.h"
#include "android/async-utils.h"
#include "android/utils/debug.h"
#include "android/protocol/core-connection.h"
#include "android/protocol/user-events-protocol.h"
#include "android/protocol/user-events-proxy.h"

/* Descriptor for the user events client. */
typedef struct UserEventsProxy {
    /* Core connection instance for the user events client. */
    CoreConnection* core_connection;

    /* Socket for the client. */
    int             sock;

    /* Writes user events to the socket. */
    SyncSocket*     sync_writer;
} UserEventsProxy;

/* One and only one user events client instance. */
static UserEventsProxy _userEventsProxy = { 0 };

/* Sends an event to the core.
 * Parameters:
 *  event - Event type. Must be one of the AUSER_EVENT_XXX.
 *  event_param - Event parameters.
 *  size - Byte size of the event parameters buffer.
 * Return:
 *  0 on success, or -1 on failure.
 */
static int
_userEventsProxy_send(uint8_t event, const void* event_param, size_t size)
{
    int res;
    UserEventHeader header;

    header.event_type = event;
    res = syncsocket_start_write(_userEventsProxy.sync_writer);
    if (!res) {
        // Send event type first (event header)
        res = syncsocket_write(_userEventsProxy.sync_writer, &header,
                               sizeof(header),
                               core_connection_get_timeout(sizeof(header)));
        if (res > 0) {
            // Send event param next.
            res = syncsocket_write(_userEventsProxy.sync_writer, event_param,
                                   size,
                                   core_connection_get_timeout(sizeof(size)));
        }
        res = syncsocket_result(res);
        syncsocket_stop_write(_userEventsProxy.sync_writer);
    }
    if (res < 0) {
        derror("Unable to send user event: %s\n", errno_str);
    }
    return res;
}

int
userEventsProxy_create(SockAddress* console_socket)
{
    char* handshake = NULL;

    // Connect to the user-events service.
    _userEventsProxy.core_connection =
        core_connection_create_and_switch(console_socket, "user-events",
                                          &handshake);
    if (_userEventsProxy.core_connection == NULL) {
        derror("Unable to connect to the user-events service: %s\n",
               errno_str);
        return -1;
    }

    // Initialze event writer.
    _userEventsProxy.sock =
        core_connection_get_socket(_userEventsProxy.core_connection);
    _userEventsProxy.sync_writer = syncsocket_init(_userEventsProxy.sock);
    if (_userEventsProxy.sync_writer == NULL) {
        derror("Unable to initialize UserEventsProxy writer: %s\n", errno_str);
        userEventsProxy_destroy();
        return -1;
    }

    fprintf(stdout, "user-events is now connected to the core at %s.",
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
userEventsProxy_destroy(void)
{
    if (_userEventsProxy.sync_writer != NULL) {
        syncsocket_close(_userEventsProxy.sync_writer);
        syncsocket_free(_userEventsProxy.sync_writer);
        _userEventsProxy.sync_writer = NULL;
    }
    if (_userEventsProxy.core_connection != NULL) {
        core_connection_close(_userEventsProxy.core_connection);
        core_connection_free(_userEventsProxy.core_connection);
        _userEventsProxy.core_connection = NULL;
    }
}
void
user_event_keycodes(int *kcodes, int count)
{
    int nn;
    for (nn = 0; nn < count; nn++)
        user_event_keycode(kcodes[nn]);
}

void
user_event_keycode(int  kcode)
{
    UserEventKeycode    message;
    message.keycode = kcode;
    _userEventsProxy_send(AUSER_EVENT_KEYCODE, &message, sizeof(message));
}

void
user_event_key(unsigned code, unsigned down)
{
    if(code == 0) {
        return;
    }
    if (VERBOSE_CHECK(keys))
        printf(">> KEY [0x%03x,%s]\n", (code & 0x1ff), down ? "down" : " up " );

    user_event_keycode((code & 0x1ff) | (down ? 0x200 : 0));
}


void
user_event_mouse(int dx, int dy, int dz, unsigned buttons_state)
{
    UserEventMouse    message;
    message.dx = dx;
    message.dy = dy;
    message.dz = dz;
    message.buttons_state = buttons_state;
    _userEventsProxy_send(AUSER_EVENT_MOUSE, &message, sizeof(message));
}

void
user_event_register_generic(void* opaque, QEMUPutGenericEvent *callback)
{
}

void
user_event_generic(int type, int code, int value)
{
    UserEventGeneric    message;
    message.type = type;
    message.code = code;
    message.value = value;
    _userEventsProxy_send(AUSER_EVENT_GENERIC, &message, sizeof(message));
}
