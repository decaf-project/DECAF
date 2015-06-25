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

#ifndef ANDROID_ASYNC_IO_COMMON_H_
#define ANDROID_ASYNC_IO_COMMON_H_

/*
 * Contains declarations common for asynchronous socket I/O
 */

/* Enumerates asynchronous I/O states.
 * Values from this enum are passed to callbacks associated with an I/O,
 * indicating at what state the I/O is. */
typedef enum AsyncIOState {
    /* Asynchronous I/O has been queued.                (0) */
    ASIO_STATE_QUEUED,
    /* Asynchronous I/O has started. This state indicates that I/O has been
     * performed for the first time.                    (1) */
    ASIO_STATE_STARTED,
    /* Asynchronous I/O is continuing. This state indicates that I/O has been
     * invoked for the second (or more) time.           (2) */
    ASIO_STATE_CONTINUES,
    /* Asynchronous I/O is about to be retried.         (3) */
    ASIO_STATE_RETRYING,
    /* Asynchronous I/O has been successfuly completed. (4) */
    ASIO_STATE_SUCCEEDED,
    /* Asynchronous I/O has failed.                     (5) */
    ASIO_STATE_FAILED,
    /* Asynchronous I/O has timed out.                  (6) */
    ASIO_STATE_TIMED_OUT,
    /* Asynchronous I/O has been cancelled (due to disconnect, for
     * instance).                                       (7) */
    ASIO_STATE_CANCELLED,
    /* Asynchronous I/O is finished and is about to be discarder. This state is
     * useful in case there is an association between an I/O and some client's
     * component, that holds a reference associated with this I/O. When callback
     * is invoked with this state, it means that it's safe to drop that extra
     * reference associated with the I/O                (8) */
    ASIO_STATE_FINISHED,
} AsyncIOState;

/* Enumerates actions to perform with an I/O on state transition.
 * Values from this enum are returned from async I/O callbacks, indicating what
 * action should be performed with the I/O by I/O handler. */
typedef enum AsyncIOAction {
    /* I/O is done. Perform default action depending on I/O type. */
    ASIO_ACTION_DONE,
    /* Abort the I/O. */
    ASIO_ACTION_ABORT,
    /* Retry the I/O. */
    ASIO_ACTION_RETRY,
} AsyncIOAction;

#endif  /* ANDROID_ASYNC_IO_COMMON_H_ */
