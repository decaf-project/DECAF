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
 * Contains the API for calling into the UI with Core's framebuffer updates.
 */

#ifndef _ANDROID_PROTOCOL_FB_UPDATES_H
#define _ANDROID_PROTOCOL_FB_UPDATES_H

#include "sysemu.h"

/* Requests the Core to refresh framebuffer.
 * This message is sent by the UI to the Core right after the UI is initialized.
 * This message forces the Core to send a full display update back to the UI. */
#define AFB_REQUEST_REFRESH     1

/* Header of framebuffer update message sent from the core to the UI. */
typedef struct FBUpdateMessage {
    /* x, y, w, and h identify the rectangle that is being updated. */
    uint16_t    x;
    uint16_t    y;
    uint16_t    w;
    uint16_t    h;

    /* Contains updating rectangle copied over from the framebuffer's pixels. */
    uint8_t rect[0];
} FBUpdateMessage;

/* Header for framebuffer requests sent from the UI to the Core. */
typedef struct FBRequestHeader {
    /* Request type. See AFB_REQUEST_XXX for the values. */
    uint8_t request_type;
} FBRequestHeader;

#endif /* _ANDROID_PROTOCOL_FB_UPDATES_H */
