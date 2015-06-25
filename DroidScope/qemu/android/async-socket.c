/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Encapsulates exchange protocol between the emulator, and an Android device
 * that is connected to the host via USB. The communication is established over
 * a TCP port forwarding, enabled by ADB.
 */

#include "android/utils/debug.h"
#include "android/async-socket-connector.h"
#include "android/async-socket.h"
#include "utils/panic.h"
#include "iolooper.h"

#define  E(...)    derror(__VA_ARGS__)
#define  W(...)    dwarning(__VA_ARGS__)
#define  D(...)    VERBOSE_PRINT(asyncsocket,__VA_ARGS__)
#define  D_ACTIVE  VERBOSE_CHECK(asyncsocket)

#define TRACE_ON    0

#if TRACE_ON
#define  T(...)    VERBOSE_PRINT(asyncsocket,__VA_ARGS__)
#else
#define  T(...)
#endif

/********************************************************************************
 *                  Asynchronous Socket internal API declarations
 *******************************************************************************/

/* Gets socket's address string. */
static const char* _async_socket_string(AsyncSocket* as);

/* Gets socket's looper. */
static Looper* _async_socket_get_looper(AsyncSocket* as);

/* Handler for the I/O time out.
 * Param:
 *  as - Asynchronous socket for the I/O.
 *  asio - Desciptor for the timed out I/O.
 */
static AsyncIOAction _async_socket_io_timed_out(AsyncSocket* as,
                                                AsyncSocketIO* asio);

/********************************************************************************
 *                  Asynchronous Socket Reader / Writer
 *******************************************************************************/

struct AsyncSocketIO {
    /* Next I/O in the reader, or writer list. */
    AsyncSocketIO*      next;
    /* Asynchronous socket for this I/O. */
    AsyncSocket*        as;
    /* Timer used for time outs on this I/O. */
    LoopTimer           timer[1];
    /* An opaque pointer associated with this I/O. */
    void*               io_opaque;
    /* Buffer where to read / write data. */
    uint8_t*            buffer;
    /* Bytes to transfer through the socket for this I/O. */
    uint32_t            to_transfer;
    /* Bytes thransferred through the socket in this I/O. */
    uint32_t            transferred;
    /* I/O callback for this I/O. */
    on_as_io_cb         on_io;
    /* I/O type selector: 1 - read, 0 - write. */
    int                 is_io_read;
    /* State of the I/O. */
    AsyncIOState        state;
    /* Number of outstanding references to the I/O. */
    int                 ref_count;
    /* Deadline for this I/O */
    Duration            deadline;
};

/*
 * Recycling I/O instances.
 * Since AsyncSocketIO instances are not that large, it makes sence to recycle
 * them for faster allocation, rather than allocating and freeing them for each
 * I/O on the socket.
 */

/* List of recycled I/O descriptors. */
static AsyncSocketIO* _asio_recycled    = NULL;
/* Number of I/O descriptors that are recycled in the _asio_recycled list. */
static int _recycled_asio_count         = 0;
/* Maximum number of I/O descriptors that can be recycled. */
static const int _max_recycled_asio_num = 32;

/* Handler for an I/O time-out timer event.
 * When this routine is invoked, it indicates that a time out has occurred on an
 * I/O.
 * Param:
 *  opaque - AsyncSocketIO instance representing the timed out I/O.
 */
static void _on_async_socket_io_timed_out(void* opaque);

/* Creates new I/O descriptor.
 * Param:
 *  as - Asynchronous socket for the I/O.
 *  is_io_read - I/O type selector: 1 - read, 0 - write.
 *  buffer, len - Reader / writer buffer address.
 *  io_cb - Callback for this reader / writer.
 *  io_opaque - An opaque pointer associated with the I/O.
 *  deadline - Deadline to complete the I/O.
 * Return:
 *  Initialized AsyncSocketIO instance.
 */
static AsyncSocketIO*
_async_socket_rw_new(AsyncSocket* as,
                     int is_io_read,
                     void* buffer,
                     uint32_t len,
                     on_as_io_cb io_cb,
                     void* io_opaque,
                     Duration deadline)
{
    /* Lookup in the recycler first. */
    AsyncSocketIO* asio = _asio_recycled;
    if (asio != NULL) {
        /* Pull the descriptor from recycler. */
        _asio_recycled = asio->next;
        _recycled_asio_count--;
    } else {
        /* No recycled descriptors. Allocate new one. */
        ANEW0(asio);
    }

    asio->next          = NULL;
    asio->as            = as;
    asio->is_io_read    = is_io_read;
    asio->buffer        = (uint8_t*)buffer;
    asio->to_transfer   = len;
    asio->transferred   = 0;
    asio->on_io         = io_cb;
    asio->io_opaque     = io_opaque;
    asio->state         = ASIO_STATE_QUEUED;
    asio->ref_count     = 1;
    asio->deadline      = deadline;
    loopTimer_init(asio->timer, _async_socket_get_looper(as),
                   _on_async_socket_io_timed_out, asio);
    loopTimer_startAbsolute(asio->timer, deadline);

    /* Reference socket that is holding this I/O. */
    async_socket_reference(as);

    T("ASocket %s: %s I/O descriptor %p is created for %d bytes of data",
      _async_socket_string(as), is_io_read ? "READ" : "WRITE", asio, len);

    return asio;
}

/* Destroys and frees I/O descriptor. */
static void
_async_socket_io_free(AsyncSocketIO* asio)
{
    AsyncSocket* const as = asio->as;

    T("ASocket %s: %s I/O descriptor %p is destroyed.",
      _async_socket_string(as), asio->is_io_read ? "READ" : "WRITE", asio);

    loopTimer_done(asio->timer);

    /* Try to recycle it first, and free the memory if recycler is full. */
    if (_recycled_asio_count < _max_recycled_asio_num) {
        asio->next = _asio_recycled;
        _asio_recycled = asio;
        _recycled_asio_count++;
    } else {
        AFREE(asio);
    }

    /* Release socket that is holding this I/O. */
    async_socket_release(as);
}

/* An I/O has been finished and its descriptor is about to be discarded. */
static void
_async_socket_io_finished(AsyncSocketIO* asio)
{
    /* Notify the client of the I/O that I/O is finished. */
    asio->on_io(asio->io_opaque, asio, ASIO_STATE_FINISHED);
}

int
async_socket_io_reference(AsyncSocketIO* asio)
{
    assert(asio->ref_count > 0);
    asio->ref_count++;
    return asio->ref_count;
}

int
async_socket_io_release(AsyncSocketIO* asio)
{
    assert(asio->ref_count > 0);
    asio->ref_count--;
    if (asio->ref_count == 0) {
        _async_socket_io_finished(asio);
        /* Last reference has been dropped. Destroy this object. */
        _async_socket_io_free(asio);
        return 0;
    }
    return asio->ref_count;
}

/* Creates new asynchronous socket reader.
 * Param:
 *  as - Asynchronous socket for the reader.
 *  buffer, len - Reader's buffer.
 *  io_cb - Reader's callback.
 *  reader_opaque - An opaque pointer associated with the reader.
 *  deadline - Deadline to complete the operation.
 * Return:
 *  An initialized AsyncSocketIO intance.
 */
static AsyncSocketIO*
_async_socket_reader_new(AsyncSocket* as,
                         void* buffer,
                         uint32_t len,
                         on_as_io_cb io_cb,
                         void* reader_opaque,
                         Duration deadline)
{
    AsyncSocketIO* const asio = _async_socket_rw_new(as, 1, buffer, len, io_cb,
                                                     reader_opaque, deadline);
    return asio;
}

/* Creates new asynchronous socket writer.
 * Param:
 *  as - Asynchronous socket for the writer.
 *  buffer, len - Writer's buffer.
 *  io_cb - Writer's callback.
 *  writer_opaque - An opaque pointer associated with the writer.
 *  deadline - Deadline to complete the operation.
 * Return:
 *  An initialized AsyncSocketIO intance.
 */
static AsyncSocketIO*
_async_socket_writer_new(AsyncSocket* as,
                         const void* buffer,
                         uint32_t len,
                         on_as_io_cb io_cb,
                         void* writer_opaque,
                         Duration deadline)
{
    AsyncSocketIO* const asio = _async_socket_rw_new(as, 0, (void*)buffer, len,
                                                     io_cb, writer_opaque,
                                                     deadline);
    return asio;
}

/* I/O timed out. */
static void
_on_async_socket_io_timed_out(void* opaque)
{
    AsyncSocketIO* const asio = (AsyncSocketIO*)opaque;
    AsyncSocket* const as = asio->as;

    D("ASocket %s: %s I/O with deadline %lld has timed out at %lld",
      _async_socket_string(as), asio->is_io_read ? "READ" : "WRITE",
      asio->deadline, async_socket_deadline(as, 0));

    /* Reference while in callback. */
    async_socket_io_reference(asio);
    _async_socket_io_timed_out(asio->as, asio);
    async_socket_io_release(asio);
}

/********************************************************************************
 *                 Public Asynchronous Socket I/O API
 *******************************************************************************/

AsyncSocket*
async_socket_io_get_socket(const AsyncSocketIO* asio)
{
    async_socket_reference(asio->as);
    return asio->as;
}

void
async_socket_io_cancel_time_out(AsyncSocketIO* asio)
{
    loopTimer_stop(asio->timer);
}

void*
async_socket_io_get_io_opaque(const AsyncSocketIO* asio)
{
    return asio->io_opaque;
}

void*
async_socket_io_get_client_opaque(const AsyncSocketIO* asio)
{
    return async_socket_get_client_opaque(asio->as);
}

void*
async_socket_io_get_buffer_info(const AsyncSocketIO* asio,
                                uint32_t* transferred,
                                uint32_t* to_transfer)
{
    if (transferred != NULL) {
        *transferred = asio->transferred;
    }
    if (to_transfer != NULL) {
        *to_transfer = asio->to_transfer;
    }
    return asio->buffer;
}

void*
async_socket_io_get_buffer(const AsyncSocketIO* asio)
{
    return asio->buffer;
}

uint32_t
async_socket_io_get_transferred(const AsyncSocketIO* asio)
{
    return asio->transferred;
}

uint32_t
async_socket_io_get_to_transfer(const AsyncSocketIO* asio)
{
    return asio->to_transfer;
}

int
async_socket_io_is_read(const AsyncSocketIO* asio)
{
    return asio->is_io_read;
}

/********************************************************************************
 *                      Asynchronous Socket internals
 *******************************************************************************/

struct AsyncSocket {
    /* TCP address for the socket. */
    SockAddress         address;
    /* Connection callback for this socket. */
    on_as_connection_cb on_connection;
    /* An opaque pointer associated with this socket by the client. */
    void*               client_opaque;
    /* I/O looper for asynchronous I/O on the socket. */
    Looper*             looper;
    /* I/O descriptor for asynchronous I/O on the socket. */
    LoopIo              io[1];
    /* Timer to use for reconnection attempts. */
    LoopTimer           reconnect_timer[1];
    /* Head of the list of the active readers. */
    AsyncSocketIO*      readers_head;
    /* Tail of the list of the active readers. */
    AsyncSocketIO*      readers_tail;
    /* Head of the list of the active writers. */
    AsyncSocketIO*      writers_head;
    /* Tail of the list of the active writers. */
    AsyncSocketIO*      writers_tail;
    /* Socket's file descriptor. */
    int                 fd;
    /* Timeout to use for reconnection attempts. */
    int                 reconnect_to;
    /* Number of outstanding references to the socket. */
    int                 ref_count;
    /* Flags whether (1) or not (0) socket owns the looper. */
    int                 owns_looper;
};

static const char*
_async_socket_string(AsyncSocket* as)
{
    return sock_address_to_string(&as->address);
}

static Looper*
_async_socket_get_looper(AsyncSocket* as)
{
    return as->looper;
}

/* Pulls first reader out of the list.
 * Param:
 *  as - Initialized AsyncSocket instance.
 * Return:
 *  First I/O pulled out of the list, or NULL if there are no I/O in the list.
 *  Note that the caller is responsible for releasing the I/O object returned
 *  from this routine.
 */
static AsyncSocketIO*
_async_socket_pull_first_io(AsyncSocket* as,
                            AsyncSocketIO** list_head,
                            AsyncSocketIO** list_tail)
{
    AsyncSocketIO* const ret = *list_head;
    if (ret != NULL) {
        *list_head = ret->next;
        ret->next = NULL;
        if (*list_head == NULL) {
            *list_tail = NULL;
        }
    }
    return ret;
}

/* Pulls first reader out of the list.
 * Param:
 *  as - Initialized AsyncSocket instance.
 * Return:
 *  First reader pulled out of the list, or NULL if there are no readers in the
 *  list.
 *  Note that the caller is responsible for releasing the I/O object returned
 *  from this routine.
 */
static AsyncSocketIO*
_async_socket_pull_first_reader(AsyncSocket* as)
{
    return _async_socket_pull_first_io(as, &as->readers_head, &as->readers_tail);
}

/* Pulls first writer out of the list.
 * Param:
 *  as - Initialized AsyncSocket instance.
 * Return:
 *  First writer pulled out of the list, or NULL if there are no writers in the
 *  list.
 *  Note that the caller is responsible for releasing the I/O object returned
 *  from this routine.
 */
static AsyncSocketIO*
_async_socket_pull_first_writer(AsyncSocket* as)
{
    return _async_socket_pull_first_io(as, &as->writers_head, &as->writers_tail);
}

/* Removes an I/O descriptor from a list of active I/O.
 * Param:
 *  as - Initialized AsyncSocket instance.
 *  list_head, list_tail - Pointers to the list head and tail.
 *  io - I/O to remove.
 * Return:
 *  Boolean: 1 if I/O has been removed, or 0 if I/O has not been found in the list.
 */
static int
_async_socket_remove_io(AsyncSocket* as,
                        AsyncSocketIO** list_head,
                        AsyncSocketIO** list_tail,
                        AsyncSocketIO* io)
{
    AsyncSocketIO* prev = NULL;

    while (*list_head != NULL && io != *list_head) {
        prev = *list_head;
        list_head = &((*list_head)->next);
    }
    if (*list_head == NULL) {
        D("%s: I/O %p is not found in the list for socket '%s'",
          __FUNCTION__, io, _async_socket_string(as));
        return 0;
    }

    *list_head = io->next;
    if (prev != NULL) {
        prev->next = io->next;
    }
    if (*list_tail == io) {
        *list_tail = prev;
    }

    /* Release I/O adjusting reference added when I/O has been saved in the list. */
    async_socket_io_release(io);

    return 1;
}

/* Advances to the next I/O in the list.
 * Param:
 *  as - Initialized AsyncSocket instance.
 *  list_head, list_tail - Pointers to the list head and tail.
 */
static void
_async_socket_advance_io(AsyncSocket* as,
                         AsyncSocketIO** list_head,
                         AsyncSocketIO** list_tail)
{
    AsyncSocketIO* first_io = *list_head;
    if (first_io != NULL) {
        *list_head = first_io->next;
        first_io->next = NULL;
    }
    if (*list_head == NULL) {
        *list_tail = NULL;
    }
    if (first_io != NULL) {
        /* Release I/O removed from the head of the list. */
        async_socket_io_release(first_io);
    }
}

/* Advances to the next reader in the list.
 * Param:
 *  as - Initialized AsyncSocket instance.
 */
static void
_async_socket_advance_reader(AsyncSocket* as)
{
    _async_socket_advance_io(as, &as->readers_head, &as->readers_tail);
}

/* Advances to the next writer in the list.
 * Param:
 *  as - Initialized AsyncSocket instance.
 */
static void
_async_socket_advance_writer(AsyncSocket* as)
{
    _async_socket_advance_io(as, &as->writers_head, &as->writers_tail);
}

/* Completes an I/O.
 * Param:
 *  as - Initialized AsyncSocket instance.
 *  asio - I/O to complete.
 * Return:
 *  One of AsyncIOAction values.
 */
static AsyncIOAction
_async_socket_complete_io(AsyncSocket* as, AsyncSocketIO* asio)
{
    T("ASocket %s: %s I/O %p is completed.",
      _async_socket_string(as), asio->is_io_read ? "READ" : "WRITE", asio);

    /* Stop the timer. */
    async_socket_io_cancel_time_out(asio);

    return asio->on_io(asio->io_opaque, asio, ASIO_STATE_SUCCEEDED);
}

/* Timeouts an I/O.
 * Param:
 *  as - Initialized AsyncSocket instance.
 *  asio - An I/O that has timed out.
 * Return:
 *  One of AsyncIOAction values.
 */
static AsyncIOAction
_async_socket_io_timed_out(AsyncSocket* as, AsyncSocketIO* asio)
{
    T("ASocket %s: %s I/O %p with deadline %lld has timed out at %lld",
      _async_socket_string(as), asio->is_io_read ? "READ" : "WRITE", asio,
      asio->deadline, async_socket_deadline(as, 0));

    /* Report to the client. */
    const AsyncIOAction action = asio->on_io(asio->io_opaque, asio,
                                             ASIO_STATE_TIMED_OUT);

    /* Remove the I/O from a list of active I/O for actions other than retry. */
    if (action != ASIO_ACTION_RETRY) {
        if (asio->is_io_read) {
            _async_socket_remove_io(as, &as->readers_head, &as->readers_tail, asio);
        } else {
            _async_socket_remove_io(as, &as->writers_head, &as->writers_tail, asio);
        }
    }

    return action;
}

/* Cancels an I/O.
 * Param:
 *  as - Initialized AsyncSocket instance.
 *  asio - An I/O to cancel.
 * Return:
 *  One of AsyncIOAction values.
 */
static AsyncIOAction
_async_socket_cancel_io(AsyncSocket* as, AsyncSocketIO* asio)
{
    T("ASocket %s: %s I/O %p is cancelled.",
      _async_socket_string(as), asio->is_io_read ? "READ" : "WRITE", asio);

    /* Stop the timer. */
    async_socket_io_cancel_time_out(asio);

    return asio->on_io(asio->io_opaque, asio, ASIO_STATE_CANCELLED);
}

/* Reports an I/O failure.
 * Param:
 *  as - Initialized AsyncSocket instance.
 *  asio - An I/O that has failed. Can be NULL for general failures.
 *  failure - Failure (errno) that has occurred.
 * Return:
 *  One of AsyncIOAction values.
 */
static AsyncIOAction
_async_socket_io_failure(AsyncSocket* as, AsyncSocketIO* asio, int failure)
{
    T("ASocket %s: %s I/O %p has failed: %d -> %s",
      _async_socket_string(as), asio->is_io_read ? "READ" : "WRITE", asio,
      failure, strerror(failure));

    /* Stop the timer. */
    async_socket_io_cancel_time_out(asio);

    errno = failure;
    return asio->on_io(asio->io_opaque, asio, ASIO_STATE_FAILED);
}

/* Cancels all the active socket readers.
 * Param:
 *  as - Initialized AsyncSocket instance.
 */
static void
_async_socket_cancel_readers(AsyncSocket* as)
{
    while (as->readers_head != NULL) {
        AsyncSocketIO* const to_cancel = _async_socket_pull_first_reader(as);
        /* We ignore action returned from the cancellation callback, since we're
         * in a disconnected state here. */
        _async_socket_cancel_io(as, to_cancel);
        async_socket_io_release(to_cancel);
    }
}

/* Cancels all the active socket writers.
 * Param:
 *  as - Initialized AsyncSocket instance.
 */
static void
_async_socket_cancel_writers(AsyncSocket* as)
{
    while (as->writers_head != NULL) {
        AsyncSocketIO* const to_cancel = _async_socket_pull_first_writer(as);
        /* We ignore action returned from the cancellation callback, since we're
         * in a disconnected state here. */
        _async_socket_cancel_io(as, to_cancel);
        async_socket_io_release(to_cancel);
    }
}

/* Cancels all the I/O on the socket. */
static void
_async_socket_cancel_all_io(AsyncSocket* as)
{
    /* Stop the reconnection timer. */
    loopTimer_stop(as->reconnect_timer);

    /* Stop read / write on the socket. */
    loopIo_dontWantWrite(as->io);
    loopIo_dontWantRead(as->io);

    /* Cancel active readers and writers. */
    _async_socket_cancel_readers(as);
    _async_socket_cancel_writers(as);
}

/* Closes socket handle used by the async socket.
 * Param:
 *  as - Initialized AsyncSocket instance.
 */
static void
_async_socket_close_socket(AsyncSocket* as)
{
    if (as->fd >= 0) {
        T("ASocket %s: Socket handle %d is closed.",
          _async_socket_string(as), as->fd);
        loopIo_done(as->io);
        socket_close(as->fd);
        as->fd = -1;
    }
}

/* Destroys AsyncSocket instance.
 * Param:
 *  as - Initialized AsyncSocket instance.
 */
static void
_async_socket_free(AsyncSocket* as)
{
    if (as != NULL) {
        T("ASocket %s: Socket descriptor is destroyed.", _async_socket_string(as));

        /* Close socket. */
        _async_socket_close_socket(as);

        /* Free allocated resources. */
        if (as->looper != NULL) {
            loopTimer_done(as->reconnect_timer);
            if (as->owns_looper) {
                looper_free(as->looper);
            }
        }
        sock_address_done(&as->address);
        AFREE(as);
    }
}

/* Starts reconnection attempts after connection has been lost.
 * Param:
 *  as - Initialized AsyncSocket instance.
 *  to - Milliseconds to wait before reconnection attempt.
 */
static void
_async_socket_reconnect(AsyncSocket* as, int to)
{
    T("ASocket %s: reconnecting in %dms...", _async_socket_string(as), to);

    /* Make sure that no I/O is active, and socket is closed before we
     * reconnect. */
    _async_socket_cancel_all_io(as);

    /* Set the timer for reconnection attempt. */
    loopTimer_startRelative(as->reconnect_timer, to);
}

/********************************************************************************
 *                      Asynchronous Socket callbacks
 *******************************************************************************/

/* A callback that is invoked when socket gets disconnected.
 * Param:
 *  as - Initialized AsyncSocket instance.
 */
static void
_on_async_socket_disconnected(AsyncSocket* as)
{
    /* Save error to restore it for the client's callback. */
    const int save_errno = errno;
    AsyncIOAction action = ASIO_ACTION_ABORT;

    D("ASocket %s: Disconnected.", _async_socket_string(as));

    /* Cancel all the I/O on this socket. */
    _async_socket_cancel_all_io(as);

    /* Close the socket. */
    _async_socket_close_socket(as);

    /* Restore errno, and invoke client's callback. */
    errno = save_errno;
    action = as->on_connection(as->client_opaque, as, ASIO_STATE_FAILED);

    if (action == ASIO_ACTION_RETRY) {
        /* Client requested reconnection. */
        _async_socket_reconnect(as, as->reconnect_to);
    }
}

/* A callback that is invoked on socket's I/O failure.
 * Param:
 *  as - Initialized AsyncSocket instance.
 *  asio - Descriptor for the failed I/O. Can be NULL for general failures.
 */
static AsyncIOAction
_on_async_socket_failure(AsyncSocket* as, AsyncSocketIO* asio)
{
    D("ASocket %s: %s I/O failure: %d -> %s",
      _async_socket_string(as), asio->is_io_read ? "READ" : "WRITE",
      errno, strerror(errno));

    /* Report the failure. */
    return _async_socket_io_failure(as, asio, errno);
}

/* A callback that is invoked when there is data available to read.
 * Param:
 *  as - Initialized AsyncSocket instance.
 * Return:
 *  0 on success, or -1 on failure. Failure returned from this routine will
 *  skip writes (if awailable) behind this read.
 */
static int
_on_async_socket_recv(AsyncSocket* as)
{
    AsyncIOAction action;

    /* Get current reader. */
    AsyncSocketIO* const asr = as->readers_head;
    if (asr == NULL) {
        D("ASocket %s: No reader is available.", _async_socket_string(as));
        loopIo_dontWantRead(as->io);
        return 0;
    }

    /* Reference the reader while we're working with it in this callback. */
    async_socket_io_reference(asr);

    /* Bump I/O state, and inform the client that I/O is in progress. */
    if (asr->state == ASIO_STATE_QUEUED) {
        asr->state = ASIO_STATE_STARTED;
    } else {
        asr->state = ASIO_STATE_CONTINUES;
    }
    action = asr->on_io(asr->io_opaque, asr, asr->state);
    if (action == ASIO_ACTION_ABORT) {
        D("ASocket %s: Read is aborted by the client.", _async_socket_string(as));
        /* Move on to the next reader. */
        _async_socket_advance_reader(as);
        /* Lets see if there are still active readers, and enable, or disable
         * read I/O callback accordingly. */
        if (as->readers_head != NULL) {
            loopIo_wantRead(as->io);
        } else {
            loopIo_dontWantRead(as->io);
        }
        async_socket_io_release(asr);
        return 0;
    }

    /* Read next chunk of data. */
    int res = socket_recv(as->fd, asr->buffer + asr->transferred,
                          asr->to_transfer - asr->transferred);
    while (res < 0 && errno == EINTR) {
        res = socket_recv(as->fd, asr->buffer + asr->transferred,
                          asr->to_transfer - asr->transferred);
    }

    if (res == 0) {
        /* Socket has been disconnected. */
        errno = ECONNRESET;
        _on_async_socket_disconnected(as);
        async_socket_io_release(asr);
        return -1;
    }

    if (res < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            /* Yield to writes behind this read. */
            loopIo_wantRead(as->io);
            async_socket_io_release(asr);
            return 0;
        }

        /* An I/O error. */
        action = _on_async_socket_failure(as, asr);
        if (action != ASIO_ACTION_RETRY) {
            D("ASocket %s: Read is aborted on failure.", _async_socket_string(as));
            /* Move on to the next reader. */
            _async_socket_advance_reader(as);
            /* Lets see if there are still active readers, and enable, or disable
             * read I/O callback accordingly. */
            if (as->readers_head != NULL) {
                loopIo_wantRead(as->io);
            } else {
                loopIo_dontWantRead(as->io);
            }
        }
        async_socket_io_release(asr);
        return -1;
    }

    /* Update the reader's descriptor. */
    asr->transferred += res;
    if (asr->transferred == asr->to_transfer) {
        /* This read is completed. Move on to the next reader. */
        _async_socket_advance_reader(as);

        /* Notify reader completion. */
        _async_socket_complete_io(as, asr);
    }

    /* Lets see if there are still active readers, and enable, or disable read
     * I/O callback accordingly. */
    if (as->readers_head != NULL) {
        loopIo_wantRead(as->io);
    } else {
        loopIo_dontWantRead(as->io);
    }

    async_socket_io_release(asr);

    return 0;
}

/* A callback that is invoked when there is data available to write.
 * Param:
 *  as - Initialized AsyncSocket instance.
 * Return:
 *  0 on success, or -1 on failure. Failure returned from this routine will
 *  skip reads (if awailable) behind this write.
 */
static int
_on_async_socket_send(AsyncSocket* as)
{
    AsyncIOAction action;

    /* Get current writer. */
    AsyncSocketIO* const asw = as->writers_head;
    if (asw == NULL) {
        D("ASocket %s: No writer is available.", _async_socket_string(as));
        loopIo_dontWantWrite(as->io);
        return 0;
    }

    /* Reference the writer while we're working with it in this callback. */
    async_socket_io_reference(asw);

    /* Bump I/O state, and inform the client that I/O is in progress. */
    if (asw->state == ASIO_STATE_QUEUED) {
        asw->state = ASIO_STATE_STARTED;
    } else {
        asw->state = ASIO_STATE_CONTINUES;
    }
    action = asw->on_io(asw->io_opaque, asw, asw->state);
    if (action == ASIO_ACTION_ABORT) {
        D("ASocket %s: Write is aborted by the client.", _async_socket_string(as));
        /* Move on to the next writer. */
        _async_socket_advance_writer(as);
        /* Lets see if there are still active writers, and enable, or disable
         * write I/O callback accordingly. */
        if (as->writers_head != NULL) {
            loopIo_wantWrite(as->io);
        } else {
            loopIo_dontWantWrite(as->io);
        }
        async_socket_io_release(asw);
        return 0;
    }

    /* Write next chunk of data. */
    int res = socket_send(as->fd, asw->buffer + asw->transferred,
                          asw->to_transfer - asw->transferred);
    while (res < 0 && errno == EINTR) {
        res = socket_send(as->fd, asw->buffer + asw->transferred,
                          asw->to_transfer - asw->transferred);
    }

    if (res == 0) {
        /* Socket has been disconnected. */
        errno = ECONNRESET;
        _on_async_socket_disconnected(as);
        async_socket_io_release(asw);
        return -1;
    }

    if (res < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            /* Yield to reads behind this write. */
            loopIo_wantWrite(as->io);
            async_socket_io_release(asw);
            return 0;
        }

        /* An I/O error. */
        action = _on_async_socket_failure(as, asw);
        if (action != ASIO_ACTION_RETRY) {
            D("ASocket %s: Write is aborted on failure.", _async_socket_string(as));
            /* Move on to the next writer. */
            _async_socket_advance_writer(as);
            /* Lets see if there are still active writers, and enable, or disable
             * write I/O callback accordingly. */
            if (as->writers_head != NULL) {
                loopIo_wantWrite(as->io);
            } else {
                loopIo_dontWantWrite(as->io);
            }
        }
        async_socket_io_release(asw);
        return -1;
    }

    /* Update the writer descriptor. */
    asw->transferred += res;
    if (asw->transferred == asw->to_transfer) {
        /* This write is completed. Move on to the next writer. */
        _async_socket_advance_writer(as);

        /* Notify writer completion. */
        _async_socket_complete_io(as, asw);
    }

    /* Lets see if there are still active writers, and enable, or disable write
     * I/O callback accordingly. */
    if (as->writers_head != NULL) {
        loopIo_wantWrite(as->io);
    } else {
        loopIo_dontWantWrite(as->io);
    }

    async_socket_io_release(asw);

    return 0;
}

/* A callback that is invoked when an I/O is available on socket.
 * Param:
 *  as - Initialized AsyncSocket instance.
 *  fd - Socket's file descriptor.
 *  events - LOOP_IO_READ | LOOP_IO_WRITE bitmask.
 */
static void
_on_async_socket_io(void* opaque, int fd, unsigned events)
{
    AsyncSocket* const as = (AsyncSocket*)opaque;

    /* Reference the socket while we're working with it in this callback. */
    async_socket_reference(as);

    if ((events & LOOP_IO_READ) != 0) {
        if (_on_async_socket_recv(as) != 0) {
            async_socket_release(as);
            return;
        }
    }

    if ((events & LOOP_IO_WRITE) != 0) {
        if (_on_async_socket_send(as) != 0) {
            async_socket_release(as);
            return;
        }
    }

    async_socket_release(as);
}

/* A callback that is invoked by asynchronous socket connector on connection
 *  events.
 * Param:
 *  opaque - Initialized AsyncSocket instance.
 *  connector - Connector that is used to connect this socket.
 *  event - Connection event.
 * Return:
 *  One of AsyncIOAction values.
 */
static AsyncIOAction
_on_connector_events(void* opaque,
                     AsyncSocketConnector* connector,
                     AsyncIOState event)
{
    AsyncIOAction action;
    AsyncSocket* const as = (AsyncSocket*)opaque;

    /* Reference the socket while we're working with it in this callback. */
    async_socket_reference(as);

    if (event == ASIO_STATE_SUCCEEDED) {
        /* Accept the connection. */
        as->fd = async_socket_connector_pull_fd(connector);
        loopIo_init(as->io, as->looper, as->fd, _on_async_socket_io, as);
    }

    /* Invoke client's callback. */
    action = as->on_connection(as->client_opaque, as, event);
    if (event == ASIO_STATE_SUCCEEDED && action != ASIO_ACTION_DONE) {
        /* For whatever reason the client didn't want to keep this connection.
         * Close it. */
        D("ASocket %s: Connection is discarded by the client.",
          _async_socket_string(as));
        _async_socket_close_socket(as);
    }

    if (action != ASIO_ACTION_RETRY) {
        async_socket_connector_release(connector);
    }

    async_socket_release(as);

    return action;
}

/* Timer callback invoked to reconnect the lost connection.
 * Param:
 *  as - Initialized AsyncSocket instance.
 */
void
_on_async_socket_reconnect(void* opaque)
{
    AsyncSocket* as = (AsyncSocket*)opaque;

    /* Reference the socket while we're working with it in this callback. */
    async_socket_reference(as);
    async_socket_connect(as, as->reconnect_to);
    async_socket_release(as);
}


/********************************************************************************
 *                  Android Device Socket public API
 *******************************************************************************/

AsyncSocket*
async_socket_new(int port,
                 int reconnect_to,
                 on_as_connection_cb client_cb,
                 void* client_opaque,
                 Looper* looper)
{
    AsyncSocket* as;

    if (client_cb == NULL) {
        E("Invalid client_cb parameter");
        return NULL;
    }

    ANEW0(as);

    as->fd = -1;
    as->client_opaque = client_opaque;
    as->on_connection = client_cb;
    as->readers_head = as->readers_tail = NULL;
    as->reconnect_to = reconnect_to;
    as->ref_count = 1;
    sock_address_init_inet(&as->address, SOCK_ADDRESS_INET_LOOPBACK, port);
    if (looper == NULL) {
        as->looper = looper_newCore();
        if (as->looper == NULL) {
            E("Unable to create I/O looper for async socket '%s'",
              _async_socket_string(as));
            client_cb(client_opaque, as, ASIO_STATE_FAILED);
            _async_socket_free(as);
            return NULL;
        }
        as->owns_looper = 1;
    } else {
        as->looper = looper;
        as->owns_looper = 0;
    }

    loopTimer_init(as->reconnect_timer, as->looper, _on_async_socket_reconnect, as);

    T("ASocket %s: Descriptor is created.", _async_socket_string(as));

    return as;
}

int
async_socket_reference(AsyncSocket* as)
{
    assert(as->ref_count > 0);
    as->ref_count++;
    return as->ref_count;
}

int
async_socket_release(AsyncSocket* as)
{
    assert(as->ref_count > 0);
    as->ref_count--;
    if (as->ref_count == 0) {
        /* Last reference has been dropped. Destroy this object. */
        _async_socket_cancel_all_io(as);
        _async_socket_free(as);
        return 0;
    }
    return as->ref_count;
}

void
async_socket_connect(AsyncSocket* as, int retry_to)
{
    T("ASocket %s: Handling connection request for %dms...",
      _async_socket_string(as), retry_to);

    AsyncSocketConnector* const connector =
        async_socket_connector_new(&as->address, retry_to, _on_connector_events,
                                   as, as->looper);
    if (connector != NULL) {
        async_socket_connector_connect(connector);
    } else {
        as->on_connection(as->client_opaque, as, ASIO_STATE_FAILED);
    }
}

void
async_socket_disconnect(AsyncSocket* as)
{
    T("ASocket %s: Handling disconnection request...", _async_socket_string(as));

    if (as != NULL) {
        _async_socket_cancel_all_io(as);
        _async_socket_close_socket(as);
    }
}

void
async_socket_reconnect(AsyncSocket* as, int retry_to)
{
    T("ASocket %s: Handling reconnection request for %dms...",
      _async_socket_string(as), retry_to);

    _async_socket_cancel_all_io(as);
    _async_socket_close_socket(as);
    _async_socket_reconnect(as, retry_to);
}

void
async_socket_read_abs(AsyncSocket* as,
                      void* buffer, uint32_t len,
                      on_as_io_cb reader_cb,
                      void* reader_opaque,
                      Duration deadline)
{
    T("ASocket %s: Handling read for %d bytes with deadline %lld...",
      _async_socket_string(as), len, deadline);

    AsyncSocketIO* const asr =
        _async_socket_reader_new(as, buffer, len, reader_cb, reader_opaque,
                                 deadline);
    if (async_socket_is_connected(as)) {
        /* Add new reader to the list. Note that we use initial reference from I/O
         * 'new' routine as "in the list" reference counter. */
        if (as->readers_head == NULL) {
            as->readers_head = as->readers_tail = asr;
        } else {
            as->readers_tail->next = asr;
            as->readers_tail = asr;
        }
        loopIo_wantRead(as->io);
    } else {
        D("ASocket %s: Read on a disconnected socket.", _async_socket_string(as));
        errno = ECONNRESET;
        reader_cb(reader_opaque, asr, ASIO_STATE_FAILED);
        async_socket_io_release(asr);
    }
}

void
async_socket_read_rel(AsyncSocket* as,
                      void* buffer, uint32_t len,
                      on_as_io_cb reader_cb,
                      void* reader_opaque,
                      int to)
{
    const Duration dl = (to >= 0) ? looper_now(_async_socket_get_looper(as)) + to :
                                    DURATION_INFINITE;
    async_socket_read_abs(as, buffer, len, reader_cb, reader_opaque, dl);
}

void
async_socket_write_abs(AsyncSocket* as,
                       const void* buffer, uint32_t len,
                       on_as_io_cb writer_cb,
                       void* writer_opaque,
                       Duration deadline)
{
    T("ASocket %s: Handling write for %d bytes with deadline %lld...",
      _async_socket_string(as), len, deadline);

    AsyncSocketIO* const asw =
        _async_socket_writer_new(as, buffer, len, writer_cb, writer_opaque,
                                 deadline);
    if (async_socket_is_connected(as)) {
        /* Add new writer to the list. Note that we use initial reference from I/O
         * 'new' routine as "in the list" reference counter. */
        if (as->writers_head == NULL) {
            as->writers_head = as->writers_tail = asw;
        } else {
            as->writers_tail->next = asw;
            as->writers_tail = asw;
        }
        loopIo_wantWrite(as->io);
    } else {
        D("ASocket %s: Write on a disconnected socket.", _async_socket_string(as));
        errno = ECONNRESET;
        writer_cb(writer_opaque, asw, ASIO_STATE_FAILED);
        async_socket_io_release(asw);
    }
}

void
async_socket_write_rel(AsyncSocket* as,
                       const void* buffer, uint32_t len,
                       on_as_io_cb writer_cb,
                       void* writer_opaque,
                       int to)
{
    const Duration dl = (to >= 0) ? looper_now(_async_socket_get_looper(as)) + to :
                                    DURATION_INFINITE;
    async_socket_write_abs(as, buffer, len, writer_cb, writer_opaque, dl);
}

void*
async_socket_get_client_opaque(const AsyncSocket* as)
{
    return as->client_opaque;
}

Duration
async_socket_deadline(AsyncSocket* as, int rel)
{
    return (rel >= 0) ? looper_now(_async_socket_get_looper(as)) + rel :
                        DURATION_INFINITE;
}

int
async_socket_get_port(const AsyncSocket* as)
{
    return sock_address_get_port(&as->address);
}

int
async_socket_is_connected(const AsyncSocket* as)
{
    return as->fd >= 0;
}
