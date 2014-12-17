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
#include "user-events.h"
#include "android/utils/debug.h"
#include "console.h"
#include <stdio.h>

void
user_event_keycodes(int *kcodes, int count)
{
    int nn;
    for (nn = 0; nn < count; nn++)
        user_event_keycode(kcodes[nn]);
}

void
user_event_keycode(int  kcode)
{
    kbd_put_keycode(kcode);
}

void
user_event_key(unsigned code, unsigned down)
{
    if(code == 0) {
        return;
    }
    if (VERBOSE_CHECK(keys))
        printf(">> KEY [0x%03x,%s]\n", (code & 0x1ff), down ? "down" : " up " );

    user_event_keycode((code & 0x1ff) | (down ? 0x200 : 0));
}


void
user_event_mouse(int dx, int dy, int dz, unsigned buttons_state)
{
    kbd_mouse_event(dx, dy, dz, buttons_state);
}

static QEMUPutGenericEvent *generic_event_callback;
static void*                generic_event_opaque;

void  user_event_register_generic(void* opaque, QEMUPutGenericEvent *callback)
{
    generic_event_callback = callback;
    generic_event_opaque   = opaque;
}

void
user_event_generic(int type, int code, int value)
{
    if (generic_event_callback)
        generic_event_callback(generic_event_opaque, type, code, value);
}
