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
 * Contains core-side framebuffer service that sends framebuffer updates
 * to the UI connected to the core.
 */

#ifndef _ANDROID_PROTOCOL_FB_UPDATES_PROXY_H
#define _ANDROID_PROTOCOL_FB_UPDATES_PROXY_H

/* Descriptor for a framebuffer core service instance */
typedef struct ProxyFramebuffer ProxyFramebuffer;

/*
 * Creates framebuffer service.
 * Param:
 *  sock - Socket descriptor for the service
 *  protocol - Defines protocol to use when sending FB updates to the UI. The
 *      supported values ar:
 *      -raw Transfers the updating rectangle buffer over the socket.
 *      -shared Used a shared memory to transfer the updating rectangle buffer.
 * Return:
 *  Framebuffer service descriptor.
 */
ProxyFramebuffer* proxyFb_create(int sock, const char* protocol);

/*
 * Destroys framebuffer service created with proxyFb_create.
 * Param:
 *  core_fb - Framebuffer service descriptor created with proxyFb_create
 */
void proxyFb_destroy(ProxyFramebuffer* core_fb);

/*
 * Gets number of bits used to encode a single pixel.
 * Param:
 *  core_fb - Framebuffer service descriptor created with proxyFb_create
 * Return:
 *  Number of bits used to encode a single pixel.
 */
int proxyFb_get_bits_per_pixel(ProxyFramebuffer* core_fb);

#endif /* _ANDROID_PROTOCOL_FB_UPDATES_PROXY_H */
