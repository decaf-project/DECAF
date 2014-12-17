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
#ifndef _QEMU_USEREVENTS_H
#define _QEMU_USEREVENTS_H

/* A simple abstract interface to user-events. This is used to de-couple
 * QEMU-specific and UI-specific code.
 *
 * QEMU-specific implementation in user-events-qemu.c
 */

void  user_event_key(unsigned  key, unsigned  down);
void  user_event_keycode(int  kcode);
void  user_event_keycodes(int *kcodes, int count);
void  user_event_mouse(int dx, int dy, int dz, unsigned buttons_state);
void  user_event_generic(int type, int code, int value);

/* The following is used to register a callback function that will receive
 * user_event_generic() calls. This is used by hw/goldfish_events_device.c
 */
typedef void QEMUPutGenericEvent(void*  opaque, int  type, int  code, int  value);
void  user_event_register_generic(void* opaque, QEMUPutGenericEvent  callback);

#endif /* _QEMU_USEREVENTS_H */
