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
 * Contains extension to android display (see android/display.h|c) that is used
 * by the core to communicate display changes to the attached UI
 */

#include "android/display-core.h"
#include "qemu-common.h"

/* This callback is call periodically by the GUI timer.
 * Its purpose is to ensure that hw framebuffer updates are properly
 * detected. Call the normal QEMU function for this here.
 */
static void
coredisplay_refresh(DisplayState* ds)
{
    (void)ds;
    vga_hw_update();
}

/* Don't do anything here because this callback can't differentiate
 * between several listeners. This will be handled by a DisplayUpdateListener
 * instead. See Android-specific changes in console.h
 *
 * In the core, the DisplayUpdateListener is registered by the ProxyFramebuffer
 * object. See android/protocol/fb-updates-proxy.c.
 */
static void
coredisplay_update(DisplayState* ds, int x, int y, int w, int h)
{
    (void)ds;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
}

/* This callback is normally used to indicate that the display resolution
 * was changed by the guest (e.g. when a Windows PC changes the display from
 * 1024x768 to 800x600. Since this doesn't happen in Android, ignore it.
 */
static void
coredisplay_resize(DisplayState* ds)
{
    (void)ds;
}

void
coredisplay_init(DisplayState* ds)
{
    static DisplayChangeListener dcl[1];

    dcl->dpy_update  = coredisplay_update;
    dcl->dpy_refresh = coredisplay_refresh;
    dcl->dpy_resize  = coredisplay_resize;
    register_displaychangelistener(ds, dcl);
}
