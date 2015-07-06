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
#include "user-events.h"
#include "android/display-core.h"
#include "android/hw-events.h"
#include "android/charmap.h"
#include "android/globals.h"  /* for android_hw */
#include "android/utils/misc.h"
#include "android/utils/debug.h"
#include "android/multitouch-screen.h"

#define  E(...)    derror(__VA_ARGS__)
#define  W(...)    dwarning(__VA_ARGS__)
#define  D(...)    VERBOSE_PRINT(mtscreen,__VA_ARGS__)
#define  D_ACTIVE  VERBOSE_CHECK(mtscreen)

#define TRACE_ON    1

#if TRACE_ON
#define  T(...)    VERBOSE_PRINT(mtscreen,__VA_ARGS__)
#else
#define  T(...)
#endif


/* Maximum number of pointers, supported by multi-touch emulation. */
#define MTS_POINTERS_NUM    10
/* Signals that pointer is not tracked (or is "up"). */
#define MTS_POINTER_UP      -1
/* Special tracking ID for a mouse pointer. */
#define MTS_POINTER_MOUSE   -2

/* Describes state of a multi-touch pointer  */
typedef struct MTSPointerState {
    /* Tracking ID assigned to the pointer by an app emulating multi-touch. */
    int tracking_id;
    /* X - coordinate of the tracked pointer. */
    int x;
    /* Y - coordinate of the tracked pointer. */
    int y;
    /* Current pressure value. */
    int pressure;
} MTSPointerState;

/* Describes state of an emulated multi-touch screen. */
typedef struct MTSState {
    /* Multi-touch port connected to the device. */
    AndroidMTSPort* mtsp;
    /* Emulator's display state. */
    DisplayState*   ds;
    /* Number of tracked pointers. */
    int             tracked_ptr_num;
    /* Index in the 'tracked_pointers' array of the last pointer for which
     * ABS_MT_SLOT was sent. -1 indicates that no slot selection has been made
     * yet. */
    int             current_slot;
    /* Accumulator for ABS_TOUCH_MAJOR value. */
    int             touch_major;
    /* Array of multi-touch pointers. */
    MTSPointerState tracked_pointers[MTS_POINTERS_NUM];
    /* Header collecting framebuffer information and updates. */
    MTFrameHeader   fb_header;
    /* Boolean value indicating if framebuffer updates are currently being
     * transferred to the application running on the device. */
    int             fb_transfer_in_progress;
    /* Indicates direction in which lines are arranged in the framebuffer. If
     * this value is negative, lines are arranged in bottom-up format (i.e. the
     * bottom line is at the beginning of the buffer). */
    int             ydir;
    /* Current framebuffer pointer. */
    uint8_t*        current_fb;
} MTSState;

/* Default multi-touch screen descriptor */
static MTSState _MTSState = { 0 };

/* Pushes event to the event device. */
static void
_push_event(int type, int code, int value)
{
    user_event_generic(type, code, value);
}

/* Gets an index in the MTS's tracking pointers array MTS for the given
 * tracking id.
 * Return:
 *  Index of a matching entry in the MTS's tracking pointers array, or -1 if
 *  matching entry was not found.
 */
static int
_mtsstate_get_pointer_index(const MTSState* mts_state, int tracking_id)
{
    int index;
    for (index = 0; index < MTS_POINTERS_NUM; index++) {
        if (mts_state->tracked_pointers[index].tracking_id == tracking_id) {
            return index;
        }
    }
    return -1;
}

/* Gets an index of the first untracking pointer in the MTS's tracking pointers
 * array.
 * Return:
 *  An index of the first untracking pointer, or -1 if all pointers are tracked.
 */
static int
_mtsstate_get_available_pointer_index(const MTSState* mts_state)
{
    return _mtsstate_get_pointer_index(mts_state, MTS_POINTER_UP);
}

/* Handles a "pointer down" event
 * Param:
 *  mts_state - MTS state descriptor.
 *  tracking_id - Tracking ID of the "downed" pointer.
 *  x, y - "Downed" pointer coordinates,
 *  pressure - Pressure value for the pointer.
 */
static void
_mts_pointer_down(MTSState* mts_state, int tracking_id, int x, int y, int pressure)
{
    /* Get first available slot for the new pointer. */
    const int slot_index = _mtsstate_get_available_pointer_index(mts_state);

    /* Make sure there is a place for the pointer. */
    if (slot_index >= 0) {
        /* Initialize pointer's entry. */
        mts_state->tracked_ptr_num++;
        mts_state->tracked_pointers[slot_index].tracking_id = tracking_id;
        mts_state->tracked_pointers[slot_index].x = x;
        mts_state->tracked_pointers[slot_index].y = y;
        mts_state->tracked_pointers[slot_index].pressure = pressure;

        /* Send events indicating a "pointer down" to the EventHub */
        /* Make sure that correct slot is selected. */
        if (slot_index != mts_state->current_slot) {
            _push_event(EV_ABS, ABS_MT_SLOT, slot_index);
        }
        _push_event(EV_ABS, ABS_MT_TRACKING_ID, slot_index);
        _push_event(EV_ABS, ABS_MT_TOUCH_MAJOR, ++mts_state->touch_major);
        _push_event(EV_ABS, ABS_MT_PRESSURE, pressure);
        _push_event(EV_ABS, ABS_MT_POSITION_X, x);
        _push_event(EV_ABS, ABS_MT_POSITION_Y, y);
        _push_event(EV_SYN, SYN_REPORT, 0);
        mts_state->current_slot = slot_index;
    } else {
        D("MTS pointer count is exceeded.");
        return;
    }
}

/* Handles a "pointer up" event
 * Param:
 *  mts_state - MTS state descriptor.
 *  slot_index - Pointer's index in the MTS's array of tracked pointers.
 */
static void
_mts_pointer_up(MTSState* mts_state, int slot_index)
{
    /* Make sure that correct slot is selected. */
    if (slot_index != mts_state->current_slot) {
        _push_event(EV_ABS, ABS_MT_SLOT, slot_index);
    }

    /* Send event indicating "pointer up" to the EventHub. */
    _push_event(EV_ABS, ABS_MT_TRACKING_ID, -1);
    _push_event(EV_SYN, SYN_REPORT, 0);

    /* Update MTS descriptor, removing the tracked pointer. */
    mts_state->tracked_pointers[slot_index].tracking_id = MTS_POINTER_UP;
    mts_state->tracked_pointers[slot_index].x = 0;
    mts_state->tracked_pointers[slot_index].y = 0;
    mts_state->tracked_pointers[slot_index].pressure = 0;

    /* Since current slot is no longer tracked, make sure we will do a "select"
     * next time we send events to the EventHub. */
    mts_state->current_slot = -1;
    mts_state->tracked_ptr_num--;
    assert(mts_state->tracked_ptr_num >= 0);
}

/* Handles a "pointer move" event
 * Param:
 *  mts_state - MTS state descriptor.
 *  slot_index - Pointer's index in the MTS's array of tracked pointers.
 *  x, y - New pointer coordinates,
 *  pressure - Pressure value for the pointer.
 */
static void
_mts_pointer_move(MTSState* mts_state, int slot_index, int x, int y, int pressure)
{
    MTSPointerState* ptr_state = &mts_state->tracked_pointers[slot_index];

    /* Make sure that coordinates have really changed. */
    if (ptr_state->x == x && ptr_state->y == y) {
        /* Coordinates didn't change. Bail out. */
        return;
    }

    /* Make sure that the right slot is selected. */
    if (slot_index != mts_state->current_slot) {
        _push_event(EV_ABS, ABS_MT_SLOT, slot_index);
        mts_state->current_slot = slot_index;
    }

    /* Push the changes down. */
    if (ptr_state->pressure != pressure && pressure != 0) {
        _push_event(EV_ABS, ABS_MT_PRESSURE, pressure);
        ptr_state->pressure = pressure;
    }
    if (ptr_state->x != x) {
        _push_event(EV_ABS, ABS_MT_POSITION_X, x);
        ptr_state->x = x;
    }
    if (ptr_state->y != y) {
        _push_event(EV_ABS, ABS_MT_POSITION_Y, y);
        ptr_state->y = y;
    }
    _push_event(EV_SYN, SYN_REPORT, 0);
}

/********************************************************************************
 *                       Multi-touch API
 *******************************************************************************/

/* Multi-touch service initialization flag. */
static int _is_mt_initialized = 0;

/* Callback that is invoked when framebuffer update has been transmitted to the
 * device. */
static AsyncIOAction
_on_fb_sent(void* opaque, SDKCtlDirectPacket* packet, AsyncIOState status)
{
    MTSState* const mts_state = (MTSState*)opaque;

    if (status == ASIO_STATE_SUCCEEDED) {
        /* Lets see if we have accumulated more changes while transmission has been
         * in progress. */
        if (mts_state->fb_header.w && mts_state->fb_header.h &&
            !mts_state->fb_transfer_in_progress) {
            mts_state->fb_transfer_in_progress = 1;
            /* Send accumulated updates. */
            if (mts_port_send_frame(mts_state->mtsp, &mts_state->fb_header,
                                    mts_state->current_fb, _on_fb_sent, mts_state,
                                    mts_state->ydir)) {
                mts_state->fb_transfer_in_progress = 0;
            }
        }
    }

    return ASIO_ACTION_DONE;
}

/* Common handler for framebuffer updates invoked by both, software, and OpenGLES
 * renderers.
 */
static void
_mt_fb_common_update(MTSState* mts_state, int x, int y, int w, int h)
{
    if (mts_state->fb_header.w == 0 && mts_state->fb_header.h == 0) {
        /* First update after previous one has been transmitted to the device. */
        mts_state->fb_header.x = x;
        mts_state->fb_header.y = y;
        mts_state->fb_header.w = w;
        mts_state->fb_header.h = h;
    } else {
        /*
         * Accumulate framebuffer changes in the header.
         */

        /* "right" and "bottom" coordinates of the current update. */
        int right = mts_state->fb_header.x + mts_state->fb_header.w;
        int bottom = mts_state->fb_header.y + mts_state->fb_header.h;

        /* "right" and "bottom" coordinates of the new update. */
        const int new_right = x + w;
        const int new_bottom = y + h;

        /* Accumulate changed rectangle coordinates in the header. */
        if (mts_state->fb_header.x > x) {
            mts_state->fb_header.x = x;
        }
        if (mts_state->fb_header.y > y) {
            mts_state->fb_header.y = y;
        }
        if (right < new_right) {
            right = new_right;
        }
        if (bottom < new_bottom) {
            bottom = new_bottom;
        }
        mts_state->fb_header.w = right - mts_state->fb_header.x;
        mts_state->fb_header.h = bottom - mts_state->fb_header.y;
    }

    /* We will send updates to the device only after previous transmission is
     * completed. */
    if (!mts_state->fb_transfer_in_progress) {
        mts_state->fb_transfer_in_progress = 1;
        if (mts_port_send_frame(mts_state->mtsp, &mts_state->fb_header,
                                mts_state->current_fb, _on_fb_sent, mts_state,
                                mts_state->ydir)) {
            mts_state->fb_transfer_in_progress = 0;
        }
    }
}

/* A callback invoked on framebuffer updates by software renderer.
 * Param:
 *  opaque - MTSState instance.
 *  x, y, w, h - Defines an updated rectangle inside the framebuffer.
 */
static void
_mt_fb_update(void* opaque, int x, int y, int w, int h)
{
    MTSState* const mts_state = (MTSState*)opaque;
    const DisplaySurface* const surface = mts_state->ds->surface;

    T("Multi-touch: Software renderer framebuffer update: %d:%d -> %dx%d",
      x, y, w, h);

    /* TODO: For sofware renderer general framebuffer properties can change on
     * the fly. Find a callback that can catch that. For now, just copy FB
     * properties over in every FB update. */
    mts_state->fb_header.bpp = surface->pf.bytes_per_pixel;
    mts_state->fb_header.bpl = surface->linesize;
    mts_state->fb_header.disp_width = surface->width;
    mts_state->fb_header.disp_height = surface->height;
    mts_state->current_fb = surface->data;
    mts_state->ydir = 1;

    _mt_fb_common_update(mts_state, x, y, w, h);
}

void
multitouch_opengles_fb_update(void* context,
                              int w, int h, int ydir,
                              int format, int type,
                              unsigned char* pixels)
{
    MTSState* const mts_state = &_MTSState;

    /* Make sure MT port is initialized. */
    if (!_is_mt_initialized) {
        return;
    }

    T("Multi-touch: openGLES framebuffer update: 0:0 -> %dx%d", w, h);

    /* GLES format is always RGBA8888 */
    mts_state->fb_header.bpp = 4;
    mts_state->fb_header.bpl = 4 * w;
    mts_state->fb_header.disp_width = w;
    mts_state->fb_header.disp_height = h;
    mts_state->current_fb = pixels;
    mts_state->ydir = ydir;

    /* GLES emulator alwas update the entire framebuffer. */
    _mt_fb_common_update(mts_state, 0, 0, w, h);
}

void
multitouch_refresh_screen(void)
{
    MTSState* const mts_state = &_MTSState;

    /* Make sure MT port is initialized. */
    if (!_is_mt_initialized) {
        return;
    }

    /* Lets see if any updates have been received so far. */
    if (NULL != mts_state->current_fb) {
        _mt_fb_common_update(mts_state, 0, 0, mts_state->fb_header.disp_width,
                             mts_state->fb_header.disp_height);
    }
}

void
multitouch_fb_updated(void)
{
    MTSState* const mts_state = &_MTSState;

    /* This concludes framebuffer update. */
    mts_state->fb_transfer_in_progress = 0;

    /* Lets see if we have accumulated more changes while transmission has been
     * in progress. */
    if (mts_state->fb_header.w && mts_state->fb_header.h) {
        mts_state->fb_transfer_in_progress = 1;
        /* Send accumulated updates. */
        if (mts_port_send_frame(mts_state->mtsp, &mts_state->fb_header,
                                mts_state->current_fb, _on_fb_sent, mts_state,
                                mts_state->ydir)) {
            mts_state->fb_transfer_in_progress = 0;
        }
    }
}

void
multitouch_init(AndroidMTSPort* mtsp)
{
    if (!_is_mt_initialized) {
        MTSState* const mts_state = &_MTSState;
        DisplayState* const ds = get_displaystate();
        DisplayUpdateListener* dul;
        int index;

        /*
         * Initialize the descriptor.
         */

        memset(mts_state, 0, sizeof(MTSState));
        mts_state->tracked_ptr_num = 0;
        mts_state->current_slot = -1;
        for (index = 0; index < MTS_POINTERS_NUM; index++) {
            mts_state->tracked_pointers[index].tracking_id = MTS_POINTER_UP;
        }
        mts_state->mtsp = mtsp;
        mts_state->fb_header.header_size = sizeof(MTFrameHeader);
        mts_state->fb_transfer_in_progress = 0;

        /*
         * Set framebuffer update listener.
         */

        ANEW0(dul);
        dul->opaque = &_MTSState;
        dul->dpy_update = _mt_fb_update;

        /* Initialize framebuffer information in the screen descriptor. */
        mts_state->ds = ds;
        mts_state->fb_header.disp_width = ds->surface->width;
        mts_state->fb_header.disp_height = ds->surface->height;
        mts_state->fb_header.x = mts_state->fb_header.y = 0;
        mts_state->fb_header.w = mts_state->fb_header.h = 0;
        mts_state->fb_header.bpp = ds->surface->pf.bytes_per_pixel;
        mts_state->fb_header.bpl = ds->surface->linesize;
        mts_state->fb_transfer_in_progress = 0;

        register_displayupdatelistener(ds, dul);

        _is_mt_initialized = 1;
    }
}

void
multitouch_update_pointer(MTESource source,
                          int tracking_id,
                          int x,
                          int y,
                          int pressure)
{
    MTSState* const mts_state = &_MTSState;

    /* Assign a fixed tracking ID to the mouse pointer. */
    if (source == MTES_MOUSE) {
        tracking_id = MTS_POINTER_MOUSE;
    }

    /* Find the tracked pointer for the tracking ID. */
    const int slot_index = _mtsstate_get_pointer_index(mts_state, tracking_id);
    if (slot_index < 0) {
        /* This is the first time the pointer is seen. Must be "pressed",
         * otherwise it's "hoovering", which we don't support yet. */
        if (pressure == 0) {
            if (tracking_id != MTS_POINTER_MOUSE) {
                D("Unexpected MTS pointer update for tracking id: %d",
                   tracking_id);
            }
            return;
        }

        /* This is a "pointer down" event */
        _mts_pointer_down(mts_state, tracking_id, x, y, pressure);
    } else if (pressure == 0) {
        /* This is a "pointer up" event */
        _mts_pointer_up(mts_state, slot_index);
    } else {
        /* This is a "pointer move" event */
        _mts_pointer_move(mts_state, slot_index, x, y, pressure);
    }
}

int
multitouch_get_max_slot()
{
    return MTS_POINTERS_NUM - 1;
}
