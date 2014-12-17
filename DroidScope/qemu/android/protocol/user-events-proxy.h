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
 * Contains the UI-side of the "user events" service. Here we send user events
 * to the Core.
 */

#ifndef _ANDROID_PROTOCOL_USER_EVENTS_PROXY_H
#define _ANDROID_PROTOCOL_USER_EVENTS_PROXY_H

/* Creates and initializes descriptor for the UI-side of the "user-events"
 * service. Note that there can be only one instance of this service in the UI.
 * Param:
 *  console_socket - Addresses Core's console.
 * Return:
 *  0 on success, or < 0 on failure.
 */
extern int userEventsProxy_create(SockAddress* console_socket);

/* Destroys the UI-side of the "user-events". */
extern void userEventsProxy_destroy(void);

#endif /* _ANDROID_PROTOCOL_USER_EVENTS_PROXY_H */
