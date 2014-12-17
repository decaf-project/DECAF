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
 * Contains the Core-side of the "user events" service. Here we receive and
 * handle user events sent from the UI.
 */

#ifndef _ANDROID_PROTOCOL_USER_EVENTS_IMPL_H
#define _ANDROID_PROTOCOL_USER_EVENTS_IMPL_H

/* Creates and initializes descriptor for the Core-side of the "user-events"
 * service. Note that there can be only one instance of this service in the core.
 * Param:
 *  fd - Socket descriptor for the service.
 * Return:
 *  0 on success, or < 0 on failure.
 */
extern int userEventsImpl_create(int fd);

/* Destroys the descriptor for the Core-side of the "user-events" service. */
extern void userEventsImpl_destroy(void);

#endif /* _ANDROID_PROTOCOL_USER_EVENTS_IMPL_H */
