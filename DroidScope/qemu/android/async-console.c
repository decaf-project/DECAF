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
#include "android/async-console.h"
#include <string.h>

/*
 * State diagram, ommitting the ERROR state
 *
 *  INITIAL -->--+
 *     |        |
 *     |     CONNECTING
 *     |       |
 *     |<-----+
 *     v
 *  READ_BANNER_1
 *     |
 *     v
 *  READ_BANNER_2
 *     |
 *     v
 *  COMPLETE
 */

enum {
    STATE_INITIAL,
    STATE_CONNECTING,
    STATE_ERROR,
    STATE_READ_BANNER_1,
    STATE_READ_BANNER_2,
    STATE_COMPLETE
};

/* A helper function to prepare the line reader and switch to a new state */
static AsyncStatus
_acc_prepareLineReader(AsyncConsoleConnector* acc, int newState)
{
    acc->state = newState;
    asyncLineReader_init(acc->lreader, acc->lbuff, sizeof(acc->lbuff), acc->io);
    return ASYNC_NEED_MORE;
}

AsyncStatus
asyncConsoleConnector_connect(AsyncConsoleConnector* acc,
                              const SockAddress*     address,
                              LoopIo*                io)
{
    acc->state = STATE_INITIAL;
    acc->address = address[0];
    acc->io = io;
    return asyncConsoleConnector_run(acc);
}


AsyncStatus
asyncConsoleConnector_run(AsyncConsoleConnector* acc)
{
    AsyncStatus  status = ASYNC_NEED_MORE;

    for (;;) {
        switch (acc->state)
        {
        case STATE_ERROR: /* reporting previous error */
            errno = acc->error;
            return ASYNC_ERROR;

        case STATE_INITIAL: /* initial connection attempt */
            acc->state = STATE_CONNECTING;
            status = asyncConnector_init(acc->connector, &acc->address, acc->io);
            if (status == ASYNC_ERROR)
                goto SET_ERROR;

            if (status == ASYNC_COMPLETE) { /* immediate connection */
                _acc_prepareLineReader(acc, STATE_READ_BANNER_1);
                continue;
            }
            break;

        case STATE_CONNECTING: /* still trying to connect */
            status = asyncConnector_run(acc->connector);
            if (status == ASYNC_ERROR)
                goto SET_ERROR;

            if (status == ASYNC_COMPLETE) {
                _acc_prepareLineReader(acc, STATE_READ_BANNER_1);
                continue;
            }
            break;

        case STATE_READ_BANNER_1: /* reading the first banner line */
            status = asyncLineReader_read(acc->lreader);
            if (status == ASYNC_ERROR)
                goto SET_ERROR;

            if (status == ASYNC_COMPLETE) {
                /* Check that first line starts with "Android Console:",
                 * otherwise we're not talking to the right program. */
                const char* line = asyncLineReader_getLine(acc->lreader);
                if (line == NULL || memcmp(line, "Android Console:", 16)) {
                    goto BAD_BANNER;
                }
                /* ok, fine, prepare for the next banner line then */
                _acc_prepareLineReader(acc, STATE_READ_BANNER_2);
                continue;
            }
            break;

        case STATE_READ_BANNER_2: /* reading the second banner line */
            status = asyncLineReader_read(acc->lreader);
            if (status == ASYNC_ERROR)
                goto SET_ERROR;

            if (status == ASYNC_COMPLETE) {
                const char* line = asyncLineReader_getLine(acc->lreader);
                if (line == NULL) {
                    goto BAD_BANNER;
                }
                /* ok, we're done !*/
                acc->state = STATE_COMPLETE;
                return ASYNC_COMPLETE;
            }
            break;

        case STATE_COMPLETE:
            status = ASYNC_COMPLETE;
        }
        return status;
    }
BAD_BANNER:
    errno = ENOPROTOOPT;
SET_ERROR:
    acc->state = STATE_ERROR;
    acc->error = errno;
    return ASYNC_ERROR;
}
