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

#ifndef ANDROID_MULTITOUCH_SCREEN_H_
#define ANDROID_MULTITOUCH_SCREEN_H_

#include "android/sdk-controller-socket.h"
#include "android/multitouch-port.h"

/*
 * Encapsulates functionality of multi-touch screen. Main task of this component
 * is to report touch events to the emulated system via event device (see
 * hw/goldfish_events_device.c) The source of touch events can be a mouse, or an
 * actual android device that is used for multi-touch emulation. Note that since
 * we need to simultaneousely support a mouse and a device as event source, we
 * need to know which one has sent us a touch event. This is important for proper
 * tracking of pointer IDs when multitouch is in play.
 */

/* Defines a source of multi-touch event. This is used to properly track
 * pointer IDs.
 */
typedef enum MTESource {
    /* The event is associated with a mouse. */
    MTES_MOUSE,
    /* The event is associated with an actual android device. */
    MTES_DEVICE,
} MTESource;

/* Initializes MTSState instance.
 * Param:
 *  mtsp - Instance of the multi-touch port connected to the device.
 */
extern void multitouch_init(AndroidMTSPort* mtsp);

/* Handles a MT pointer event.
 * Param:
 *  source - Identifies the source of the event (mouse or a device).
 *  tracking_id - Tracking ID of the pointer.
 *  x, y - Pointer coordinates,
 *  pressure - Pressure value for the pointer.
 */
extern void multitouch_update_pointer(MTESource source,
                                      int tracking_id,
                                      int x,
                                      int y,
                                      int pressure);

/* Gets maximum slot index available for the multi-touch emulation. */
extern int multitouch_get_max_slot();

/* A callback set to monitor OpenGLES framebuffer updates.
 * This callback is called by the renderer just before each new frame is
 * displayed, providing a copy of the framebuffer contents.
 * The callback will be called from one of the renderer's threads, so it may
 * require synchronization on any data structures it modifies. The pixels buffer
 * may be overwritten as soon as the callback returns.
 * The pixels buffer is intentionally not const: the callback may modify the data
 * without copying to another buffer if it wants, e.g. in-place RGBA to RGB
 * conversion, or in-place y-inversion.
 * Param:
 *   context        The pointer optionally provided when the callback was
 *                  registered. The client can use this to pass whatever
 *                  information it wants to the callback.
 *   width, height  Dimensions of the image, in pixels. Rows are tightly packed;
 *                  there is no inter-row padding.
 *   ydir           Indicates row order: 1 means top-to-bottom order, -1 means
 *                  bottom-to-top order.
 *   format, type   Format and type GL enums, as used in glTexImage2D() or
 *                  glReadPixels(), describing the pixel format.
 *   pixels         The framebuffer image.
 *
 * In the first implementation, ydir is always -1 (bottom to top), format and
 * type are always GL_RGBA and GL_UNSIGNED_BYTE, and the width and height will
 * always be the same as the ones passed to initOpenGLRenderer().
 */
extern void multitouch_opengles_fb_update(void* context,
                                          int width,
                                          int height,
                                          int ydir,
                                          int format,
                                          int type,
                                          unsigned char* pixels);

/* Pushes the entire framebuffer to the device. This will force the device to
 * refresh the entire screen.
 */
extern void multitouch_refresh_screen(void);

/* Framebuffer update has been handled by the device. */
extern void multitouch_fb_updated(void);

#endif  /* ANDROID_MULTITOUCH_SCREEN_H_ */
