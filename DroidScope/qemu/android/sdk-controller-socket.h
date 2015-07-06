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

#ifndef ANDROID_SDKCONTROL_SOCKET_H_
#define ANDROID_SDKCONTROL_SOCKET_H_

#include "android/async-socket.h"
#include "android/async-utils.h"

/*
 * Contains declaration of an API that encapsulates communication protocol with
 * SdkController running on an Android device.
 *
 * SdkController is used to provide:
 *
 * - Realistic sensor emulation.
 * - Multi-touch emulation.
 * - Open for other types of emulation.
 *
 * The idea behind this type of emulation is such that there is an actual
 * Android device that is connected via USB to the host running the emulator.
 * On the device there is an SdkController service running that enables
 * communication between an Android application that gathers information required
 * by the emulator, and transmits that info to the emulator.
 *
 * SdkController service on the device, and SDKCtlSocket API implemented here
 * implement the exchange protocol between an Android application, and emulation
 * engine running inside the emulator.
 *
 * In turn, the exchange protocol is implemented on top of asynchronous socket
 * communication (abstracted in AsyncSocket protocol implemented in
 * android/async-socket.*). It means that connection, and all data transfer
 * (both, in, and out) are completely asynchronous, and results of each operation
 * are reported through callbacks.
 *
 * Essentially, this entire API implements two types of protocols:
 *
 * - Connection protocol.
 * - Data exchange protocol.
 *
 * 1. Connection protocol.
 *
 * Connecting to SdkController service on the attached device can be broken down
 * into two phases:
 * - Connecting to a TCP socket.
 * - Sending a "handshake" query to the SdkController.
 *
 * 1.1. Connecting to the socket.
 *
 * TCP socket connection with SdkController is enabled by using adb port
 * forwarding. SdkController is always listening to a local abstract socket named
 * 'android.sdk.controller', so to enable connecting to it from the host, run
 *
 *   adb forward tcp:<port> localabstract: android.sdk.controller
 *
 * After that TCP socket for the requested port can be used to connect to
 * SdkController, and connecting to it is no different than connecting to any
 * socket server. Except for one thing: adb port forwarding is implemented in
 * such a way, that socket_connect will always succeed, even if there is no
 * server listening to that port on the other side of connection. Moreover,
 * even socked_send will succeed in this case, so the only way to ensure that
 * SdkController in deed is listening is to exchange a handshake with it:
 * Fortunatelly, an attempt to read from forwarded TCP port on condition that
 * there is no listener on the oher side will fail.
 *
 * 1.2. Handshake query.
 *
 * Handshake query is a special type of query that SDKCtlSocket sends to the
 * SdkController upon successful socket connection. This query served two
 * purposes:
 * - Informs the SdkController about host endianness. This information is
 *   important, because SdkController needs it in order to format its messages
 *   with proper endianness.
 * - Ensures that SdkController is in deed listening on the other side of the
 *   connected socket.
 *
 * Connection with SdkController is considered to be successfuly established when
 * SdkController responds to the handshake query, thus, completing the connection.
 *
 * 2. Data exchange protocol.
 *
 * As it was mentioned above, all data transfer in this API is completely
 * asynchronous, and result of each data transfer is reported via callbacks.
 * This also complicates destruction of data involved in exchange, since in an
 * asynchronous environment it's hard to control the lifespan of an object, its
 * owner, and who and when is responsible to free resources allocated for the
 * transfer. To address this issue, all the descriptors that this API operates
 * with are referenced on use / released after use, and get freed when reference
 * counter for them drops to zero, indicating that there is no component that is
 * interested in that particular descriptor.
 *
 * There are three types of data in the exchange protocol:
 * - A message - the simplest type of data that doesn't require any replies.
 * - A query - A message that require a reply, and
 * - A query reply - A message that delivers query reply.
 */

/* Default TCP port to use for connection with SDK controller. */
#define SDKCTL_DEFAULT_TCP_PORT     1970

/* Declares SDK controller socket descriptor. */
typedef struct SDKCtlSocket SDKCtlSocket;

/* Declares SDK controller message descriptor. */
typedef struct SDKCtlMessage SDKCtlMessage;

/* Declares SDK controller query descriptor. */
typedef struct SDKCtlQuery SDKCtlQuery;

/* Declares SDK controller direct packet descriptor.
 * Direct packet (unlike message, or query packets) doesn't contain data buffer,
 * but rather references message, or query data allocated by the client.
 */
typedef struct SDKCtlDirectPacket SDKCtlDirectPacket;

/* Defines client's callback set to monitor SDK controller socket connection.
 *
 * SDKCtlSocket will invoke this callback when connection to TCP port is
 * established, but before handshake query is processed. The client should use
 * on_sdkctl_handshake_cb to receive notification about an operational connection
 * with SdkController.
 *
 * The main purpose of this callback for the client is to monitor connection
 * state: in addition to TCP port connection, this callback will be invoked when
 * connection with the port is lost.
 *
 * Param:
 *  client_opaque - An opaque pointer associated with the client.
 *  sdkctl - Initialized SDKCtlSocket instance.
 *  status - Socket connection status.  Can be one of these:
 *    - ASIO_STATE_SUCCEEDED : Socket is connected to the port.
 *    - ASIO_STATE_FAILED    : Connection attempt has failed, or connection with
 *                             the port is lost.
 * Return:
 *  One of the AsyncIOAction values.
 */
typedef AsyncIOAction (*on_sdkctl_socket_connection_cb)(void* client_opaque,
                                                        SDKCtlSocket* sdkctl,
                                                        AsyncIOState status);

/* Enumerates port connection statuses passed to port connection callback.
 */
typedef enum SdkCtlPortStatus {
    /* Service-side port has connected to the socket. */
    SDKCTL_PORT_DISCONNECTED,
    /* Service-side port has disconnected from the socket. */
    SDKCTL_PORT_CONNECTED,
    /* Service-side port has enabled emulation */
    SDKCTL_PORT_ENABLED,
    /* Service-side port has disabled emulation */
    SDKCTL_PORT_DISABLED,
    /* Handshake request has succeeded, and service-side port is connected. */
    SDKCTL_HANDSHAKE_CONNECTED,
    /* Handshake request has succeeded, but service-side port is not connected. */
    SDKCTL_HANDSHAKE_NO_PORT,
    /* Handshake request has failed due to port duplication. */
    SDKCTL_HANDSHAKE_DUP,
    /* Handshake request has failed on an unknown query. */
    SDKCTL_HANDSHAKE_UNKNOWN_QUERY,
    /* Handshake request has failed on an unknown response. */
    SDKCTL_HANDSHAKE_UNKNOWN_RESPONSE,
} SdkCtlPortStatus;

/* Defines client's callback set to receive port connection status.
 *
 * Port connection is different than socket connection, and indicates whether
 * or not a service-side port that provides requested emulation functionality is
 * hooked up with the connected socket. For instance, multi-touch port may be
 * inactive at the time when socket is connected. So, there is a successful
 * socket connection, but there is no service at the device end that provides
 * multi-touch functionality. So, for multi-touch example, this callback will be
 * invoked when multi-touch port at the device end becomes active, and hooks up
 * with the socket that was connected before.
 *
 * Param:
 *  client_opaque - An opaque pointer associated with the client.
 *  sdkctl - Initialized SDKCtlSocket instance.
 *  status - Port connection status.
 */
typedef void (*on_sdkctl_port_connection_cb)(void* client_opaque,
                                             SDKCtlSocket* sdkctl,
                                             SdkCtlPortStatus status);

/* Defines a message notification callback.
 * Param:
 *  client_opaque - An opaque pointer associated with the client.
 *  sdkctl - Initialized SDKCtlSocket instance.
 *  message - Descriptor for received message. Note that message descriptor will
 *      be released upon exit from this callback (thus, could be freed along
 *      with message data). If the client is interested in working with that
 *      message after the callback returns, it should reference the message
 *      descriptor in this callback.
 *  msg_type - Message type.
 *  msg_data, msg_size - Message data.
 */
typedef void (*on_sdkctl_message_cb)(void* client_opaque,
                                     SDKCtlSocket* sdkctl,
                                     SDKCtlMessage* message,
                                     int msg_type,
                                     void* msg_data,
                                     int msg_size);

/* Defines query completion callback.
 * Param:
 *  query_opaque - An opaque pointer associated with the query by the client.
 *  query - Query descriptor.  Note that query descriptor will be released upon
 *      exit from this callback (thus, could be freed along with query data). If
 *      the client is interested in working with that query after the callback
 *      returns, it should reference the query descriptor in this callback.
 *  status - Query status. Can be one of these:
 *    - ASIO_STATE_CONTINUES : Query data has been transmitted to the service,
 *                             and query is now waiting for response.
 *    - ASIO_STATE_SUCCEEDED : Query has been successfully completed.
 *    - ASIO_STATE_FAILED    : Query has failed on an I/O.
 *    - ASIO_STATE_TIMED_OUT : Deadline set for the query has expired.
 *    - ASIO_STATE_CANCELLED : Query has been cancelled due to socket
 *                             disconnection.
 * Return:
 *  One of the AsyncIOAction values.
 */
typedef AsyncIOAction (*on_sdkctl_query_cb)(void* query_opaque,
                                            SDKCtlQuery* query,
                                            AsyncIOState status);

/* Defines direct packet completion callback.
 * Param:
 *  opaque - An opaque pointer associated with the direct packet by the client.
 *  packet - Packet descriptor.  Note that packet descriptor will be released
 *      upon exit from this callback (thus, could be freed). If the client is
 *      interested in working with that packet after the callback returns, it
 *      should reference the packet descriptor in this callback.
 *  status - Packet status. Can be one of these:
 *    - ASIO_STATE_SUCCEEDED : Packet has been successfully sent.
 *    - ASIO_STATE_FAILED    : Packet has failed on an I/O.
 *    - ASIO_STATE_CANCELLED : Packet has been cancelled due to socket
 *                             disconnection.
 * Return:
 *  One of the AsyncIOAction values.
 */
typedef AsyncIOAction (*on_sdkctl_direct_cb)(void* opaque,
                                             SDKCtlDirectPacket* packet,
                                             AsyncIOState status);

/********************************************************************************
 *                        SDKCtlDirectPacket API
 ********************************************************************************/

/* Creates new SDKCtlDirectPacket descriptor.
 * Param:
 *  sdkctl - Initialized SDKCtlSocket instance to create a direct packet for.
 * Return:
 *  Referenced SDKCtlDirectPacket instance.
 */
extern SDKCtlDirectPacket* sdkctl_direct_packet_new(SDKCtlSocket* sdkctl);

/* References SDKCtlDirectPacket object.
 * Param:
 *  packet - Initialized SDKCtlDirectPacket instance.
 * Return:
 *  Number of outstanding references to the object.
 */
extern int sdkctl_direct_packet_reference(SDKCtlDirectPacket* packet);

/* Releases SDKCtlDirectPacket object.
 * Note that upon exiting from this routine the object might be destroyed, even
 * if this routine returns value other than zero.
 * Param:
 *  packet - Initialized SDKCtlDirectPacket instance.
 * Return:
 *  Number of outstanding references to the object.
 */
extern int sdkctl_direct_packet_release(SDKCtlDirectPacket* packet);

/* Sends direct packet.
 * Param:
 *  packet - Packet descriptor for the direct packet to send.
 *  data - Data to send with the packet. Must be fully initialized message, or
 *      query header.
 *  cb, cb_opaque - Callback to invoke on packet transmission events.
 */
extern void sdkctl_direct_packet_send(SDKCtlDirectPacket* packet,
                                      void* data,
                                      on_sdkctl_direct_cb cb,
                                      void* cb_opaque);

/********************************************************************************
 *                         SDKCtlMessage API
 ********************************************************************************/

/* References SDKCtlMessage object.
 * Param:
 *  msg - Initialized SDKCtlMessage instance.
 * Return:
 *  Number of outstanding references to the object.
 */
extern int sdkctl_message_reference(SDKCtlMessage* msg);

/* Releases SDKCtlMessage object.
 * Note that upon exiting from this routine the object might be destroyed, even
 * if this routine returns value other than zero.
 * Param:
 *  msg - Initialized SDKCtlMessage instance.
 * Return:
 *  Number of outstanding references to the object.
 */
extern int sdkctl_message_release(SDKCtlMessage* msg);

/* Builds and sends a message to the device.
 * Param:
 *  sdkctl - SDKCtlSocket instance for the message.
 *  msg_type - Defines message type.
 *  data - Message data. Can be NULL if there is no data associated with the
 *      message.
 *  size - Byte size of the data buffer.
 * Return:
 *  Referenced SDKCtlQuery descriptor.
 */
extern SDKCtlMessage* sdkctl_message_send(SDKCtlSocket* sdkctl,
                                          int msg_type,
                                          const void* data,
                                          uint32_t size);

/* Gets message header size */
extern int sdkctl_message_get_header_size(void);

/* Initializes message header.
 * Param:
 *  msg - Beginning of the message packet.
 *  msg_type - Message type.
 *  msg_size - Message data size.
 */
extern void sdkctl_init_message_header(void* msg, int msg_type, int msg_size);

/********************************************************************************
 *                          SDKCtlQuery API
 ********************************************************************************/

/* Creates, and partially initializes query descriptor.
 * Note that returned descriptor is referenced, and it must be eventually
 * released with a call to sdkctl_query_release.
 * Param:
 *  sdkctl - SDKCtlSocket instance for the query.
 *  query_type - Defines query type.
 *  in_data_size Size of the query's input buffer (data to be sent with this
 *      query). Note that buffer for query data will be allocated along with the
 *      query descriptor. Use sdkctl_query_get_buffer_in to get address of data
 *      buffer for this query.
 * Return:
 *  Referenced SDKCtlQuery descriptor.
 */
extern SDKCtlQuery* sdkctl_query_new(SDKCtlSocket* sdkctl,
                                     int query_type,
                                     uint32_t in_data_size);

/* Creates, and fully initializes query descriptor.
 * Note that returned descriptor is referenced, and it must be eventually
 * released with a call to sdkctl_query_release.
 * Param:
 *  sdkctl - SDKCtlSocket instance for the query.
 *  query_type - Defines query type.
 *  in_data_size Size of the query's input buffer (data to be sent with this
 *      query). Note that buffer for query data will be allocated along with the
 *      query descriptor. Use sdkctl_query_get_buffer_in to get address of data
 *      buffer for this query.
 *  in_data - Data to initialize query's input buffer with.
 *  response_buffer - Contains address of the buffer addressing preallocated
 *      response buffer on the way in, and address of the buffer containing query
 *      response on query completion. If this parameter is NULL, the API will
 *      allocate its own query response buffer, which is going to be freed after
 *      query completion.
 *  response_size - Contains size of preallocated response buffer on the way in,
 *      and size of the received response on query completion. Can be NULL.
 *  query_cb - A callback to monitor query state.
 *  query_opaque - An opaque pointer associated with the query.
 * Return:
 *  Referenced SDKCtlQuery descriptor.
 */
extern SDKCtlQuery* sdkctl_query_new_ex(SDKCtlSocket* sdkctl,
                                        int query_type,
                                        uint32_t in_data_size,
                                        const void* in_data,
                                        void** response_buffer,
                                        uint32_t* response_size,
                                        on_sdkctl_query_cb query_cb,
                                        void* query_opaque);

/* Sends query to SDK controller.
 * Param:
 *  query - Query to send. Note that this must be a fully initialized query
 *      descriptor.
 *  to - Milliseconds to allow for the query to complete. Negative value means
 *  "forever".
 */
extern void sdkctl_query_send(SDKCtlQuery* query, int to);

/* Creates, fully initializes query descriptor, and sends the query to SDK
 * controller.
 * Note that returned descriptor is referenced, and it must be eventually
 * released with a call to sdkctl_query_release.
 * Param:
 *  sdkctl - SDKCtlSocket instance for the query.
 *  query_type - Defines query type.
 *  in_data_size Size of the query's input buffer (data to be sent with this
 *      query). Note that buffer for query data will be allocated along with the
 *      query descriptor. Use sdkctl_query_get_buffer_in to get address of data
 *      buffer for this query.
 *  in_data - Data to initialize query's input buffer with.
 *  response_buffer - Contains address of the buffer addressing preallocated
 *      response buffer on the way in, and address of the buffer containing query
 *      response on query completion. If this parameter is NULL, the API will
 *      allocate its own query response buffer, which is going to be freed after
 *      query completion.
 *  response_size - Contains size of preallocated response buffer on the way in,
 *      and size of the received response on query completion. Can be NULL.
 *  query_cb - A callback to monitor query state.
 *  query_opaque - An opaque pointer associated with the query.
 *  to - Milliseconds to allow for the query to complete. Negative value means
 *  "forever".
 * Return:
 *  Referenced SDKCtlQuery descriptor for the query that has been sent.
 */
extern SDKCtlQuery* sdkctl_query_build_and_send(SDKCtlSocket* sdkctl,
                                                int query_type,
                                                uint32_t in_data_size,
                                                const void* in_data,
                                                void** response_buffer,
                                                uint32_t* response_size,
                                                on_sdkctl_query_cb query_cb,
                                                void* query_opaque,
                                                int to);

/* References SDKCtlQuery object.
 * Param:
 *  query - Initialized SDKCtlQuery instance.
 * Return:
 *  Number of outstanding references to the object.
 */
extern int sdkctl_query_reference(SDKCtlQuery* query);

/* Releases SDKCtlQuery object.
 * Note that upon exit from this routine the object might be destroyed, even if
 * this routine returns value other than zero.
 * Param:
 *  query - Initialized SDKCtlQuery instance.
 * Return:
 *  Number of outstanding references to the object.
 */
extern int sdkctl_query_release(SDKCtlQuery* query);

/* Gets address of query's input data buffer (data to be send).
 * Param:
 *  query - Query to get data buffer for.
 * Return:
 *  Address of query's input data buffer.
 */
extern void* sdkctl_query_get_buffer_in(SDKCtlQuery* query);

/* Gets address of query's output data buffer (response data).
 * Param:
 *  query - Query to get data buffer for.
 * Return:
 *  Address of query's output data buffer.
 */
extern void* sdkctl_query_get_buffer_out(SDKCtlQuery* query);

/********************************************************************************
 *                          SDKCtlSocket API
 ********************************************************************************/

/* Creates an SDK controller socket descriptor.
 * Param:
 *  reconnect_to - Timeout before trying to reconnect, or retry connection
 *      attempts after disconnection, or on connection failures.
 *  service_name - Name of the SdkController service for this socket (such as
 *      'sensors', 'milti-touch', etc.)
 *  on_socket_connection - A callback to invoke on socket connection events.
 *  on_port_connection - A callback to invoke on port connection events.
 *  on_message - A callback to invoke when a message is received from the SDK
 *      controller.
 *  opaque - An opaque pointer to associate with the socket.
 * Return:
 *  Initialized SDKCtlSocket instance on success, or NULL on failure.
 */
extern SDKCtlSocket* sdkctl_socket_new(int reconnect_to,
                                       const char* service_name,
                                       on_sdkctl_socket_connection_cb on_socket_connection,
                                       on_sdkctl_port_connection_cb on_port_connection,
                                       on_sdkctl_message_cb on_message,
                                       void* opaque);

/* Improves throughput by recycling memory allocated for buffers transferred via
 * this API.
 *
 * In many cases data exchanged between SDK controller sides are small, and,
 * although may come quite often, are coming in a sequential manner. For instance,
 * sensor service on the device may send us sensor updates every 20ms, one after
 * another. For such data traffic it makes perfect sense to recycle memory
 * allocated for the previous sensor update, rather than to free it just to
 * reallocate same buffer in 20ms. This routine sets properties of the recycler
 * for the given SDK controller connection. Recycling includes memory allocated
 * for all types of data transferred in this API: packets, and queries.
 * Param:
 *  sdkctl - Initialized SDKCtlSocket instance.
 *  data_size - Size of buffer to allocate for each data block.
 *  max_recycled_num - Maximum number of allocated buffers to keep in the
 *      recycler.
 */
extern void sdkctl_init_recycler(SDKCtlSocket* sdkctl,
                                 uint32_t data_size,
                                 int max_recycled_num);

/* References SDKCtlSocket object.
 * Param:
 *  sdkctl - Initialized SDKCtlSocket instance.
 * Return:
 *  Number of outstanding references to the object.
 */
extern int sdkctl_socket_reference(SDKCtlSocket* sdkctl);

/* Releases SDKCtlSocket object.
 * Note that upon exit from this routine the object might be destroyed, even if
 * this routine returns value other than zero.
 * Param:
 *  sdkctl - Initialized SDKCtlSocket instance.
 * Return:
 *  Number of outstanding references to the object.
 */
extern int sdkctl_socket_release(SDKCtlSocket* sdkctl);

/* Asynchronously connects to SDK controller.
 * Param:
 *  sdkctl - Initialized SDKCtlSocket instance.
 *  port - TCP port to connect the socket to.
 *  retry_to - Number of milliseconds to wait before retrying a failed
 *      connection attempt.
 */
extern void sdkctl_socket_connect(SDKCtlSocket* sdkctl, int port, int retry_to);

/* Asynchronously reconnects to SDK controller.
 * Param:
 *  sdkctl - Initialized SDKCtlSocket instance.
 *  port - TCP port to reconnect the socket to.
 *  retry_to - Number of milliseconds to wait before reconnecting. Same timeout
 *      will be used for retrying a failed connection attempt.
 */
extern void sdkctl_socket_reconnect(SDKCtlSocket* sdkctl, int port, int retry_to);

/* Disconnects from SDK controller.
 * Param:
 *  sdkctl - Initialized SDKCtlSocket instance.
 */
extern void sdkctl_socket_disconnect(SDKCtlSocket* sdkctl);

/* Checks if SDK controller socket is connected.
 * Param:
 *  sdkctl - Initialized SDKCtlSocket instance.
 * Return:
 *  Boolean: 1 if socket is connected, 0 if socket is not connected.
 */
extern int sdkctl_socket_is_connected(SDKCtlSocket* sdkctl);

/* Checks if SDK controller port is ready for emulation.
 * Param:
 *  sdkctl - Initialized SDKCtlSocket instance.
 * Return:
 *  Boolean: 1 if port is ready, 0 if port is not ready.
 */
extern int sdkctl_socket_is_port_ready(SDKCtlSocket* sdkctl);

/* Gets status of the SDK controller port for this socket.
 * Param:
 *  sdkctl - Initialized SDKCtlSocket instance.
 * Return:
 *  Status of the SDK controller port for this socket.
 */
extern SdkCtlPortStatus sdkctl_socket_get_port_status(SDKCtlSocket* sdkctl);

/* Checks if handshake was successful.
 * Param:
 *  sdkctl - Initialized SDKCtlSocket instance.
 * Return:
 *  Boolean: 1 if handshake was successful, 0 if handshake was not successful.
 */
extern int sdkctl_socket_is_handshake_ok(SDKCtlSocket* sdkctl);

#endif  /* ANDROID_SDKCONTROL_SOCKET_H_ */
