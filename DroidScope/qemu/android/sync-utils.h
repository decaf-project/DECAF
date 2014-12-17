/*
 * Copyright (C) 2010 The Android Open Source Project
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
 * Contains helper routines dealing with syncronous access to a non-blocking
 * sokets.
 */

#ifndef ANDROID_SYNC_UTILS_H
#define ANDROID_SYNC_UTILS_H

#include "android/android.h"
#include "sockets.h"

/* Descriptor for a connected non-blocking socket providing synchronous I/O */
typedef struct SyncSocket SyncSocket;

/*
 * Connect to a non-blocking socket for further synchronous I/O.
 * Note: this routine will explicitly call socket_set_nonblock on the fd passed
 * to this routine.
 * Param:
 *  fd - File descriptor for the socket, created with socket_create_xxx routine.
 *  sockaddr - Address of the socket to connect to.
 *  timeout - Time out (in milliseconds) to wait for the connection to occur.
 * Return:
 *  Initialized SyncSocket descriptor on success, or NULL on failure.
 */
SyncSocket* syncsocket_connect(int fd, SockAddress* sockaddr, int timeout);

/*
 * Initializes a non-blocking socket for further synchronous I/O.
 * Note: this routine will explicitly call socket_set_nonblock on the fd passed
 * to this routine.
 * Param:
 *  fd - File descriptor for the already connected socket.
 * Return:
 *  Initialized SyncSocket descriptor on success, or NULL on failure.
 */
SyncSocket* syncsocket_init(int fd);

/*
 * Closes SyncSocket descriptor obtained from syncsocket_connect routine.
 * Param:
 *  ssocket - SyncSocket descriptor obtained from syncsocket_connect routine.
 */
void syncsocket_close(SyncSocket* ssocket);

/*
 * Frees memory allocated for SyncSocket descriptor obtained from
 * syncsocket_connect routine. Note that this routine will also close socket
 * connection.
 * Param:
 *  ssocket - SyncSocket descriptor obtained from syncsocket_connect routine.
 */
void syncsocket_free(SyncSocket* ssocket);

/*
 * Prepares the socket for read.
 * Note: this routine must be called before calling into syncsocket_read_xxx
 * routines.
 * Param:
 *  ssocket - SyncSocket descriptor obtained from syncsocket_connect routine.
 * Return:
 *  0 on success, or -1 on failure.
 */
int syncsocket_start_read(SyncSocket* ssocket);

/*
 * Clears the socket after reading.
 * Note: this routine must be called after all expected data has been read from
 * the socket.
 * Param:
 *  ssocket - SyncSocket descriptor obtained from syncsocket_connect routine.
 * Return:
 *  0 on success, or -1 on failure.
 */
int syncsocket_stop_read(SyncSocket* ssocket);

/*
 * Prepares the socket for write.
 * Note: this routine must be called before calling into syncsocket_write_xxx
 * routines.
 * Param:
 *  ssocket - SyncSocket descriptor obtained from syncsocket_connect routine.
 * Return:
 *  0 on success, or -1 on failure.
 */
int syncsocket_start_write(SyncSocket* ssocket);

/*
 * Clears the socket after writing.
 * Note: this routine must be called after all data has been written to the
 * socket.
 * Param:
 *  ssocket - SyncSocket descriptor obtained from syncsocket_connect routine.
 * Return:
 *  0 on success, or -1 on failure.
 */
int syncsocket_stop_write(SyncSocket* ssocket);

/*
 * Synchronously reads from the socket.
 * Note: syncsocket_start_read must be called before first call to this routine.
 * Once syncsocket_start_read has been called, multiple syncsocket_read_xxx can
 * be called to read all necessary data from the socket. When all necessary data
 * has been read, syncsocket_stop_read must be called.
 * Param:
 *  ssocket - SyncSocket descriptor obtained from syncsocket_connect routine.
 *  buf - Buffer where to read data.
 *  size - Number of bytes to read.
 *  deadline - Absoulte deadline time to complete the reading.
 * Return:
 *  Number of bytes read on success, or -1 on failure.
 */
ssize_t syncsocket_read_absolute(SyncSocket* ssocket,
                                 void* buf,
                                 size_t size,
                                 int64_t deadline);

/*
 * Synchronously reads from the socket.
 * Note: syncsocket_start_read must be called before first call to this routine.
 * Once syncsocket_start_read has been called, multiple syncsocket_read_xxx can
 * be called to read all necessary data from the socket. When all necessary data
 * has been read, syncsocket_stop_read must be called.
 * Param:
 *  ssocket - SyncSocket descriptor obtained from syncsocket_connect routine.
 *  buf - Buffer where to read data.
 *  size - Number of bytes to read.
 *  timeout - Timeout (in milliseconds) to complete the reading.
 * Return:
 *  Number of bytes read on success, or -1 on failure.
 */
ssize_t syncsocket_read(SyncSocket* ssocket, void* buf, size_t size, int timeout);

/*
 * Synchronously writes to the socket.
 * Note: syncsocket_start_write must be called before first call to this routine.
 * Once syncsocket_start_write has been called, multiple syncsocket_write_xxx can
 * be called to write all necessary data to the socket. When all necessary data
 * has been written, syncsocket_stop_write must be called.
 * Param:
 *  ssocket - SyncSocket descriptor obtained from syncsocket_connect routine.
 *  buf - Buffer containing data to write.
 *  size - Number of bytes to write.
 *  deadline - Absoulte deadline time to complete the writing.
 * Return:
 *  Number of bytes written on success,or -1 on failure.
 */
ssize_t syncsocket_write_absolute(SyncSocket* ssocket,
                                  const void* buf,
                                  size_t size,
                                  int64_t deadline);

/*
 * Synchronously writes to the socket.
 * Note: syncsocket_start_write must be called before first call to this routine.
 * Once syncsocket_start_write has been called, multiple syncsocket_write_xxx can
 * be called to write all necessary data to the socket. When all necessary data
 * has been written, syncsocket_stop_write must be called.
 * Param:
 *  ssocket - SyncSocket descriptor obtained from syncsocket_connect routine.
 *  buf - Buffer containing data to write.
 *  size - Number of bytes to write.
 *  timeout - Timeout (in milliseconds) to complete the writing.
 * Return:
 *  Number of bytes written on success, or -1 on failure.
 */
ssize_t syncsocket_write(SyncSocket* ssocket,
                         const void* buf,
                         size_t size,
                         int timeout);

/*
 * Synchronously reads a line terminated with '\n' from the socket.
 * Note: syncsocket_start_read must be called before first call to this routine.
 * Param:
 *  ssocket - SyncSocket descriptor obtained from syncsocket_connect routine.
 *  buffer - Buffer where to read line.
 *  size - Number of characters the buffer can contain.
 *  deadline - Absoulte deadline time to complete the reading.
 * Return:
 *  Number of chracters read on success, 0 on deadline expiration,
 *  or -1 on failure.
 */
ssize_t syncsocket_read_line_absolute(SyncSocket* ssocket,
                                      char* buffer,
                                      size_t size,
                                      int64_t deadline);

/*
 * Synchronously reads a line terminated with '\n' from the socket.
 * Note: syncsocket_start_read must be called before first call to this routine.
 * Param:
 *  ssocket - SyncSocket descriptor obtained from syncsocket_connect routine.
 *  buffer - Buffer where to read line.
 *  size - Number of characters the buffer can contain.
 *  timeout - Timeout (in milliseconds) to complete the reading.
 * Return:
 *  Number of chracters read on success, 0 on deadline expiration,
 *  or -1 on failure.
 */
ssize_t syncsocket_read_line(SyncSocket* ssocket,
                             char* buffer,
                             size_t size,
                             int timeout);

/* Gets socket descriptor associated with the sync socket.
 * Param:
 *  ssocket - SyncSocket descriptor obtained from syncsocket_connect routine.
 * Return
 *  Socket descriptor associated with the sync socket.
 */
int syncsocket_get_socket(SyncSocket* ssocket);

/* Converts syncsocket_xxx operation status into success / failure result.
 * Param:
 *  status - syncsocket_xxx operation status to convert.
 * Return:
 *  0 if status passed to this routine indicated a success, or < 0 if status
 *  indicated a failure.
 */
static inline int
syncsocket_result(int status)
{
    if (status == 0) {
        // Status 0 returned from syncsocket_xxx means "disconnection", which is
        // a failure.
        status = -1;
    } else if (status > 0) {
        // Status > 0 means success.
        status = 0;
    }
    return status;
}

#endif  // ANDROID_SYNC_UTILS_H
