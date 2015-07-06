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

#ifndef ANDROID_ANDROID_MULTITOUCH_PORT_H_
#define ANDROID_ANDROID_MULTITOUCH_PORT_H_

#include "android/sdk-controller-socket.h"

/*
 * Encapsulates exchange protocol between the multi-touch screen emulator, and an
 * application running on an Android device that provides touch events, and is
 * connected to the host via USB.
 */

/*
 * Codes that define transmitted framebuffer format:
 *
 * NOTE: Application on the device side depends on these values. Any changes
 * made here must be reflected in the app too. Application location is at
 * 'sdk/apps/SdkController/SdkControllerMultitouch' root.
 */

/* Framebuffer is transmitted as JPEG. */
#define MTFB_JPEG       1
/* Framebuffer is transmitted as raw RGB565 bitmap. */
#define MTFB_RGB565     2
/* Framebuffer is transmitted as raw RGB888 bitmap. */
#define MTFB_RGB888     3

/* Framebuffer update descriptor.
 * This descriptor is used to collect properties of the updated framebuffer
 * region. This descriptor is also sent to the MT emulation application on the
 * device, so it can properly redraw its screen.
 *
 * NOTE: Application on the device side depends on that structure. Any changes
 * made here must be reflected in the app too. Application location is at
 * 'sdk/apps/SdkController/SdkControllerMultitouch' root.
 */
typedef struct MTFrameHeader {
    /* Size of the header. Must be always sizeof(MTFrameHeader). */
    int         header_size;
    /* Display width */
    int         disp_width;
    /* Display height */
    int         disp_height;
    /* x, y, w, and h define framebuffer region that has been updated. */
    int         x;
    int         y;
    int         w;
    int         h;
    /* Bytes per line in the framebufer. */
    int         bpl;
    /* Bytes per pixel in the framebufer. */
    int         bpp;
    /* Defines format in which framebuffer is transmitted to the device. */
    int         format;
} MTFrameHeader;

/* Declares multi-touch port descriptor. */
typedef struct AndroidMTSPort AndroidMTSPort;

/* Creates multi-touch port, and connects it to the device.
 * Param:
 *  opaque - An opaque pointer that is passed back to the callback routines.
 * Return:
 *  Initialized device descriptor on success, or NULL on failure. If failure is
 *  returned from this routine, 'errno' indicates the reason for failure. If this
 *  routine successeds, a connection is established with the sensor reading
 *  application on the device.
 */
extern AndroidMTSPort* mts_port_create(void* opaque);

/* Disconnects from the multi-touch port, and destroys the descriptor. */
extern void mts_port_destroy(AndroidMTSPort* amtp);

/* Sends framebuffer update to the multi-touch emulation application, running on
 * the android device.
 * Param:
 *  mtsp - Android multi-touch port instance returned from mts_port_create.
 *  fmt - Framebuffer update descriptor.
 *  fb - Beginning of the framebuffer.
 *  cb - Callback to invoke when update has been transferred to the MT-emulating
 *      application on the device.
 *  cb_opaque - An opaque parameter to pass back to the 'cb' callback.
 *  ydir - Indicates direction in which lines are arranged in the framebuffer. If
 *      this value is negative, lines are arranged in bottom-up format (i.e. the
 *      bottom line is at the beginning of the buffer).
 * Return:
 *  0 on success, or != 0 on failure.
 */
extern int mts_port_send_frame(AndroidMTSPort* mtsp,
                               MTFrameHeader* fmt,
                               const uint8_t* fb,
                               on_sdkctl_direct_cb cb,
                               void* cb_opaque,
                               int ydir);

#endif  /* ANDROID_ANDROID_MULTITOUCH_PORT_H_ */
