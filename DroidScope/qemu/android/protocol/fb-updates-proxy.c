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

#include "console.h"
#include "android/looper.h"
#include "android/display-core.h"
#include "android/async-utils.h"
#include "android/protocol/fb-updates.h"
#include "android/protocol/fb-updates-proxy.h"
#include "android/utils/system.h"
#include "android/utils/debug.h"

/* Descriptor for the Core-side implementation of the "framebufer" service.
 */
struct ProxyFramebuffer {
    /* Writer used to send FB update notification messages. */
    AsyncWriter             fb_update_writer;

    /* Reader used to read FB requests from the client. */
    AsyncReader             fb_req_reader;

    /* I/O associated with this descriptor. */
    LoopIo                  io;

    /* Display state used for this service */
    DisplayState*           ds;
    DisplayUpdateListener*  ds_listener;

    /* Looper used to communicate framebuffer updates. */
    Looper* looper;

    /* Head of the list of pending FB update notifications. */
    struct FBUpdateNotify*  fb_update_head;

    /* Tail of the list of pending FB update notifications. */
    struct FBUpdateNotify*  fb_update_tail;

    /* Socket used to communicate framebuffer updates. */
    int     sock;

    /* Framebuffer request header. */
    FBRequestHeader         fb_req_header;
};

/* Framebuffer update notification descriptor. */
typedef struct FBUpdateNotify {
    /* Links all pending FB update notifications. */
    struct FBUpdateNotify*  next_fb_update;

    /* Core framebuffer instance that owns the message. */
    ProxyFramebuffer*       proxy_fb;

    /* Size of the message to transfer. */
    size_t                  message_size;

    /* Update message. */
    FBUpdateMessage         message;
} FBUpdateNotify;

/*
 * Gets pointer in framebuffer's pixels for the given pixel.
 * Param:
 *  fb Framebuffer containing pixels.
 *  x, and y identify the pixel to get pointer for.
 * Return:
 *  Pointer in framebuffer's pixels for the given pixel.
 */
static const uint8_t*
_pixel_offset(const DisplaySurface* dsu, int x, int y)
{
    return (const uint8_t*)dsu->data + y * dsu->linesize + x * dsu->pf.bytes_per_pixel;
}

/*
 * Copies pixels from a framebuffer rectangle.
 * Param:
 *  rect - Buffer where to copy pixel.
 *  fb - Framebuffer containing the rectangle to copy.
 *  x, y, w, and h - dimensions of the rectangle to copy.
 */
static void
_copy_fb_rect(uint8_t* rect, const DisplaySurface* dsu, int x, int y, int w, int h)
{
    const uint8_t* start = _pixel_offset(dsu, x, y);
    for (; h > 0; h--) {
        memcpy(rect, start, w * dsu->pf.bytes_per_pixel);
        start += dsu->linesize;
        rect += w * dsu->pf.bytes_per_pixel;
    }
}

/*
 * Allocates and initializes framebuffer update notification descriptor.
 * Param:
 *  ds - Display state for the framebuffer.
 *  fb Framebuffer containing pixels.
 *  x, y, w, and h identify the rectangle that is being updated.
 * Return:
 *  Initialized framebuffer update notification descriptor.
 */
static FBUpdateNotify*
fbupdatenotify_create(ProxyFramebuffer* proxy_fb,
                      int x, int y, int w, int h)
{
    const size_t rect_size = w * h * proxy_fb->ds->surface->pf.bytes_per_pixel;
    FBUpdateNotify* ret = malloc(sizeof(FBUpdateNotify) + rect_size);

    ret->next_fb_update = NULL;
    ret->proxy_fb = proxy_fb;
    ret->message_size = sizeof(FBUpdateMessage) + rect_size;
    ret->message.x = x;
    ret->message.y = y;
    ret->message.w = w;
    ret->message.h = h;
    _copy_fb_rect(ret->message.rect, proxy_fb->ds->surface, x, y, w, h);
    return ret;
}

/*
 * Deletes FBUpdateNotify descriptor, created with fbupdatenotify_create.
 * Param:
 *  desc - Descreptor to delete.
 */
static void
fbupdatenotify_delete(FBUpdateNotify* desc)
{
    if (desc != NULL) {
        free(desc);
    }
}

/*
 * Asynchronous write I/O callback launched when writing framebuffer
 * notifications to the socket.
 * Param:
 *  proxy_fb - ProxyFramebuffer instance.
 */
static void
_proxyFb_io_write(ProxyFramebuffer* proxy_fb)
{
    while (proxy_fb->fb_update_head != NULL) {
        FBUpdateNotify* current_update = proxy_fb->fb_update_head;
        // Lets continue writing of the current notification.
        const AsyncStatus status =
            asyncWriter_write(&proxy_fb->fb_update_writer);
        switch (status) {
            case ASYNC_COMPLETE:
                // Done with the current update. Move on to the next one.
                break;
            case ASYNC_ERROR:
                // Done with the current update. Move on to the next one.
                loopIo_dontWantWrite(&proxy_fb->io);
                break;

            case ASYNC_NEED_MORE:
                // Transfer will eventually come back into this routine.
                return;
        }

        // Advance the list of updates
        proxy_fb->fb_update_head = current_update->next_fb_update;
        if (proxy_fb->fb_update_head == NULL) {
            proxy_fb->fb_update_tail = NULL;
        }
        fbupdatenotify_delete(current_update);

        if (proxy_fb->fb_update_head != NULL) {
            // Schedule the next one.
            asyncWriter_init(&proxy_fb->fb_update_writer,
                             &proxy_fb->fb_update_head->message,
                             proxy_fb->fb_update_head->message_size,
                             &proxy_fb->io);
        }
    }
}

static void proxyFb_update(void* opaque, int x, int y, int w, int h);

/*
 * Asynchronous read I/O callback launched when reading framebuffer requests
 * from the socket.
 * Param:
 *  proxy_fb - ProxyFramebuffer instance.
 */
static void
_proxyFb_io_read(ProxyFramebuffer* proxy_fb)
{
    // Read the request header.
    DisplaySurface* dsu;
    const AsyncStatus status =
        asyncReader_read(&proxy_fb->fb_req_reader);
    switch (status) {
        case ASYNC_COMPLETE:
            // Request header is received
            switch (proxy_fb->fb_req_header.request_type) {
                case AFB_REQUEST_REFRESH:
                    // Force full screen update to be sent
                    dsu = proxy_fb->ds->surface;
                    proxyFb_update(proxy_fb,
                                  0, 0, dsu->width, dsu->height);
                    break;
                default:
                    derror("Unknown framebuffer request %d\n",
                           proxy_fb->fb_req_header.request_type);
                    break;
            }
            proxy_fb->fb_req_header.request_type = -1;
            asyncReader_init(&proxy_fb->fb_req_reader, &proxy_fb->fb_req_header,
                             sizeof(proxy_fb->fb_req_header), &proxy_fb->io);
            break;
        case ASYNC_ERROR:
            loopIo_dontWantRead(&proxy_fb->io);
            if (errno == ECONNRESET) {
                // UI has exited. We need to destroy framebuffer service.
                proxyFb_destroy(proxy_fb);
            }
            break;

        case ASYNC_NEED_MORE:
            // Transfer will eventually come back into this routine.
            return;
    }
}

/*
 * Asynchronous I/O callback launched when writing framebuffer notifications
 * to the socket.
 * Param:
 *  opaque - ProxyFramebuffer instance.
 */
static void
_proxyFb_io_fun(void* opaque, int fd, unsigned events)
{
    if (events & LOOP_IO_READ) {
        _proxyFb_io_read((ProxyFramebuffer*)opaque);
    } else if (events & LOOP_IO_WRITE) {
        _proxyFb_io_write((ProxyFramebuffer*)opaque);
    }
}

ProxyFramebuffer*
proxyFb_create(int sock, const char* protocol)
{
    // At this point we're implementing the -raw protocol only.
    ProxyFramebuffer* ret;
    DisplayState* ds = get_displaystate();
    DisplayUpdateListener* dul;

    ANEW0(ret);
    ret->sock = sock;
    ret->looper = looper_newCore();
    ret->ds = ds;

    ANEW0(dul);
    dul->opaque = ret;
    dul->dpy_update = proxyFb_update;
    register_displayupdatelistener(ds, dul);
    ret->ds_listener = dul;

    ret->fb_update_head = NULL;
    ret->fb_update_tail = NULL;
    loopIo_init(&ret->io, ret->looper, sock, _proxyFb_io_fun, ret);
    asyncReader_init(&ret->fb_req_reader, &ret->fb_req_header,
                     sizeof(ret->fb_req_header), &ret->io);
    return ret;
}

void
proxyFb_destroy(ProxyFramebuffer* proxy_fb)
{
    if (proxy_fb != NULL) {
        unregister_displayupdatelistener(proxy_fb->ds, proxy_fb->ds_listener);
        if (proxy_fb->looper != NULL) {
            // Stop all I/O that may still be going on.
            loopIo_done(&proxy_fb->io);
            // Delete all pending frame updates.
            while (proxy_fb->fb_update_head != NULL) {
                FBUpdateNotify* pending_update = proxy_fb->fb_update_head;
                proxy_fb->fb_update_head = pending_update->next_fb_update;
                fbupdatenotify_delete(pending_update);
            }
            proxy_fb->fb_update_tail = NULL;
            looper_free(proxy_fb->looper);
            proxy_fb->looper = NULL;
        }
        AFREE(proxy_fb);
    }
}

static void
proxyFb_update(void* opaque, int x, int y, int w, int h)
{
    ProxyFramebuffer* proxy_fb = opaque;
    AsyncStatus status;
    FBUpdateNotify* descr = fbupdatenotify_create(proxy_fb, x, y, w, h);

    // Lets see if we should list it behind other pending updates.
    if (proxy_fb->fb_update_tail != NULL) {
        proxy_fb->fb_update_tail->next_fb_update = descr;
        proxy_fb->fb_update_tail = descr;
        return;
    }

    // We're first in the list. Just send it now.
    proxy_fb->fb_update_head = proxy_fb->fb_update_tail = descr;
    asyncWriter_init(&proxy_fb->fb_update_writer,
                     &proxy_fb->fb_update_head->message,
                     proxy_fb->fb_update_head->message_size, &proxy_fb->io);
    status = asyncWriter_write(&proxy_fb->fb_update_writer);
    switch (status) {
        case ASYNC_COMPLETE:
            fbupdatenotify_delete(descr);
            proxy_fb->fb_update_head = proxy_fb->fb_update_tail = NULL;
            return;
        case ASYNC_ERROR:
            fbupdatenotify_delete(descr);
            proxy_fb->fb_update_head = proxy_fb->fb_update_tail = NULL;
            return;
        case ASYNC_NEED_MORE:
            // Update transfer will eventually complete in _proxyFb_io_fun
            return;
    }
}

int
proxyFb_get_bits_per_pixel(ProxyFramebuffer* proxy_fb)
{
    if (proxy_fb == NULL || proxy_fb->ds == NULL)
        return -1;

    return proxy_fb->ds->surface->pf.bits_per_pixel;
}
