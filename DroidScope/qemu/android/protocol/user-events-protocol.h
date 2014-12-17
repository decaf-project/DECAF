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

#ifndef _ANDROID_PROTOCOL_USER_EVENTS_H
#define _ANDROID_PROTOCOL_USER_EVENTS_H

/*
 * Contains declarations related to the UI events handled by the Core.
 */

#include "android/globals.h"

/* Mouse event. */
#define AUSER_EVENT_MOUSE     0
/* Keycode event. */
#define AUSER_EVENT_KEYCODE   1
/* Generic event. */
#define AUSER_EVENT_GENERIC   2

/* Header for user event message sent from the UI to the Core.
 * Every user event sent by the UI begins with this header, immediately followed
 * by the event parameters (if there are any).
 */
typedef struct UserEventHeader {
    /* Event type. See AUSER_EVENT_XXX for possible values. */
    uint8_t event_type;
} UserEventHeader;

/* Formats mouse event message (AUSER_EVENT_MOUSE) */
typedef struct UserEventMouse {
    int         dx;
    int         dy;
    int         dz;
    unsigned    buttons_state;
} UserEventMouse;

/* Formats keycode event message (AUSER_EVENT_KEYCODE) */
typedef struct UserEventKeycode {
    int         keycode;
} UserEventKeycode;

/* Formats generic event message (AUSER_EVENT_GENERIC) */
typedef struct UserEventGeneric {
    int         type;
    int         code;
    int         value;
} UserEventGeneric;

#endif /* _ANDROID_PROTOCOL_USER_EVENTS_H */
