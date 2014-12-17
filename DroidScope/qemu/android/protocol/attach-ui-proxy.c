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
 * Contains the Core-side implementation of the "attach-ui" service that is
 * used to establish connection between the UI and the Core.
 */

#include "android/android.h"
#include "android/globals.h"
#include "android/looper.h"
#include "android/async-utils.h"
#include "android/sync-utils.h"
#include "android/utils/debug.h"
#include "android/protocol/core-commands.h"
#include "android/protocol/core-commands-impl.h"

/* Descriptor for the UI attach-ui proxy. */
typedef struct AttachUIProxy {
    /* Reader to detect UI disconnection. */
    AsyncReader         async_reader;

    /* I/O associated with this descriptor. */
    LoopIo              io;

    /* Looper used to communicate with the UI. */
    Looper*             looper;

    /* Socket descriptor for this service. */
    int                 sock;
} AttachUIProxy;

/* One and only one AttachUIProxy instance. */
static AttachUIProxy    _attachUiProxy;

/* Implemented in android/console.c */
extern void destroy_attach_ui_client(void);

/* Asynchronous I/O callback for AttachUIProxy instance.
 * We expect this callback to be called only on UI detachment condition. In this
 * case the event should be LOOP_IO_READ, and read should fail with errno set
 * to ECONNRESET.
 * Param:
 *  opaque - AttachUIProxy instance.
 */
static void
_attachUiProxy_io_func(void* opaque, int fd, unsigned events)
{
    AttachUIProxy* uicmd = (AttachUIProxy*)opaque;
    AsyncReader reader;
    AsyncStatus status;
    uint8_t read_buf[1];

    if (events & LOOP_IO_WRITE) {
        derror("Unexpected LOOP_IO_WRITE in _attachUiProxy_io_func.\n");
        return;
    }

    // Try to read
    asyncReader_init(&reader, read_buf, sizeof(read_buf), &uicmd->io);
    status = asyncReader_read(&reader);
    // We expect only error status here.
    if (status != ASYNC_ERROR) {
        derror("Unexpected read status %d in _attachUiProxy_io_func\n", status);
        return;
    }
    // We expect only socket disconnection error here.
    if (errno != ECONNRESET) {
        derror("Unexpected read error %d (%s) in _attachUiProxy_io_func.\n",
               errno, errno_str);
        return;
    }

    // Client got disconnectted.
    destroy_attach_ui_client();
}

int
attachUiProxy_create(int fd)
{
    // Initialize the only AttachUIProxy instance.
    _attachUiProxy.sock = fd;
    _attachUiProxy.looper = looper_newCore();
    loopIo_init(&_attachUiProxy.io, _attachUiProxy.looper, _attachUiProxy.sock,
                _attachUiProxy_io_func, &_attachUiProxy);
    loopIo_wantRead(&_attachUiProxy.io);

    return 0;
}

void
attachUiProxy_destroy(void)
{
    if (_attachUiProxy.looper != NULL) {
        // Stop all I/O that may still be going on.
        loopIo_done(&_attachUiProxy.io);
        looper_free(_attachUiProxy.looper);
        _attachUiProxy.looper = NULL;
    }
    _attachUiProxy.sock = -1;
}
