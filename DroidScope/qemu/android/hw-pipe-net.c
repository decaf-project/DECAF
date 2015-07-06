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

/* This file implements the 'tcp:' goldfish pipe type which allows
 * guest clients to directly connect to a TCP port through /dev/qemu_pipe.
 */

#include "sockets.h"
#include "android/utils/assert.h"
#include "android/utils/panic.h"
#include "android/utils/system.h"
#include "android/async-utils.h"
#include "android/opengles.h"
#include "android/looper.h"
#include "hw/goldfish_pipe.h"

/* Implement the OpenGL fast-pipe */

/* Set to 1 or 2 for debug traces */
// #define  DEBUG  1

#if DEBUG >= 1
#  define D(...)   printf(__VA_ARGS__), printf("\n")
#else
#  define D(...)   ((void)0)
#endif

#if DEBUG >= 2
#  define DD(...)                       printf(__VA_ARGS__), printf("\n")
#  define DDASSERT(cond)                _ANDROID_ASSERT(cond, "Assertion failure: ", #cond)
#  define DDASSERT_INT_OP(cond,val,op)  _ANDROID_ASSERT_INT_OP(cond,val,op)
#else
#  define DD(...)                       ((void)0)
#  define DDASSERT(cond)                ((void)0)
#  define DDASSERT_INT_OP(cond,val,op)  ((void)0)
#endif

#define DDASSERT_INT_LT(cond,val)  DDASSERT_INT_OP(cond,val,<)
#define DDASSERT_INT_LTE(cond,val)  DDASSERT_INT_OP(cond,val,<=)
#define DDASSERT_INT_GT(cond,val)  DDASSERT_INT_OP(cond,val,>)
#define DDASSERT_INT_GTE(cond,val)  DDASSERT_INT_OP(cond,val,>=)
#define DDASSERT_INT_EQ(cond,val)  DDASSERT_INT_OP(cond,val,==)
#define DDASSERT_INT_NEQ(cond,val)  DDASSERT_INT_OP(cond,val,!=)

enum {
    STATE_INIT,
    STATE_CONNECTING,
    STATE_CONNECTED,
    STATE_CLOSING_GUEST,
    STATE_CLOSING_SOCKET
};

typedef struct {
    void*           hwpipe;
    int             state;
    int             wakeWanted;
    LoopIo          io[1];
    AsyncConnector  connector[1];
} NetPipe;

static void
netPipe_free( NetPipe*  pipe )
{
    int  fd;

    /* Close the socket */
    fd = pipe->io->fd;
    loopIo_done(pipe->io);
    socket_close(fd);

    /* Release the pipe object */
    AFREE(pipe);
}


static void
netPipe_resetState( NetPipe* pipe )
{
    if ((pipe->wakeWanted & PIPE_WAKE_WRITE) != 0) {
        loopIo_wantWrite(pipe->io);
    } else {
        loopIo_dontWantWrite(pipe->io);
    }

   if (pipe->state == STATE_CONNECTED && (pipe->wakeWanted & PIPE_WAKE_READ) != 0) {
        loopIo_wantRead(pipe->io);
    } else {
        loopIo_dontWantRead(pipe->io);
    }
}


/* This function is only called when the socket is disconnected.
 * See netPipe_closeFromGuest() for the case when the guest requires
 * the disconnection. */
static void
netPipe_closeFromSocket( void* opaque )
{
    NetPipe*  pipe = opaque;

    D("%s", __FUNCTION__);

    /* If the guest already ordered the pipe to be closed, delete immediately */
    if (pipe->state == STATE_CLOSING_GUEST) {
        netPipe_free(pipe);
        return;
    }

    /* Force the closure of the QEMUD channel - if a guest is blocked
     * waiting for a wake signal, it will receive an error. */
    if (pipe->hwpipe != NULL) {
        goldfish_pipe_close(pipe->hwpipe);
        pipe->hwpipe = NULL;
    }

    pipe->state = STATE_CLOSING_SOCKET;
    netPipe_resetState(pipe);
}


/* This is the function that gets called each time there is an asynchronous
 * event on the network pipe.
 */
static void
netPipe_io_func( void* opaque, int fd, unsigned events )
{
    NetPipe*  pipe = opaque;
    int         wakeFlags = 0;

    /* Run the connector if we are in the CONNECTING state     */
    /* TODO: Add some sort of time-out, to deal with the case */
    /*        when the server is wedged.                      */
    if (pipe->state == STATE_CONNECTING) {
        AsyncStatus  status = asyncConnector_run(pipe->connector);
        if (status == ASYNC_NEED_MORE) {
            return;
        }
        else if (status == ASYNC_ERROR) {
            /* Could not connect, tell our client by closing the channel. */

            netPipe_closeFromSocket(pipe);
            return;
        }
        pipe->state = STATE_CONNECTED;
        netPipe_resetState(pipe);
        return;
    }

    /* Otherwise, accept incoming data */
    if ((events & LOOP_IO_READ) != 0) {
        if ((pipe->wakeWanted & PIPE_WAKE_READ) != 0) {
            wakeFlags |= PIPE_WAKE_READ;
        }
    }

    if ((events & LOOP_IO_WRITE) != 0) {
        if ((pipe->wakeWanted & PIPE_WAKE_WRITE) != 0) {
            wakeFlags |= PIPE_WAKE_WRITE;
        }
    }

    /* Send wake signal to the guest if needed */
    if (wakeFlags != 0) {
        goldfish_pipe_wake(pipe->hwpipe, wakeFlags);
        pipe->wakeWanted &= ~wakeFlags;
    }

    /* Reset state */
    netPipe_resetState(pipe);
}


void*
netPipe_initFromAddress( void* hwpipe, const SockAddress*  address, Looper* looper )
{
    NetPipe*     pipe;

    ANEW0(pipe);

    pipe->hwpipe = hwpipe;
    pipe->state  = STATE_INIT;

    {
        AsyncStatus  status;

        int  fd = socket_create( sock_address_get_family(address), SOCKET_STREAM );
        if (fd < 0) {
            D("%s: Could create socket from address family!", __FUNCTION__);
            netPipe_free(pipe);
            return NULL;
        }

        loopIo_init(pipe->io, looper, fd, netPipe_io_func, pipe);
        status = asyncConnector_init(pipe->connector, address, pipe->io);
        pipe->state = STATE_CONNECTING;

        if (status == ASYNC_ERROR) {
            D("%s: Could not connect to socket: %s",
              __FUNCTION__, errno_str);
            netPipe_free(pipe);
            return NULL;
        }
        if (status == ASYNC_COMPLETE) {
            pipe->state = STATE_CONNECTED;
            netPipe_resetState(pipe);
        }
    }

    return pipe;
}


/* Called when the guest wants to close the channel. This is different
 * from netPipe_closeFromSocket() which is called when the socket is
 * disconnected. */
static void
netPipe_closeFromGuest( void* opaque )
{
    NetPipe*  pipe = opaque;
    netPipe_free(pipe);
}

static int netPipeReadySend(NetPipe *pipe)
{
    if (pipe->state == STATE_CONNECTED)
        return 0;
    else if (pipe->state == STATE_CONNECTING)
        return PIPE_ERROR_AGAIN;
    else if (pipe->hwpipe == NULL)
        return PIPE_ERROR_INVAL;
    else
        return PIPE_ERROR_IO;
}

static int
netPipe_sendBuffers( void* opaque, const GoldfishPipeBuffer* buffers, int numBuffers )
{
    NetPipe*  pipe = opaque;
    int       count = 0;
    int       ret   = 0;
    int       buffStart = 0;
    const GoldfishPipeBuffer* buff = buffers;
    const GoldfishPipeBuffer* buffEnd = buff + numBuffers;

    ret = netPipeReadySend(pipe);
    if (ret != 0)
        return ret;

    for (; buff < buffEnd; buff++)
        count += buff->size;

    buff = buffers;
    while (count > 0) {
        int  avail = buff->size - buffStart;
        int  len = socket_send(pipe->io->fd, buff->data + buffStart, avail);

        /* the write succeeded */
        if (len > 0) {
            buffStart += len;
            if (buffStart >= buff->size) {
                buff++;
                buffStart = 0;
            }
            count -= len;
            ret   += len;
            continue;
        }

        /* we reached the end of stream? */
        if (len == 0) {
            if (ret == 0)
                ret = PIPE_ERROR_IO;
            break;
        }

        /* if we already wrote some stuff, simply return */
        if (ret > 0) {
            break;
        }

        /* need to return an appropriate error code */
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            ret = PIPE_ERROR_AGAIN;
        } else {
            ret = PIPE_ERROR_IO;
        }
        break;
    }

    return ret;
}

static int
netPipe_recvBuffers( void* opaque, GoldfishPipeBuffer*  buffers, int  numBuffers )
{
    NetPipe*  pipe = opaque;
    int       count = 0;
    int       ret   = 0;
    int       buffStart = 0;
    GoldfishPipeBuffer* buff = buffers;
    GoldfishPipeBuffer* buffEnd = buff + numBuffers;

    for (; buff < buffEnd; buff++)
        count += buff->size;

    buff = buffers;
    while (count > 0) {
        int  avail = buff->size - buffStart;
        int  len = socket_recv(pipe->io->fd, buff->data + buffStart, avail);

        /* the read succeeded */
        if (len > 0) {
            buffStart += len;
            if (buffStart >= buff->size) {
                buff++;
                buffStart = 0;
            }
            count -= len;
            ret   += len;
            continue;
        }

        /* we reached the end of stream? */
        if (len == 0) {
            if (ret == 0)
                ret = PIPE_ERROR_IO;
            break;
        }

        /* if we already read some stuff, simply return */
        if (ret > 0) {
            break;
        }

        /* need to return an appropriate error code */
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            ret = PIPE_ERROR_AGAIN;
        } else {
            ret = PIPE_ERROR_IO;
        }
        break;
    }
    return ret;
}

static unsigned
netPipe_poll( void* opaque )
{
    NetPipe*  pipe = opaque;
    unsigned  mask = loopIo_poll(pipe->io);
    unsigned  ret  = 0;

    if (mask & LOOP_IO_READ)
        ret |= PIPE_POLL_IN;
    if (mask & LOOP_IO_WRITE)
        ret |= PIPE_POLL_OUT;

    return ret;
}

static void
netPipe_wakeOn( void* opaque, int flags )
{
    NetPipe*  pipe = opaque;

    DD("%s: flags=%d", __FUNCTION__, flags);

    pipe->wakeWanted |= flags;
    netPipe_resetState(pipe);
}


void*
netPipe_initTcp( void* hwpipe, void* _looper, const char* args )
{
    /* Build SockAddress from arguments. Acceptable formats are:
     *   <port>
     */
    SockAddress  address;
    uint16_t     port;
    void*        ret;

    if (args == NULL) {
        D("%s: Missing address!", __FUNCTION__);
        return NULL;
    }
    D("%s: Port is '%s'", __FUNCTION__, args);

    /* Now, look at the port number */
    {
        char* end;
        long  val = strtol(args, &end, 10);
        if (end == NULL || *end != '\0' || val <= 0 || val > 65535) {
            D("%s: Invalid port number: '%s'", __FUNCTION__, args);
        }
        port = (uint16_t)val;
    }
    sock_address_init_inet(&address, SOCK_ADDRESS_INET_LOOPBACK, port);

    ret = netPipe_initFromAddress(hwpipe, &address, _looper);

    sock_address_done(&address);
    return ret;
}

#ifndef _WIN32
void*
netPipe_initUnix( void* hwpipe, void* _looper, const char* args )
{
    /* Build SockAddress from arguments. Acceptable formats are:
     *
     *   <path>
     */
    SockAddress  address;
    void*        ret;

    if (args == NULL || args[0] == '\0') {
        D("%s: Missing address!", __FUNCTION__);
        return NULL;
    }
    D("%s: Address is '%s'", __FUNCTION__, args);

    sock_address_init_unix(&address, args);

    ret = netPipe_initFromAddress(hwpipe, &address, _looper);

    sock_address_done(&address);
    return ret;
}
#endif

/**********************************************************************
 **********************************************************************
 *****
 *****  N E T W O R K   P I P E   M E S S A G E S
 *****
 *****/

static const GoldfishPipeFuncs  netPipeTcp_funcs = {
    netPipe_initTcp,
    netPipe_closeFromGuest,
    netPipe_sendBuffers,
    netPipe_recvBuffers,
    netPipe_poll,
    netPipe_wakeOn,
    NULL,  /* we can't save these */
    NULL,  /* we can't load these */
};

#ifndef _WIN32
static const GoldfishPipeFuncs  netPipeUnix_funcs = {
    netPipe_initUnix,
    netPipe_closeFromGuest,
    netPipe_sendBuffers,
    netPipe_recvBuffers,
    netPipe_poll,
    netPipe_wakeOn,
    NULL,  /* we can't save these */
    NULL,  /* we can't load these */
};
#endif

/* This is set to 1 in android_init_opengles() below, and tested
 * by openglesPipe_init() to refuse a pipe connection if the function
 * was never called.
 */
static int  _opengles_init;

static void*
openglesPipe_init( void* hwpipe, void* _looper, const char* args )
{
    char temp[32];
    NetPipe *pipe;

    if (!_opengles_init) {
        /* This should never happen, unless there is a bug in the
         * emulator's initialization, or the system image. */
        D("Trying to open the OpenGLES pipe without GPU emulation!");
        return NULL;
    }

    char server_addr[PATH_MAX];
    android_gles_server_path(server_addr, sizeof(server_addr));
#ifndef _WIN32
    if (android_gles_fast_pipes) {
        pipe = (NetPipe *)netPipe_initUnix(hwpipe, _looper, server_addr);
        D("Creating Unix OpenGLES pipe for GPU emulation: %s", server_addr);
    } else {
#else /* _WIN32 */
    {
#endif
        /* Connect through TCP as a fallback */
        pipe = (NetPipe *)netPipe_initTcp(hwpipe, _looper, server_addr);
        D("Creating TCP OpenGLES pipe for GPU emulation!");
    }
    if (pipe != NULL) {
        // Disable TCP nagle algorithm to improve throughput of small packets
        socket_set_nodelay(pipe->io->fd);

    // On Win32, adjust buffer sizes
#ifdef _WIN32
        {
            int sndbuf = 128 * 1024;
            int len = sizeof(sndbuf);
            if (setsockopt(pipe->io->fd, SOL_SOCKET, SO_SNDBUF,
                        (char*)&sndbuf, len) == SOCKET_ERROR) {
                D("Failed to set SO_SNDBUF to %d error=0x%x\n",
                sndbuf, WSAGetLastError());
            }
        }
#endif /* _WIN32 */
    }

    return pipe;
}

static const GoldfishPipeFuncs  openglesPipe_funcs = {
    openglesPipe_init,
    netPipe_closeFromGuest,
    netPipe_sendBuffers,
    netPipe_recvBuffers,
    netPipe_poll,
    netPipe_wakeOn,
    NULL,  /* we can't save these */
    NULL,  /* we can't load these */
};

void
android_net_pipes_init(void)
{
    Looper*  looper = looper_newCore();

    goldfish_pipe_add_type( "tcp", looper, &netPipeTcp_funcs );
#ifndef _WIN32
    goldfish_pipe_add_type( "unix", looper, &netPipeUnix_funcs );
#endif
    goldfish_pipe_add_type( "opengles", looper, &openglesPipe_funcs );
}

int
android_init_opengles_pipes(void)
{
    /* TODO: Check that we can load and initialize the host emulation
     *        libraries, and return -1 in case of error.
     */
    _opengles_init = 1;
    return 0;
}
