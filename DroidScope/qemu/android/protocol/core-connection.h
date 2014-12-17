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
 * This file contains declaration related to communication between emulator's
 * UI and core through a console port.
 */

#ifndef QEMU_ANDROID_CORE_CONNECTION_H
#define QEMU_ANDROID_CORE_CONNECTION_H

#include "android/sync-utils.h"

// Opaque CoreConnection structure.
typedef struct CoreConnection CoreConnection;

// Base console port
#define CORE_BASE_PORT          5554

// Maximum number of core porocesses running simultaneously on a machine.
#define MAX_CORE_PROCS          16

// Socket timeout in millisec (set to 5 seconds)
#define CORE_PORT_TIMEOUT_MS    5000

/* Opens core console socket.
 * Param:
 *  sockaddr Socket address to the core console.
 * Return:
 *  Sync socket descriptor on success, or -1 on failure, with errno appropriately
 *  set.
 */
SyncSocket* core_connection_open_socket(SockAddress* sockaddr);

/* Creates descriptor for a console client.
 * Param:
 *  console_socket Socket address for the console.
 * Return:
 *  Allocated and initialized descriptor for the client on success, or NULL
 *  on failure.
 */
CoreConnection* core_connection_create(SockAddress* console_socket);

/* Frees descriptor allocated with core_connection_create.
 * Param:
 *  desc Descriptor to free. Note that this routine will simply free the memory
 *      used by the descriptor.
 */
void core_connection_free(CoreConnection* desc);

/* Opens a socket handle to the console.
 * Param:
 *  desc Console client descriptor. Note that if the descriptor has been already
 *      opened, this routine will simply return with success.
 * Return:
 *  0 on success, or -1 on failure with errno properly set. This routine will
 *      return in at most one second.
 */
int core_connection_open(CoreConnection* desc);

/* Closes a socket handle to the console opened with core_connection_open.
 * Param:
 *  desc Console client descriptor opened with core_connection_open.
 */
void core_connection_close(CoreConnection* desc);

/* Synchronously writes to the console. See CORE_PORT_TIMEOUT_MS for the timeout
 * value used to wait for the write operation to complete.
 * Param:
 *  desc Console client descriptor opened with core_connection_open.
 *      buffer Buffer to write.
 *  to_write Number of bytes to write.
 *  written_bytes Upon success, contains number of bytes written. This parameter
 *      is optional, and can be NULL.
 * Return:
 *  0 on success, or -1 on failure.
 */
int core_connection_write(CoreConnection* desc,
                          const void* buffer,
                          size_t to_write,
                          size_t* written_bytes);

/* Synchronously reads from the console. See CORE_PORT_TIMEOUT_MS for the
 * timeout value used to wait for the read operation to complete.
 * Param:
 *  desc Console client descriptor opened with core_connection_open.
 *  buffer Buffer to read data to.
 *  to_read Number of bytes to read.
 *  read_bytes Upon success, contains number of bytes that have been actually
 *    read. This parameter is optional, and can be NULL.
 * Return:
 *  0 on success, or -1 on failure.
 */
int core_connection_read(CoreConnection* desc,
                         void* buffer,
                         size_t to_read,
                         size_t* read_bytes);

/* Switches opened console client to a given stream.
 * Param:
 *  desc Console client descriptor opened with core_connection_open. Note
 *      that this descriptor should represent console itself. In other words,
 *      there must have been no previous calls to this routine for that
 *      descriptor.
 *  stream_name Name of the stream to switch to.
 *  handshake Address of a string to allocate for a handshake message on
 *      success, or an error message on failure. If upon return from this
 *      routine that string is not NULL, its buffer must be freed with 'free'.
 * Return:
 *  0 on success, or -1 on failure.
 */
int core_connection_switch_stream(CoreConnection* desc,
                                  const char* stream_name,
                                  char** handshake);

/* Creates a console client, and switches it to a given stream.
 *  console_socket Socket address for the console.
 *  stream_name Name of the stream to switch to.
 *  handshake Address of a string to allocate for a handshake message on
 *      success, or an error message on failure. If upon return from this
 *      routine that string is not NULL, its buffer must be freed with 'free'.
 * Return:
 *  Allocated and initialized descriptor for the switched client on success, or
 *  NULL on failure.
 */
CoreConnection* core_connection_create_and_switch(SockAddress* console_socket,
                                                  const char* stream_name,
                                                  char** handshake);

/* Detaches opened console client from the console.
 * By console protocol, writing "\r\n" string to the console will destroy the
 * console client.
 * Param:
 *  desc Console client descriptor opened with core_connection_open.
 */
void core_connection_detach(CoreConnection* desc);

/* Gets socket descriptor associated with the core connection.
 * Param:
 *  desc Console client descriptor opened with core_connection_open.
 * Return
 *  Socket descriptor associated with the core connection.
 */
int core_connection_get_socket(CoreConnection* desc);

/* Calculates timeout for transferring the given number of bytes via core
 * connection.
 * Return:
 *  Number of milliseconds during which the entire number of bytes is expected
 *  to be transferred via core connection.
 */
static inline int
core_connection_get_timeout(size_t data_size)
{
    // Min 2 seconds + 10 millisec for each transferring byte.
    // TODO: Come up with a better arithmetics here.
    return 2000 + data_size * 10;
}

#endif  // QEMU_ANDROID_CORE_CONNECTION_H
