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

#ifndef ANDROID_ASYNC_SOCKET_H_
#define ANDROID_ASYNC_SOCKET_H_

#include "qemu-common.h"
#include "android/async-io-common.h"
#include "android/async-utils.h"

/*
 * Contains declaration of an API that encapsulates communication via an
 * asynchronous socket.
 *
 * This is pretty basic API that allows asynchronous connection to a socket,
 * and asynchronous read from / write to the connected socket.
 *
 * Since all the operations (including connection) are asynchronous, all the
 * operation results are reported back to the client of this API via set of
 * callbacks that client supplied to this API.
 *
 * Since it's hard to control lifespan of an object in asynchronous environment,
 * we make AsyncSocketConnector a referenced object, that will self-destruct when
 * its reference count drops to zero, indicating that the last client has
 * abandoned that object.
 */

/* Declares asynchronous socket descriptor. */
typedef struct AsyncSocket AsyncSocket;

/* Asynchronous socket I/O (reader, or writer) descriptor. */
typedef struct AsyncSocketIO AsyncSocketIO;

/* Defines client's callback set to monitor socket connection.
 * Param:
 *  client_opaque - An opaque pointer associated with the client.
 *  as - Initialized AsyncSocket instance.
 *  status - Socket connection status.
 * Return:
 *  One of the AsyncIOAction values.
 */
typedef AsyncIOAction (*on_as_connection_cb)(void* client_opaque,
                                             AsyncSocket* as,
                                             AsyncIOState status);

/* Defines client's callback set to monitor I/O progress.
 * Param:
 *  io_opaque - An opaque pointer associated with the I/O.
 *  asio - Async I/O in progress.
 *  status - Status of the I/O.
 * Return:
 *  One of the AsyncIOAction values.
 */
typedef AsyncIOAction (*on_as_io_cb)(void* io_opaque,
                                     AsyncSocketIO* asio,
                                     AsyncIOState status);

/********************************************************************************
 *                          AsyncSocketIO API
 *******************************************************************************/

/* References AsyncSocketIO object.
 * Param:
 *  asio - Initialized AsyncSocketIO instance.
 * Return:
 *  Number of outstanding references to the object.
 */
extern int async_socket_io_reference(AsyncSocketIO* asio);

/* Releases AsyncSocketIO object.
 * Note that upon exit from this routine the object might be destroyed, even if
 * the routine returns value other than zero.
 * Param:
 *  asio - Initialized AsyncSocketIO instance.
 * Return:
 *  Number of outstanding references to the object.
 */
extern int async_socket_io_release(AsyncSocketIO* asio);

/* Gets AsyncSocket instance for an I/O. Note that this routine will reference
 * AsyncSocket instance before returning it to the caller. */
extern AsyncSocket* async_socket_io_get_socket(const AsyncSocketIO* asio);

/* Cancels time out set for an I/O */
extern void async_socket_io_cancel_time_out(AsyncSocketIO* asio);

/* Gets an opaque pointer associated with an I/O */
extern void* async_socket_io_get_io_opaque(const AsyncSocketIO* asio);

/* Gets an opaque pointer associated with the client that has requested I/O */
extern void* async_socket_io_get_client_opaque(const AsyncSocketIO* asio);

/* Gets I/O buffer information.
 * Param:
 *  asio - I/O descriptor.
 *  transferred - Optional pointer to receive number of bytes transferred with
 *      this I/O. Can be NULL.
 *  to_transfer - Optional pointer to receive number of bytes requested to
 *      transfer with this I/O. Can be NULL.
 * Return:
 *  I/O buffer.
 */
extern void* async_socket_io_get_buffer_info(const AsyncSocketIO* asio,
                                             uint32_t* transferred,
                                             uint32_t* to_transfer);

/* Gets I/O buffer. */
extern void* async_socket_io_get_buffer(const AsyncSocketIO* asio);

/* Gets number of bytes transferred with this I/O. */
extern uint32_t async_socket_io_get_transferred(const AsyncSocketIO* asio);

/* Gets number of bytes requested to transfer with this I/O. */
extern uint32_t async_socket_io_get_to_transfer(const AsyncSocketIO* asio);

/* Gets I/O type: read (returns 1), or write (returns 0). */
extern int async_socket_io_is_read(const AsyncSocketIO* asio);

/********************************************************************************
 *                            AsyncSocket API
 *******************************************************************************/

/* Creates an asynchronous socket descriptor.
 * Param:
 *  port - TCP port to connect the socket to.
 *  reconnect_to - Timeout before trying to reconnect after disconnection.
 *  connect_cb - Client callback to monitor connection state (must not be NULL).
 *  client_opaque - An opaque pointer to associate with the socket client.
 *  looper - An optional (can be NULL) I/O looper to use for socket I/O. If
 *      this parameter is NULL, the socket will create its own looper.
 * Return:
 *  Initialized AsyncSocket instance on success, or NULL on failure.
 */
extern AsyncSocket* async_socket_new(int port,
                                     int reconnect_to,
                                     on_as_connection_cb connect_cb,
                                     void* client_opaque,
                                     Looper* looper);

/* References AsyncSocket object.
 * Param:
 *  as - Initialized AsyncSocket instance.
 * Return:
 *  Number of outstanding references to the object.
 */
extern int async_socket_reference(AsyncSocket* as);

/* Releases AsyncSocket object.
 * Note that upon exit from this routine the object might be destroyed, even if
 * the routine returns value other than zero.
 * Param:
 *  as - Initialized AsyncSocket instance.
 * Return:
 *  Number of outstanding references to the object.
 */
extern int async_socket_release(AsyncSocket* as);

/* Asynchronously connects to an asynchronous socket.
 * Note that connection result will be reported via callback passed to the
 * async_socket_new routine.
 * Param:
 *  as - Initialized AsyncSocket instance.
 *  retry_to - Number of milliseconds to wait before retrying a failed
 *      connection attempt.
 */
extern void async_socket_connect(AsyncSocket* as, int retry_to);

/* Disconnects from an asynchronous socket.
 * NOTE: AsyncSocket instance referenced in this call will be destroyed in this
 *  routine.
 * Param:
 *  as - Initialized and connected AsyncSocket instance.
 */
extern void async_socket_disconnect(AsyncSocket* as);

/* Asynchronously reconnects to an asynchronous socket.
 * Note that connection result will be reported via callback passed to the
 * async_socket_new routine.
 * Param:
 *  as - Initialized AsyncSocket instance.
 *  retry_to - Number of milliseconds to wait before trying to reconnect.
 */
extern void async_socket_reconnect(AsyncSocket* as, int retry_to);

/* Asynchronously reads data from an asynchronous socket with a deadline.
 * Param:
 *  as - Initialized and connected AsyncSocket instance.
 *  buffer, len - Buffer where to read data.
 *  reader_cb - Callback to monitor I/O progress (must not be NULL).
 *  reader_opaque - An opaque pointer associated with the reader.
 *  deadline - Deadline to complete the read.
 */
extern void async_socket_read_abs(AsyncSocket* as,
                                  void* buffer, uint32_t len,
                                  on_as_io_cb reader_cb,
                                  void* reader_opaque,
                                  Duration deadline);

/* Asynchronously reads data from an asynchronous socket with a relative timeout.
 * Param:
 *  as - Initialized and connected AsyncSocket instance.
 *  buffer, len - Buffer where to read data.
 *  reader_cb - Callback to monitor I/O progress (must not be NULL).
 *  reader_opaque - An opaque pointer associated with the reader.
 *  to - Milliseconds to complete the read. to < 0 indicates "no timeout"
 */
extern void async_socket_read_rel(AsyncSocket* as,
                                  void* buffer, uint32_t len,
                                  on_as_io_cb reader_cb,
                                  void* reader_opaque,
                                  int to);

/* Asynchronously writes data to an asynchronous socket with a deadline.
 * Param:
 *  as - Initialized and connected AsyncSocket instance.
 *  buffer, len - Buffer with writing data.
 *  writer_cb - Callback to monitor I/O progress (must not be NULL).
 *  writer_opaque - An opaque pointer associated with the writer.
 *  deadline - Deadline to complete the write.
 */
extern void async_socket_write_abs(AsyncSocket* as,
                                   const void* buffer, uint32_t len,
                                   on_as_io_cb writer_cb,
                                   void* writer_opaque,
                                   Duration deadline);

/* Asynchronously writes data to an asynchronous socket with a relative timeout.
 * Param:
 *  as - Initialized and connected AsyncSocket instance.
 *  buffer, len - Buffer with writing data.
 *  writer_cb - Callback to monitor I/O progress (must not be NULL)
 *  writer_opaque - An opaque pointer associated with the writer.
 *  to - Milliseconds to complete the write. to < 0 indicates "no timeout"
 */
extern void async_socket_write_rel(AsyncSocket* as,
                                   const void* buffer, uint32_t len,
                                   on_as_io_cb writer_cb,
                                   void* writer_opaque,
                                   int to);

/* Get a deadline for the given time interval relative to "now".
 * Param:
 *  as - Initialized AsyncSocket instance.
 *  rel - Time interval. If < 0 an infinite duration will be returned.
 * Return:
 *  A deadline for the given time interval relative to "now".
 */
extern Duration async_socket_deadline(AsyncSocket* as, int rel);

/* Gets an opaque pointer associated with the socket's client */
extern void* async_socket_get_client_opaque(const AsyncSocket* as);

/* Gets TCP port for the socket. */
extern int async_socket_get_port(const AsyncSocket* as);

/* Checks if socket is connected.
 * Return:
 *  Boolean: 1 - socket is connected, 0 - socket is not connected.
 */
extern int async_socket_is_connected(const AsyncSocket* as);

#endif  /* ANDROID_ASYNC_SOCKET_H_ */
