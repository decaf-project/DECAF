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
 * Contains the Core-side implementation of the "core-ui-control" service that is
 * part of the UI control protocol. Here we send UI control commands to the UI.
 */

#include "android/android.h"
#include "android/hw-control.h"
#include "android/looper.h"
#include "android/async-utils.h"
#include "android/sync-utils.h"
#include "android/utils/debug.h"
#include "android/protocol/ui-commands.h"
#include "android/protocol/ui-commands-proxy.h"
#include "android/protocol/ui-commands-api.h"

/* Descriptor for the UI commands proxy. */
typedef struct UICmdProxy {
    /* I/O associated with this descriptor. */
    LoopIo          io;

    /* Looper associated with this descriptor. */
    Looper*         looper;

    /* Writer to send UI commands. */
    SyncSocket*     sync_writer;

    /* Socket descriptor for this service. */
    int             sock;
} UICmdProxy;

/* One and only one UICmdProxy instance. */
static UICmdProxy    _uiCmdProxy;

/* Implemented in android/console.c */
extern void destroy_uicmd_client(void);

/* Calculates timeout for transferring the given number of bytes via socket.
 * Return:
 *  Number of milliseconds during which the entire number of bytes is expected
 *  to be transferred via socket.
 */
static int
_uiCmdProxy_get_timeout(size_t data_size)
{
    // Min 2 seconds + 10 millisec for each transferring byte.
    // TODO: Come up with a better arithmetics here.
    return 2000 + data_size * 10;
}

/* Sends request to the UI client of this service.
 * Param:
 *  cmd_type, cmd_param, cmd_param_size - Define the command to send.
 * Return:
 *  0 on success, or < 0 on failure.
 */
static int
_uiCmdProxy_send_command(uint8_t cmd_type,
                         void* cmd_param,
                         uint32_t cmd_param_size)
{
    UICmdHeader header;
    int status = syncsocket_start_write(_uiCmdProxy.sync_writer);
    if (!status) {
        // Initialize and send the header.
        header.cmd_type = cmd_type;
        header.cmd_param_size = cmd_param_size;
        status = syncsocket_write(_uiCmdProxy.sync_writer, &header, sizeof(header),
                                  _uiCmdProxy_get_timeout(sizeof(header)));
        // If there are command parameters, send them too.
        if (status > 0 && cmd_param != NULL && cmd_param_size > 0) {
            status = syncsocket_write(_uiCmdProxy.sync_writer, cmd_param,
                                      cmd_param_size,
                                      _uiCmdProxy_get_timeout(cmd_param_size));
        }
        status = syncsocket_result(status);
        syncsocket_stop_write(_uiCmdProxy.sync_writer);
    }
    if (status < 0) {
        derror("Send UI command %d (%u bytes) has failed: %s\n",
               cmd_type, cmd_param_size, errno_str);
    }
    return status;
}

/* Asynchronous I/O callback for UICmdProxy instance.
 * We expect this callback to be called only on UI detachment condition. In this
 * case the event should be LOOP_IO_READ, and read should fail with errno set
 * to ECONNRESET.
 * Param:
 *  opaque - UICmdProxy instance.
 */
static void
_uiCmdProxy_io_func(void* opaque, int fd, unsigned events)
{
    UICmdProxy* uicmd = (UICmdProxy*)opaque;
    AsyncReader reader;
    AsyncStatus status;
    uint8_t read_buf[1];

    if (events & LOOP_IO_WRITE) {
        derror("Unexpected LOOP_IO_WRITE in _uiCmdProxy_io_func.\n");
        return;
    }

    // Try to read
    asyncReader_init(&reader, read_buf, sizeof(read_buf), &uicmd->io);
    status = asyncReader_read(&reader);
    // We expect only error status here.
    if (status != ASYNC_ERROR) {
        derror("Unexpected read status %d in _uiCmdProxy_io_func\n", status);
        return;
    }
    // We expect only socket disconnection error here.
    if (errno != ECONNRESET) {
        derror("Unexpected read error %d (%s) in _uiCmdProxy_io_func.\n",
               errno, errno_str);
        return;
    }

    // Client got disconnectted.
    destroy_uicmd_client();
}
/* a callback function called when the system wants to change the brightness
 * of a given light. 'light' is a string which can be one of:
 * 'lcd_backlight', 'button_backlight' or 'Keyboard_backlight'
 *
 * brightness is an integer (acceptable range are 0..255), however the
 * default is around 105, and we probably don't want to dim the emulator's
 * output at that level.
 */
static void
_uiCmdProxy_brightness_change_callback(void* opaque,
                                       const char* light,
                                       int brightness)
{
    // Calculate size of the command parameters.
    const size_t cmd_size = sizeof(UICmdChangeDispBrightness) + strlen(light) + 1;
    // Allocate and initialize parameters.
    UICmdChangeDispBrightness* cmd =
        (UICmdChangeDispBrightness*)qemu_malloc(cmd_size);
    cmd->brightness = brightness;
    strcpy(cmd->light, light);
    // Send the command.
    _uiCmdProxy_send_command(AUICMD_CHANGE_DISP_BRIGHTNESS, cmd, cmd_size);
    qemu_free(cmd);
}

int
uiCmdProxy_create(int fd)
{
    // Initialize the only UICmdProxy instance.
    _uiCmdProxy.sock = fd;
    _uiCmdProxy.looper = looper_newCore();
    loopIo_init(&_uiCmdProxy.io, _uiCmdProxy.looper, _uiCmdProxy.sock,
                _uiCmdProxy_io_func, &_uiCmdProxy);
    loopIo_wantRead(&_uiCmdProxy.io);
    _uiCmdProxy.sync_writer = syncsocket_init(fd);
    if (_uiCmdProxy.sync_writer == NULL) {
        derror("Unable to initialize UICmdProxy writer: %s\n", errno_str);
        uiCmdProxy_destroy();
        return -1;
    }
    {
        // Set brighness change callback, so we can notify
        // the UI about the event.
        AndroidHwControlFuncs  funcs;
        funcs.light_brightness = _uiCmdProxy_brightness_change_callback;
        android_hw_control_set(&_uiCmdProxy, &funcs);
    }
    return 0;
}

void
uiCmdProxy_destroy()
{
    // Destroy the sync writer.
    if (_uiCmdProxy.sync_writer != NULL) {
        syncsocket_close(_uiCmdProxy.sync_writer);
        syncsocket_free(_uiCmdProxy.sync_writer);
    }
    if (_uiCmdProxy.looper != NULL) {
        // Stop all I/O that may still be going on.
        loopIo_done(&_uiCmdProxy.io);
        looper_free(_uiCmdProxy.looper);
        _uiCmdProxy.looper = NULL;
    }
    _uiCmdProxy.sock = -1;
}

int
uicmd_set_window_scale(double scale, int is_dpi)
{
    UICmdSetWindowsScale cmd;
    cmd.scale = scale;
    cmd.is_dpi = is_dpi;
    return _uiCmdProxy_send_command(AUICMD_SET_WINDOWS_SCALE, &cmd, sizeof(cmd));
}
