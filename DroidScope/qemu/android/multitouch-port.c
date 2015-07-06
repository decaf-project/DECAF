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

#include "qemu-common.h"
#include "utils/panic.h"
#include "android/hw-events.h"
#include "android/charmap.h"
#include "android/multitouch-screen.h"
#include "android/sdk-controller-socket.h"
#include "android/multitouch-port.h"
#include "android/globals.h"  /* for android_hw */
#include "android/opengles.h"
#include "android/utils/misc.h"
#include "android/utils/jpeg-compress.h"
#include "android/utils/debug.h"

#define  E(...)    derror(__VA_ARGS__)
#define  W(...)    dwarning(__VA_ARGS__)
#define  D(...)    VERBOSE_PRINT(mtport,__VA_ARGS__)
#define  D_ACTIVE  VERBOSE_CHECK(mtport)

#define TRACE_ON    1

#if TRACE_ON
#define  T(...)    VERBOSE_PRINT(mtport,__VA_ARGS__)
#else
#define  T(...)
#endif

/* Timeout (millisec) to use when communicating with SDK controller. */
#define SDKCTL_MT_TIMEOUT      3000

/*
 * Message types used in multi-touch emulation.
 */

/* Pointer move message. */
#define SDKCTL_MT_MOVE                  1
/* First pointer down message. */
#define SDKCTL_MT_FISRT_DOWN            2
/* Last pointer up message. */
#define SDKCTL_MT_LAST_UP               3
/* Pointer down message. */
#define SDKCTL_MT_POINTER_DOWN          4
/* Pointer up message. */
#define SDKCTL_MT_POINTER_UP            5
/* Sends framebuffer update. */
#define SDKCTL_MT_FB_UPDATE             6
/* Framebuffer update has been received. */
#define SDKCTL_MT_FB_UPDATE_RECEIVED    7
/* Framebuffer update has been handled. */
#define SDKCTL_MT_FB_UPDATE_HANDLED     8

/* Multi-touch port descriptor. */
struct AndroidMTSPort {
    /* Caller identifier. */
    void*               opaque;
    /* Communication socket. */
    SDKCtlSocket*       sdkctl;
    /* Initialized JPEG compressor instance. */
    AJPEGDesc*          jpeg_compressor;
    /* Direct packet descriptor for framebuffer updates. */
    SDKCtlDirectPacket* fb_packet;
};

/* Data sent with SDKCTL_MT_QUERY_START */
typedef struct QueryDispData {
    /* Width of the emulator display. */
    int     width;
    /* Height of the emulator display. */
    int     height;
} QueryDispData;

/* Multi-touch event structure received from SDK controller port. */
typedef struct AndroidMTEvent {
    /* Pointer identifier. */
    int     pid;
    /* Pointer 'x' coordinate. */
    int     x;
    /* Pointer 'y' coordinate. */
    int     y;
    /* Pointer pressure. */
    int     pressure;
} AndroidMTEvent;

/* Multi-touch pointer descriptor received from SDK controller port. */
typedef struct AndroidMTPtr {
    /* Pointer identifier. */
    int     pid;
} AndroidMTPtr;

/* Destroys and frees the descriptor. */
static void
_mts_port_free(AndroidMTSPort* mtsp)
{
    if (mtsp != NULL) {
        if (mtsp->fb_packet != NULL) {
            sdkctl_direct_packet_release(mtsp->fb_packet);
        }
        if (mtsp->jpeg_compressor != NULL) {
            jpeg_compressor_destroy(mtsp->jpeg_compressor);
        }
        if (mtsp->sdkctl != NULL) {
            sdkctl_socket_release(mtsp->sdkctl);
        }
        AFREE(mtsp);
    }
}

/********************************************************************************
 *                          Multi-touch action handlers
 *******************************************************************************/

/*
 * Although there are a lot of similarities in the way the handlers below are
 * implemented, for the sake of tracing / debugging it's better to have a
 * separate handler for each distinctive action.
 */

/* First pointer down event handler. */
static void
_on_action_down(int tracking_id, int x, int y, int pressure)
{
    multitouch_update_pointer(MTES_DEVICE, tracking_id, x, y, pressure);
}

/* Last pointer up event handler. */
static void
_on_action_up(int tracking_id)
{
    multitouch_update_pointer(MTES_DEVICE, tracking_id, 0, 0, 0);
}

/* Pointer down event handler. */
static void
_on_action_pointer_down(int tracking_id, int x, int y, int pressure)
{
    multitouch_update_pointer(MTES_DEVICE, tracking_id, x, y, pressure);
}

/* Pointer up event handler. */
static void
_on_action_pointer_up(int tracking_id)
{
    multitouch_update_pointer(MTES_DEVICE, tracking_id, 0, 0, 0);
}

/* Pointer move event handler. */
static void
_on_action_move(int tracking_id, int x, int y, int pressure)
{
    multitouch_update_pointer(MTES_DEVICE, tracking_id, x, y, pressure);
}

/********************************************************************************
 *                          Multi-touch event handlers
 *******************************************************************************/

/* Handles "pointer move" event.
 * Param:
 *  param - Array of moving pointers.
 *  pointers_count - Number of pointers in the array.
 */
static void
_on_move(const AndroidMTEvent* param, int pointers_count)
{
    int n;
    for (n = 0; n < pointers_count; n++, param++) {
        T("Multi-touch: MOVE(%d): %d-> %d:%d:%d",
          n, param->pid, param->x, param->y, param->pressure);
         _on_action_move(param->pid, param->x, param->y, param->pressure);
    }
}

/* Handles "first pointer down" event. */
static void
_on_down(const AndroidMTEvent* param)
{
    T("Multi-touch: 1-ST DOWN: %d-> %d:%d:%d",
      param->pid, param->x, param->y, param->pressure);
    _on_action_down(param->pid, param->x, param->y, param->pressure);
}

/* Handles "last pointer up" event. */
static void
_on_up(const AndroidMTPtr* param)
{
    T("Multi-touch: LAST UP: %d", param->pid);
    _on_action_up(param->pid);
}

/* Handles "next pointer down" event. */
static void
_on_pdown(const AndroidMTEvent* param)
{
    T("Multi-touch: DOWN: %d-> %d:%d:%d",
      param->pid, param->x, param->y, param->pressure);
    _on_action_pointer_down(param->pid, param->x, param->y, param->pressure);
}

/* Handles "next pointer up" event. */
static void
_on_pup(const AndroidMTPtr* param)
{
    T("Multi-touch: UP: %d", param->pid);
    _on_action_pointer_up(param->pid);
}

/********************************************************************************
 *                      Device communication callbacks
 *******************************************************************************/

/* A callback that is invoked on SDK controller socket connection events. */
static AsyncIOAction
_on_multitouch_socket_connection(void* opaque,
                                 SDKCtlSocket* sdkctl,
                                 AsyncIOState status)
{
    if (status == ASIO_STATE_FAILED) {
        /* Reconnect (after timeout delay) on failures */
        if (sdkctl_socket_is_handshake_ok(sdkctl)) {
            sdkctl_socket_reconnect(sdkctl, SDKCTL_DEFAULT_TCP_PORT,
                                    SDKCTL_MT_TIMEOUT);
        }
    }
    return ASIO_ACTION_DONE;
}

/* A callback that is invoked on SDK controller port connection events. */
static void
_on_multitouch_port_connection(void* opaque,
                               SDKCtlSocket* sdkctl,
                               SdkCtlPortStatus status)
{
    switch (status) {
        case SDKCTL_PORT_CONNECTED:
            D("Multi-touch: SDK Controller is connected");
            break;

        case SDKCTL_PORT_DISCONNECTED:
            D("Multi-touch: SDK Controller is disconnected");
            // Disable OpenGLES framebuffer updates.
            if (android_hw->hw_gpu_enabled) {
                android_setPostCallback(NULL, NULL);
            }
            break;

        case SDKCTL_PORT_ENABLED:
            D("Multi-touch: SDK Controller port is enabled.");
            // Enable OpenGLES framebuffer updates.
            if (android_hw->hw_gpu_enabled) {
                android_setPostCallback(multitouch_opengles_fb_update, NULL);
            }
            /* Refresh (possibly stale) device screen. */
            multitouch_refresh_screen();
            break;

        case SDKCTL_PORT_DISABLED:
            D("Multi-touch: SDK Controller port is disabled.");
            // Disable OpenGLES framebuffer updates.
            if (android_hw->hw_gpu_enabled) {
                android_setPostCallback(NULL, NULL);
            }
            break;

        case SDKCTL_HANDSHAKE_CONNECTED:
            D("Multi-touch: Handshake succeeded with connected port.");
            break;

        case SDKCTL_HANDSHAKE_NO_PORT:
            D("Multi-touch: Handshake succeeded with disconnected port.");
            break;

        case SDKCTL_HANDSHAKE_DUP:
            W("Multi-touch: Handshake failed due to port duplication.");
            sdkctl_socket_disconnect(sdkctl);
            break;

        case SDKCTL_HANDSHAKE_UNKNOWN_QUERY:
            W("Multi-touch: Handshake failed due to unknown query.");
            sdkctl_socket_disconnect(sdkctl);
            break;

        case SDKCTL_HANDSHAKE_UNKNOWN_RESPONSE:
        default:
            W("Multi-touch: Handshake failed due to unknown reason.");
            sdkctl_socket_disconnect(sdkctl);
            break;
    }
}

/* A callback that is invoked when a message is received from the device. */
static void
_on_multitouch_message(void* client_opaque,
                       SDKCtlSocket* sdkctl,
                       SDKCtlMessage* message,
                       int msg_type,
                       void* msg_data,
                       int msg_size)
{
    switch (msg_type) {
        case SDKCTL_MT_MOVE: {
            assert((msg_size / sizeof(AndroidMTEvent)) && !(msg_size % sizeof(AndroidMTEvent)));
            _on_move((const AndroidMTEvent*)msg_data, msg_size / sizeof(AndroidMTEvent));
            break;
        }

        case SDKCTL_MT_FISRT_DOWN:
            assert(msg_size / sizeof(AndroidMTEvent) && !(msg_size % sizeof(AndroidMTEvent)));
            _on_down((const AndroidMTEvent*)msg_data);
            break;

        case SDKCTL_MT_LAST_UP:
            _on_up((const AndroidMTPtr*)msg_data);
            break;

        case SDKCTL_MT_POINTER_DOWN:
            assert(msg_size / sizeof(AndroidMTEvent) && !(msg_size % sizeof(AndroidMTEvent)));
            _on_pdown((const AndroidMTEvent*)msg_data);
            break;

        case SDKCTL_MT_POINTER_UP:
            _on_pup((const AndroidMTPtr*)msg_data);
            break;

        case SDKCTL_MT_FB_UPDATE_RECEIVED:
            D("Framebuffer update ACK.");
            break;

        case SDKCTL_MT_FB_UPDATE_HANDLED:
            D("Framebuffer update handled.");
            multitouch_fb_updated();
            break;

        default:
            W("Multi-touch: Unknown message %d", msg_type);
            break;
    }
}

/********************************************************************************
 *                          MTS port API
 *******************************************************************************/

AndroidMTSPort*
mts_port_create(void* opaque)
{
    AndroidMTSPort* mtsp;

    ANEW0(mtsp);
    mtsp->opaque                = opaque;

    /* Initialize default MTS descriptor. */
    multitouch_init(mtsp);

    /* Create JPEG compressor. Put message header + MTFrameHeader in front of the
     * compressed data. this way we will have entire query ready to be
     * transmitted to the device. */
    mtsp->jpeg_compressor =
        jpeg_compressor_create(sdkctl_message_get_header_size() + sizeof(MTFrameHeader), 4096);

    mtsp->sdkctl = sdkctl_socket_new(SDKCTL_MT_TIMEOUT, "multi-touch",
                                     _on_multitouch_socket_connection,
                                     _on_multitouch_port_connection,
                                     _on_multitouch_message, mtsp);
    sdkctl_init_recycler(mtsp->sdkctl, 64, 8);

    /* Create a direct packet that will wrap up framebuffer updates. Note that
     * we need to do this after we have initialized the recycler! */
    mtsp->fb_packet = sdkctl_direct_packet_new(mtsp->sdkctl);

    /* Now we can initiate connection witm MT port on the device. */
    sdkctl_socket_connect(mtsp->sdkctl, SDKCTL_DEFAULT_TCP_PORT,
                          SDKCTL_MT_TIMEOUT);

    return mtsp;
}

void
mts_port_destroy(AndroidMTSPort* mtsp)
{
    _mts_port_free(mtsp);
}

/********************************************************************************
 *                       Handling framebuffer updates
 *******************************************************************************/

/* Compresses a framebuffer region into JPEG image.
 * Param:
 *  mtsp - Multi-touch port descriptor with initialized JPEG compressor.
 *  fmt Descriptor for framebuffer region to compress.
 *  fb Beginning of the framebuffer.
 *  jpeg_quality JPEG compression quality. A number from 1 to 100. Note that
 *      value 10 provides pretty decent image for the purpose of multi-touch
 *      emulation.
 */
static void
_fb_compress(const AndroidMTSPort* mtsp,
             const MTFrameHeader* fmt,
             const uint8_t* fb,
             int jpeg_quality,
             int ydir)
{
    T("Multi-touch: compressing %d bytes frame buffer", fmt->w * fmt->h * fmt->bpp);

    jpeg_compressor_compress_fb(mtsp->jpeg_compressor, fmt->x, fmt->y, fmt->w,
                                fmt->h, fmt->disp_height, fmt->bpp, fmt->bpl,
                                fb, jpeg_quality, ydir);
}

int
mts_port_send_frame(AndroidMTSPort* mtsp,
                    MTFrameHeader* fmt,
                    const uint8_t* fb,
                    on_sdkctl_direct_cb cb,
                    void* cb_opaque,
                    int ydir)
{
    /* Make sure that port is connected. */
    if (!sdkctl_socket_is_port_ready(mtsp->sdkctl)) {
        return -1;
    }

    /* Compress framebuffer region. 10% quality seems to be sufficient. */
    fmt->format = MTFB_JPEG;
    _fb_compress(mtsp, fmt, fb, 10, ydir);

    /* Total size of the update data: header + JPEG image. */
    const int update_size =
        sizeof(MTFrameHeader) + jpeg_compressor_get_jpeg_size(mtsp->jpeg_compressor);

    /* Update message starts at the beginning of the buffer allocated by the
     * compressor's destination manager. */
    uint8_t* const msg = (uint8_t*)jpeg_compressor_get_buffer(mtsp->jpeg_compressor);

    /* Initialize message header. */
    sdkctl_init_message_header(msg, SDKCTL_MT_FB_UPDATE, update_size);

    /* Copy framebuffer update header to the message. */
    memcpy(msg + sdkctl_message_get_header_size(), fmt, sizeof(MTFrameHeader));

    /* Compression rate... */
    const float comp_rate = ((float)jpeg_compressor_get_jpeg_size(mtsp->jpeg_compressor) / (fmt->w * fmt->h * fmt->bpp)) * 100;

    /* Zeroing the rectangle in the update header we indicate that it contains
     * no updates. */
    fmt->x = fmt->y = fmt->w = fmt->h = 0;

    /* Send update to the device. */
    sdkctl_direct_packet_send(mtsp->fb_packet, msg, cb, cb_opaque);

    T("Multi-touch: Sent %d bytes in framebuffer update. Compression rate is %.2f%%",
      update_size, comp_rate);

    return 0;
}
