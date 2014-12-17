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

#ifndef _ANDROID_PROTOCOL_ATTACH_UI_PROXY_H
#define _ANDROID_PROTOCOL_ATTACH_UI_PROXY_H

/*
 * Contains the Core-side implementation of the "attach-ui" service that is
 * used to establish connection between the UI and the Core.
 */

/* Creates and initializes descriptor for the Core-side of the "atatch-ui"
 * service. Note that there can be only one instance of this service in the core.
 * Param:
 *  fd - Socket descriptor for the proxy.
 * Return:
 *  0 on success, or < 0 on failure.
 */
extern int attachUiProxy_create(int fd);

/* Destroys the descriptor for the Core-side of the "attach-ui" service. */
extern void attachUiProxy_destroy(void);

#endif /* _ANDROID_PROTOCOL_ATTACH_UI_PROXY_H */
