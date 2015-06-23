/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ANDROID_ASYNC_SOCKET_CONNECTOR_H_
#define ANDROID_ASYNC_SOCKET_CONNECTOR_H_

#include "qemu-common.h"
#include "android/async-io-common.h"
#include "android/async-utils.h"

/*
 * Contains declaration of an API that allows asynchronous connection to a
 * socket with retries.
 *
 * The typical usage of this API is as such:
 *
 * 1. The client creates an asynchronous connector instance by calling
 *    async_socket_connector_new routine, supplying there address of the socket
 *    to connect, and a callback to invoke on connection events.
 * 2. The client then proceeds with calling async_socket_connector_connect that
 *    would initiate connection attempts.
 *
 * The main job on the client side falls on the client's callback routine that
 * serves the connection events. Once connection has been initiated, the connector
 * will invoke that callback to report current connection status.
 *
 * In general, there are three connection events passed to the callback:
 * 1. Success.
 * 2. Failure.
 * 3. Retry.
 *
 * Typically, when client's callback is called for a successful connection, the
 * client will pull connected socket's FD from the connector, and then this FD
 * will be used by the client for I/O on the connected socket.
 *
 * When client's callback is invoked with an error (ASIO_STATE_FAILED event), the
 * client has an opportunity to review the error (available in 'errno'), and
 * either abort the connection by returning ASIO_ACTION_ABORT, or schedule a retry
 * by returning ASIO_ACTION_RETRY from the callback. If client returns ASIO_ACTION_ABORT
 * from the callback, the connector will stop connection attempts, and will
 * self-destruct. If ASIO_ACTION_RETRY is returned from the callback, the connector
 * will retry connection attempt after timeout that was set by the caller in the
 * call to async_socket_connector_new routine.
 *
 * When client's callback is invoked with ASIO_STATE_RETRYING (indicating that
 * connector is about to retry a connection attempt), the client has an opportunity
 * to cancel further connection attempts by returning ASIO_ACTION_ABORT, or it
 * can allow another connection attempt by returning ASIO_ACTION_RETRY.
 *
 * Since it's hard to control lifespan of an object in asynchronous environment,
 * we make AsyncSocketConnector a referenced object, that will self-destruct when
 * its reference count drops to zero, indicating that the last client has
 * abandoned that object.
 */

/* Declares async socket connector descriptor. */
typedef struct AsyncSocketConnector AsyncSocketConnector;

/* Declares callback that connector's client uses to monitor connection
 * status / progress.
 * Param:
 *  opaque - An opaque pointer associated with the client.
 *  connector - AsyncSocketConnector instance.
 *  event - Event that has occurred. If event is set to ASIO_STATE_FAILED,
 *      errno contains connection error.
 * Return:
 *  One of AsyncIOAction values.
 */
typedef AsyncIOAction (*asc_event_cb)(void* opaque,
                                      AsyncSocketConnector* connector,
                                      AsyncIOState event);

/* Creates and initializes AsyncSocketConnector instance.
 * Note that upon exit from this routine the reference count to the returned
 * object is set to 1.
 * Param:
 *  address - Initialized socket address to connect to.
 *  retry_to - Timeout to retry a failed connection attempt in milliseconds.
 *  cb, cb_opaque - Callback to invoke on connection events. This callback is
 *      required, and must not be NULL.
 *  looper - An optional (can be NULL) I/O looper to use for connection I/O. If
 *      this parameter is NULL, the connector will create its own looper.
 * Return:
 *  Initialized AsyncSocketConnector instance. Note that AsyncSocketConnector
 *  instance returned from this routine will be destroyed by the connector itself,
 *  when its work on connecting to the socket is completed. Typically, connector
 *  will destroy its descriptor after client's callback routine returns with a
 *  status other than ASIO_ACTION_RETRY.
 */
extern AsyncSocketConnector* async_socket_connector_new(const SockAddress* address,
                                                        int retry_to,
                                                        asc_event_cb cb,
                                                        void* cb_opaque,
                                                        Looper* looper);

/* References AsyncSocketConnector object.
 * Param:
 *  connector - Initialized AsyncSocketConnector instance.
 * Return:
 *  Number of outstanding references to the object.
 */
extern int async_socket_connector_reference(AsyncSocketConnector* connector);

/* Releases AsyncSocketConnector object.
 * Note that upon exit from this routine the object might be destroyed, even if
 * the routine returns value other than zero.
 * Param:
 *  connector - Initialized AsyncSocketConnector instance.
 * Return:
 *  Number of outstanding references to the object.
 */
extern int async_socket_connector_release(AsyncSocketConnector* connector);

/* Initiates asynchronous connection.
 * Note that connection result will be reported via callback set with the call to
 * async_socket_connector_new routine.
 * Param:
 *  connector - Initialized AsyncSocketConnector instance. Note that this
 *      connector descriptor might be destroyed asynchronously, before this
 *      routine returns.
 */
extern void async_socket_connector_connect(AsyncSocketConnector* connector);

/* Pulls socket's file descriptor from the connector.
 * This routine should be called from the connection callback on successful
 * connection status. This will provide the connector's client with an operational
 * socket FD, and at the same time this will tell the connector not to close the
 * FD when connector descriptor gets destroyed.
 * Param:
 *  connector - Initialized AsyncSocketConnector instance.
 * Return:
 *  File descriptor for the connected socket.
 */
extern int async_socket_connector_pull_fd(AsyncSocketConnector* connector);

#endif  /* ANDROID_ASYNC_SOCKET_CONNECTOR_H_ */
