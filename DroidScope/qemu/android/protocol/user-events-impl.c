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
 * Contains the Core-side of the "user events" service. Here we receive and
 * handle user events sent from the UI.
 */

#include "user-events.h"
#include "android/globals.h"
#include "android/android.h"
#include "android/looper.h"
#include "android/async-utils.h"
#include "android/sync-utils.h"
#include "android/utils/system.h"
#include "android/utils/debug.h"
#include "android/protocol/user-events-protocol.h"
#include "android/protocol/user-events-impl.h"

/* Enumerates state values for the event reader in the UserEventsImpl descriptor.
 */
typedef enum UserEventsImplState {
    /* The reader is waiting on event header. */
    EXPECTS_HEADER,

    /* The reader is waiting on event parameters. */
    EXPECTS_PARAMETERS,
} UserEventsImplState;


/* Core user events service descriptor. */
typedef struct UserEventsImpl {
    /* Reader to receive user events. */
    AsyncReader         user_events_reader;

    /* I/O associated with this descriptor. */
    LoopIo              io;

    /* Looper used to communicate user events. */
    Looper*             looper;

    /* Socket for this service. */
    int                 sock;

    /* State of the service (see UE_STATE_XXX for possible values). */
    UserEventsImplState state;

    /* Current event header. */
    UserEventHeader     event_header;

    /* Current event parameters. */
    union {
        UserEventGeneric    generic_event;
        UserEventMouse      mouse_event;
        UserEventKeycode    keycode_event;
    };
} UserEventsImpl;

/* Implemented in android/console.c */
extern void destroy_user_events_client(void);

/* One and only one UserEventsImpl instance. */
static UserEventsImpl   _UserEventsImpl;

/* Asynchronous I/O callback reading user events.
 * Param:
 *  opaque - UserEventsImpl instance.
 */
static void
_userEventsImpl_io_func(void* opaque, int fd, unsigned events)
{
    UserEventsImpl* ueimpl;
    AsyncStatus status;

    if (events & LOOP_IO_WRITE) {
        // We don't use async writer here, so we don't expect
        // any write callbacks.
        derror("Unexpected LOOP_IO_WRITE in _userEventsImpl_io_func\n");
        return;
    }

    ueimpl = (UserEventsImpl*)opaque;
    // Read whatever is expected from the socket.
    status = asyncReader_read(&ueimpl->user_events_reader);


    switch (status) {
        case ASYNC_COMPLETE:
            switch (ueimpl->state) {
                case EXPECTS_HEADER:
                    // We just read event header. Now we expect event parameters.
                    ueimpl->state = EXPECTS_PARAMETERS;
                    // Setup the reader depending on the event type.
                    switch (ueimpl->event_header.event_type) {
                        case AUSER_EVENT_MOUSE:
                            asyncReader_init(&ueimpl->user_events_reader,
                                             &ueimpl->mouse_event,
                                             sizeof(ueimpl->mouse_event),
                                             &ueimpl->io);
                            break;

                        case AUSER_EVENT_KEYCODE:
                            asyncReader_init(&ueimpl->user_events_reader,
                                             &ueimpl->keycode_event,
                                             sizeof(ueimpl->keycode_event),
                                             &ueimpl->io);
                            break;

                        case AUSER_EVENT_GENERIC:
                            asyncReader_init(&ueimpl->user_events_reader,
                                             &ueimpl->generic_event,
                                             sizeof(ueimpl->generic_event),
                                             &ueimpl->io);
                            break;

                        default:
                            derror("Unexpected user event type %d\n",
                                   ueimpl->event_header.event_type);
                            break;
                    }
                    break;

                case EXPECTS_PARAMETERS:
                    // We just read event parameters. Lets fire the event.
                    switch (ueimpl->event_header.event_type) {
                        case AUSER_EVENT_MOUSE:
                            user_event_mouse(ueimpl->mouse_event.dx,
                                             ueimpl->mouse_event.dy,
                                             ueimpl->mouse_event.dz,
                                             ueimpl->mouse_event.buttons_state);
                            break;

                        case AUSER_EVENT_KEYCODE:
                            user_event_keycode(ueimpl->keycode_event.keycode);
                            break;

                        case AUSER_EVENT_GENERIC:
                            user_event_generic(ueimpl->generic_event.type,
                                               ueimpl->generic_event.code,
                                               ueimpl->generic_event.value);
                            break;

                        default:
                            derror("Unexpected user event type %d\n",
                                   ueimpl->event_header.event_type);
                            break;
                    }
                    // Prepare to receive the next event header.
                    ueimpl->event_header.event_type = -1;
                    ueimpl->state = EXPECTS_HEADER;
                    asyncReader_init(&ueimpl->user_events_reader,
                                     &ueimpl->event_header,
                                     sizeof(ueimpl->event_header), &ueimpl->io);
                    break;
            }
            break;
        case ASYNC_ERROR:
            loopIo_dontWantRead(&ueimpl->io);
            if (errno == ECONNRESET) {
                // UI has exited. We need to destroy user event service.
                destroy_user_events_client();
            } else {
                derror("User event read error %d -> %s\n", errno, errno_str);
            }
            break;

        case ASYNC_NEED_MORE:
            // Transfer will eventually come back into this routine.
            return;
    }
}

int
userEventsImpl_create(int fd)
{
    _UserEventsImpl.sock = fd;
    _UserEventsImpl.event_header.event_type = -1;
    _UserEventsImpl.state = EXPECTS_HEADER;
    _UserEventsImpl.looper = looper_newCore();
    loopIo_init(&_UserEventsImpl.io, _UserEventsImpl.looper, _UserEventsImpl.sock,
                _userEventsImpl_io_func, &_UserEventsImpl);
    asyncReader_init(&_UserEventsImpl.user_events_reader,
                     &_UserEventsImpl.event_header,
                     sizeof(_UserEventsImpl.event_header), &_UserEventsImpl.io);
    return 0;
}

void
userEventsImpl_destroy(void)
{
    if (_UserEventsImpl.looper != NULL) {
        // Stop all I/O that may still be going on.
        loopIo_done(&_UserEventsImpl.io);
        looper_free(_UserEventsImpl.looper);
        _UserEventsImpl.looper = NULL;
    }
}
