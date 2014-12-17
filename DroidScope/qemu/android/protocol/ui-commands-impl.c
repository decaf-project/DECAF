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

#include <unistd.h>
#include "android/looper.h"
#include "android/async-utils.h"
#include "android/sync-utils.h"
#include "android/utils/system.h"
#include "android/utils/debug.h"
#include "android/utils/panic.h"
#include "android/protocol/core-connection.h"
#include "android/protocol/ui-commands-impl.h"
#include "android/protocol/ui-commands-api.h"

/* Enumerates states for the command reader in UICmdImpl instance. */
typedef enum UICmdImplState {
    /* The reader is waiting on command header. */
    EXPECTS_HEADER,

    /* The reader is waiting on command parameters. */
    EXPECTS_PARAMETERS,
} UICmdImplState;

/* Descriptor for the UI-side of the "core-ui-control" service. */
typedef struct UICmdImpl {
    /* Core connection established for this service. */
    CoreConnection* core_connection;

    /* Socket descriptor for the UI service. */
    int             sock;

    /* Custom i/o handler */
    LoopIo          io[1];

    /* Command reader state. */
    UICmdImplState  reader_state;

    /* Incoming command header. */
    UICmdHeader     cmd_header;

    /* Reader's buffer. This field can point to the cmd_header field of this
     * structure (when we expect a command header), or to a buffer allocated for
     * the (when we expect command parameters). */
    uint8_t*        reader_buffer;

    /* Offset in the reader's buffer where to read next chunk of data. */
    size_t          reader_offset;

    /* Total number of bytes the reader expects to read. */
    size_t          reader_bytes;
} UICmdImpl;

/* Implemented in android/qemulator.c */
extern void android_emulator_set_window_scale(double scale, int is_dpi);

/* One and only one UICmdImpl instance. */
static UICmdImpl  _uiCmdImpl;

/* Display brightness change callback. */
static AndroidHwLightBrightnessCallback _brightness_change_callback = NULL;
static void* _brightness_change_callback_param = NULL;

/* Handles UI control command received from the core.
 * Param:
 *  uicmd - UICmdImpl instance that received the command.
 *  header - UI control command header.
 *  data - Command parameters formatted accordingly to the command type.
 */
static void
_uiCmdImpl_handle_command(UICmdImpl* uicmd,
                          const UICmdHeader* header,
                          const uint8_t* data)
{
    switch (header->cmd_type) {
        case AUICMD_SET_WINDOWS_SCALE:
        {
            UICmdSetWindowsScale* cmd = (UICmdSetWindowsScale*)data;
            android_emulator_set_window_scale(cmd->scale, cmd->is_dpi);
            break;
        }

        case AUICMD_CHANGE_DISP_BRIGHTNESS:
        {
            UICmdChangeDispBrightness* cmd = (UICmdChangeDispBrightness*)data;
            if (_brightness_change_callback != NULL) {
                _brightness_change_callback(_brightness_change_callback_param,
                                            cmd->light, cmd->brightness);
            }
            break;
        }

        default:
            derror("Unknown command %d is received from the Core\n",
                   header->cmd_type);
            break;
    }
}

/* Asynchronous I/O callback reading UI control commands.
 * Param:
 *  opaque - UICmdImpl instance.
 */
static void
_uiCmdImpl_io_callback(void* opaque, int fd, unsigned events)
{
    UICmdImpl* uicmd = opaque;
    int status;

    // Read requests while they are immediately available.
    for (;;) {
        // Read next chunk of data.
        status = socket_recv(uicmd->sock,
                             uicmd->reader_buffer + uicmd->reader_offset,
                             uicmd->reader_bytes - uicmd->reader_offset);
        if (status == 0) {
            /* Disconnection, meaning that the core process got terminated. */
            fprintf(stderr, "core-ui-control service got disconnected\n");
            uiCmdImpl_destroy();
            return;
        }
        if (status < 0) {
            if (errno == EINTR) {
                /* loop on EINTR */
                continue;
            } else if (errno == EWOULDBLOCK || errno == EAGAIN) {
                // Chunk is not avalable at this point. Come back later.
                return;
            }
        }

        uicmd->reader_offset += status;
        if (uicmd->reader_offset != uicmd->reader_bytes) {
            // There are still some data left in the pipe.
            continue;
        }

        // All expected data has been read. Time to change the state.
        if (uicmd->reader_state == EXPECTS_HEADER) {
            // Header has been read.
            if (uicmd->cmd_header.cmd_param_size) {
                // Prepare for the command parameters.
                uicmd->reader_state = EXPECTS_PARAMETERS;
                uicmd->reader_offset = 0;
                uicmd->reader_bytes = uicmd->cmd_header.cmd_param_size;
                uicmd->reader_buffer = malloc(uicmd->reader_bytes);
                if (uicmd->reader_buffer == NULL) {
                    APANIC("Unable to allocate memory for UI command parameters.\n");
                }
            } else {
                // This command doesn't have any parameters. Handle it now.
                _uiCmdImpl_handle_command(uicmd, &uicmd->cmd_header, NULL);
                // Prepare for the next command header.
                uicmd->reader_state = EXPECTS_HEADER;
                uicmd->reader_offset = 0;
                uicmd->reader_bytes = sizeof(uicmd->cmd_header);
                uicmd->reader_buffer = (uint8_t*)&uicmd->cmd_header;
            }
        } else {
            // All command data is in. Handle it.
            _uiCmdImpl_handle_command(uicmd, &uicmd->cmd_header,
                                      uicmd->reader_buffer);
            // Prepare for the next command header.
            free(uicmd->reader_buffer);
            uicmd->reader_state = EXPECTS_HEADER;
            uicmd->reader_offset = 0;
            uicmd->reader_bytes = sizeof(uicmd->cmd_header);
            uicmd->reader_buffer = (uint8_t*)&uicmd->cmd_header;
        }
    }
}

int
uiCmdImpl_create(SockAddress* console_socket, Looper* looper)
{
    UICmdImpl* uicmd = &_uiCmdImpl;
    char* handshake = NULL;

    // Setup command reader.
    uicmd->reader_buffer = (uint8_t*)&uicmd->cmd_header;
    uicmd->reader_state = EXPECTS_HEADER;
    uicmd->reader_offset = 0;
    uicmd->reader_bytes = sizeof(UICmdHeader);

    // Connect to the core-ui-control service.
    uicmd->core_connection =
        core_connection_create_and_switch(console_socket, "core-ui-control",
                                          &handshake);
    if (uicmd->core_connection == NULL) {
        derror("Unable to connect to the core-ui-control service: %s\n",
               errno_str);
        return -1;
    }

    // Initialize UI command reader.
    uicmd->sock = core_connection_get_socket(uicmd->core_connection);
    loopIo_init(uicmd->io, looper, uicmd->sock,
                _uiCmdImpl_io_callback,
                &_uiCmdImpl);
    loopIo_wantRead(uicmd->io);

    fprintf(stdout, "core-ui-control is now connected to the core at %s.",
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
uiCmdImpl_destroy(void)
{
    UICmdImpl* uicmd = &_uiCmdImpl;

    if (uicmd->core_connection != NULL) {
        // Disable I/O callbacks.
        loopIo_done(uicmd->io);
        core_connection_close(uicmd->core_connection);
        core_connection_free(uicmd->core_connection);
        uicmd->core_connection = NULL;
    }
    // Properly deallocate the reader buffer.
    if (uicmd->reader_buffer != NULL &&
        uicmd->reader_buffer != (uint8_t*)&uicmd->cmd_header) {
        free(uicmd->reader_buffer);
        uicmd->reader_buffer = (uint8_t*)&uicmd->cmd_header;
    }
}

int
uicmd_set_brightness_change_callback(AndroidHwLightBrightnessCallback callback,
                                     void* opaque)
{
    _brightness_change_callback = callback;
    _brightness_change_callback_param = opaque;
    return 0;
}
