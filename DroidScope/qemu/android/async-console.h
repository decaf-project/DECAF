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
#ifndef ANDROID_ASYNC_CONSOLE_H
#define ANDROID_ASYNC_CONSOLE_H

#include "android/async-utils.h"

/* An AsyncConsoleConnector allows you to asynchronously connect to an
 * Android console port.
 */
typedef struct {
    int              state;
    int              error;
    LoopIo*          io;
    SockAddress      address;
    AsyncConnector   connector[1];
    AsyncLineReader  lreader[1];
    uint8_t          lbuff[128];
} AsyncConsoleConnector;

/* Initialize the console connector. This attempts to connect to the address
 * provided through 'io'. Use asyncConsoleConnect_run() after that.
 */
AsyncStatus
asyncConsoleConnector_connect(AsyncConsoleConnector* acc,
                              const SockAddress*     address,
                              LoopIo*                io);

/* Asynchronous console connection management. Returns:
 *
 * ASYNC_COMPLETE:
 *    Connection was complete, and the console banner was properly read/eaten.
 *    you can now send/write commands through the console with 'io'.
 *
 * ASYNC_ERROR:
 *    An error occured, either during the connection itself, or when
 *    reading the content. This sets errno to ENOPROTOOPT if the connector
 *    detects that you're not connected to a proper Android emulator console
 *    port (i.e. if the banner was incorrect). Other errors are possible
 *    (e.g. in case of early connection termination).
 *
 * ASYNC_NEED_MORE:
 *     Not enough data was exchanged, call this function later.
 */
AsyncStatus
asyncConsoleConnector_run(AsyncConsoleConnector* acc);


#endif /* ANDROID_ASYNC_CONSOLE_H */
