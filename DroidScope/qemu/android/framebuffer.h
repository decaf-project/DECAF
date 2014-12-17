/* Copyright (C) 2007-2008 The Android Open Source Project
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
#ifndef _ANDROID_FRAMEBUFFER_H_
#define _ANDROID_FRAMEBUFFER_H_

/* A simple abstract interface to framebuffer displays. this is used to
 * de-couple hardware emulation from final display.
 *
 * Each QFrameBuffer object holds a pixel buffer that is shared between
 * one 'Producer' and one or more 'Clients'
 *
 * The Producer is in charge of updating the pixel buffer from the state
 * of the emulated VRAM. A Client listens to updates to the pixel buffer,
 * sent from the producer through qframebuffer_update()/_rotate() and
 * displays them.
 *
 * note the 'rotation' field: it can take values 0, 1, 2 or 3 and corresponds
 * to a rotation that must be performed to the pixels stored in the framebuffer
 * *before* displaying them a value of 1 corresponds to a rotation of
 * 90 clockwise-degrees, when the framebuffer is rotated 90 or 270 degrees,
 * its width/height are swapped automatically
 *
 * phys_width_mm and phys_height_mm are physical dimensions expressed
 * in millimeters
 *
 * More about the client/producer relationships below.
 */
typedef struct QFrameBuffer   QFrameBuffer;


typedef enum {
    QFRAME_BUFFER_NONE   = 0,
    QFRAME_BUFFER_RGB565 = 1,
    QFRAME_BUFFER_RGBX_8888 = 2,
    QFRAME_BUFFER_MAX          /* do not remove */
} QFrameBufferFormat;

struct QFrameBuffer {
    int                 width;        /* width in pixels */
    int                 height;       /* height in pixels */
    int                 pitch;        /* bytes per line */
    int                 bits_per_pixel; /* bits per pixel */
    int                 bytes_per_pixel;    /* bytes per pixel */
    int                 rotation;     /* rotation to be applied when displaying */
    QFrameBufferFormat  format;
    void*               pixels;       /* pixel buffer */

    int                 phys_width_mm;
    int                 phys_height_mm;

    /* extra data that is handled by the framebuffer implementation */
    void*               extra;

};

/* the default dpi resolution of a typical framebuffer. this is an average
 * between various prototypes being used during the development of the
 * Android system...
 */
#define  DEFAULT_FRAMEBUFFER_DPI   165


/* initialize a framebuffer object and allocate its pixel buffer */
/* this computes phys_width_mm and phys_height_mm assuming a 165 dpi screen */
/* returns -1 in case of error, 0 otherwise */
extern int
qframebuffer_init( QFrameBuffer*       qfbuff,
                   int                 width,
                   int                 height,
                   int                 rotation,
                   QFrameBufferFormat  format );

/* recompute phys_width_mm and phys_height_mm according to the emulated
 * screen DPI settings */
extern void
qframebuffer_set_dpi( QFrameBuffer*   qfbuff,
                      int             x_dpi,
                      int             y_dpi );

/* alternative to qframebuffer_set_dpi where one can set the physical
 * dimensions directly in millimeters. for the record 1 inch = 25.4 mm */
extern void
qframebuffer_set_mm( QFrameBuffer*   qfbuff,
                     int             width_mm,
                     int             height_mm );

/* the Client::Update method is called to instruct a client that a given
 * rectangle of the framebuffer pixels was updated and needs to be
 * redrawn.
 */
typedef void (*QFrameBufferUpdateFunc)( void*  opaque, int  x, int  y,
                                                       int  w, int  h );

/* the Client::Rotate method is called to instruct the client that a
 * framebuffer's internal rotation has changed. This is the rotation
 * that must be applied before displaying the pixels.
 *
 * Note that it is assumed that all framebuffer pixels have changed too
 * so the client should call its Update method as well.
 */
typedef void (*QFrameBufferRotateFunc)( void*  opaque, int  rotation );

/* the Client::Poll method is called periodically to poll for input
 * events and act on them. Putting this here is not 100% pure but
 * make things simpler due to QEMU's weird architecture where the
 * GUI timer drivers event polling.
 */
typedef void (*QFrameBufferPollFunc)( void* opaque );

/* the Client::Done func tells a client that a framebuffer object was freed.
 * no more reference to its pixels should be done.
 */
typedef void (*QFrameBufferDoneFunc)  ( void*  opaque );

/* add one client to a given framebuffer.
 * the current implementation only allows one client per frame-buffer,
 * but we could allow more for various reasons (e.g. displaying the
 * framebuffer + dispatching it through VNC at the same time)
 */
extern void
qframebuffer_add_client( QFrameBuffer*           qfbuff,
                         void*                   fb_opaque,
                         QFrameBufferUpdateFunc  fb_update,
                         QFrameBufferRotateFunc  fb_rotate,
                         QFrameBufferPollFunc    fb_poll,
                         QFrameBufferDoneFunc    fb_done );

/* Producer::CheckUpdate is called to let the producer check the
 * VRAM state (e.g. VRAM dirty pages) to see if anything changed since the
 * last call to the method. When true, the method should call either
 * qframebuffer_update() or qframebuffer_rotate() with the appropriate values.
 */
typedef void (*QFrameBufferCheckUpdateFunc)( void*  opaque );

/* Producer::Invalidate tells the producer that the next call to
 * CheckUpdate should act as if the whole content of VRAM had changed.
 * this is normally done to force client initialization/refreshes.
 */
typedef void (*QFrameBufferInvalidateFunc) ( void*  opaque );

/* the Producer::Detach method is used to tell the producer that the
 * underlying QFrameBuffer object is about to be de-allocated.
 */
typedef void (*QFrameBufferDetachFunc)     ( void*  opaque );

/* set the producer of a given framebuffer */
extern void
qframebuffer_set_producer( QFrameBuffer*                qfbuff,
                           void*                        opaque,
                           QFrameBufferCheckUpdateFunc  fb_check,
                           QFrameBufferInvalidateFunc   fb_invalidate,
                           QFrameBufferDetachFunc       fb_detach );

/* tell a client that a rectangle region has been updated in the framebuffer
 * pixel buffer this is typically called from a Producer::CheckUpdate method
 */
extern void
qframebuffer_update( QFrameBuffer*  qfbuff, int  x, int  y, int  w, int  h );

/* rotate the framebuffer (may swap width/height), and tell all clients.
 * Should be called from a Producer::CheckUpdate method
 */
extern void
qframebuffer_rotate( QFrameBuffer*  qfbuff, int  rotation );

/* this function is used to poll a framebuffer's client for input
 * events. Should be called either explicitely, or through qframebuffer_pulse()
 * periodically.
 */
extern void
qframebuffer_poll( QFrameBuffer* qfbuff );

/* finalize a framebuffer, release its pixel buffer. Should be called
 * from the framebuffer object's owner
 */
extern void
qframebuffer_done( QFrameBuffer*   qfbuff );


/* this is called repeatedly by the emulator. for each registered framebuffer,
 * call its producer's CheckUpdate method, if any.
 */
extern void
qframebuffer_check_updates( void );

/* call this function periodically to force a poll on all franebuffers
 */
extern void
qframebuffer_pulse( void );

/* this is called by the emulator. for each registered framebuffer, call
 * its producer's Invalidate method, if any
 */
extern void
qframebuffer_invalidate_all( void );

/*
 * to completely separate the implementation of clients, producers, and skins,
 * we use a simple global FIFO list of QFrameBuffer objects.
 *
 * qframebuffer_fifo_add() is typically called by the emulator initialization
 * depending on the emulated device's configuration
 *
 * qframebuffer_fifo_get() is typically called by a hardware framebuffer
 * emulation.
 */

/* add a new constructed frame buffer object to our global list */
extern void
qframebuffer_fifo_add( QFrameBuffer*  qfbuff );

/* retrieve a frame buffer object from the global FIFO list */
extern QFrameBuffer*
qframebuffer_fifo_get( void );

/* */

#endif /* _ANDROID_FRAMEBUFFER_H_ */
