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
#include "android/framebuffer.h"
#include <memory.h>
#include <stdlib.h>

typedef struct {
    /* client fields, these correspond to code that waits for updates before displaying them */
    /* at the moment, only one client is supported */
    void*                        fb_opaque;
    QFrameBufferUpdateFunc       fb_update;
    QFrameBufferRotateFunc       fb_rotate;
    QFrameBufferPollFunc         fb_poll;
    QFrameBufferDoneFunc         fb_done;

    void*                        pr_opaque;
    QFrameBufferCheckUpdateFunc  pr_check;
    QFrameBufferInvalidateFunc   pr_invalidate;
    QFrameBufferDetachFunc       pr_detach;

} QFrameBufferExtra;


static int
_get_pitch( int  width, QFrameBufferFormat  format )
{

    switch (format) {
        case QFRAME_BUFFER_RGB565:
            return width*2;
        case QFRAME_BUFFER_RGBX_8888:
            return width*4;
        default:
            return -1;
    }
}

static int
_get_bits_per_pixel(QFrameBufferFormat  format)
{

    switch (format) {
        case QFRAME_BUFFER_RGB565:
            return 16;
        case QFRAME_BUFFER_RGBX_8888:
            return 32;
        default:
            return -1;
    }
}

static int
_get_bytes_per_pixel(QFrameBufferFormat  format)
{

    switch (format) {
        case QFRAME_BUFFER_RGB565:
            return 2;
        case QFRAME_BUFFER_RGBX_8888:
            return 4;
        default:
            return -1;
    }
}

int
qframebuffer_init( QFrameBuffer*       qfbuff,
                   int                 width,
                   int                 height,
                   int                 rotation,
                   QFrameBufferFormat  format )
{
    int   pitch, bytes_per_pixel, bits_per_pixel;

    rotation &= 3;

    if (!qfbuff || width < 0 || height < 0)
        return -1;

    pitch = _get_pitch( width, format );
    if (pitch < 0)
        return -1;

    bits_per_pixel = _get_bits_per_pixel(format);
    if (bits_per_pixel < 0)
        return -1;

    bytes_per_pixel = _get_bytes_per_pixel(format);
    if (bytes_per_pixel < 0)
        return -1;

    memset( qfbuff, 0, sizeof(*qfbuff) );

    qfbuff->extra = calloc( 1, sizeof(QFrameBufferExtra) );
    if (qfbuff->extra == NULL)
        return -1;

    qfbuff->pixels = calloc( pitch, height );
    if (qfbuff->pixels == NULL && (height > 0 && pitch > 0)) {
        free( qfbuff->extra );
        return -1;
    }

    qfbuff->width  = width;
    qfbuff->height = height;
    qfbuff->pitch  = pitch;
    qfbuff->format = format;
    qfbuff->bits_per_pixel = bits_per_pixel;
    qfbuff->bytes_per_pixel = bytes_per_pixel;

    qframebuffer_set_dpi( qfbuff, DEFAULT_FRAMEBUFFER_DPI, DEFAULT_FRAMEBUFFER_DPI );
    return 0;
}


void
qframebuffer_set_dpi( QFrameBuffer*   qfbuff,
                      int             x_dpi,
                      int             y_dpi )
{
    /* dpi = dots / inch
    ** inch = dots / dpi
    ** mm / 25.4 = dots / dpi
    ** mm = (dots * 25.4)/dpi
    */
    qfbuff->phys_width_mm  = (int)(0.5 + 25.4 * qfbuff->width  / x_dpi);
    qfbuff->phys_height_mm = (int)(0.5 + 25.4 * qfbuff->height / y_dpi);
}

/* alternative to qframebuffer_set_dpi where one can set the physical dimensions directly */
/* in millimeters. for the record 1 inch = 25.4 mm */
void
qframebuffer_set_mm( QFrameBuffer*   qfbuff,
                     int             width_mm,
                     int             height_mm )
{
    qfbuff->phys_width_mm  = width_mm;
    qfbuff->phys_height_mm = height_mm;
}

void
qframebuffer_update( QFrameBuffer*  qfbuff, int  x, int  y, int  w, int  h )
{
    QFrameBufferExtra*  extra = qfbuff->extra;

    if (extra->fb_update)
        extra->fb_update( extra->fb_opaque, x, y, w, h );
}


void
qframebuffer_add_client( QFrameBuffer*           qfbuff,
                         void*                   fb_opaque,
                         QFrameBufferUpdateFunc  fb_update,
                         QFrameBufferRotateFunc  fb_rotate,
                         QFrameBufferPollFunc    fb_poll,
                         QFrameBufferDoneFunc    fb_done )
{
    QFrameBufferExtra*  extra = qfbuff->extra;

    extra->fb_opaque = fb_opaque;
    extra->fb_update = fb_update;
    extra->fb_rotate = fb_rotate;
    extra->fb_poll   = fb_poll;
    extra->fb_done   = fb_done;
}

void
qframebuffer_set_producer( QFrameBuffer*                qfbuff,
                           void*                        opaque,
                           QFrameBufferCheckUpdateFunc  pr_check,
                           QFrameBufferInvalidateFunc   pr_invalidate,
                           QFrameBufferDetachFunc       pr_detach )
{
    QFrameBufferExtra*  extra = qfbuff->extra;

    extra->pr_opaque     = opaque;
    extra->pr_check      = pr_check;
    extra->pr_invalidate = pr_invalidate;
    extra->pr_detach     = pr_detach;
}


void
qframebuffer_rotate( QFrameBuffer*  qfbuff, int  rotation )
{
    QFrameBufferExtra*  extra = qfbuff->extra;

    if ((rotation ^ qfbuff->rotation) & 1) {
        /* swap width and height if new rotation requires it */
        int  temp = qfbuff->width;
        qfbuff->width  = qfbuff->height;
        qfbuff->height = temp;
        qfbuff->pitch  = _get_pitch( qfbuff->width, qfbuff->format );

        temp = qfbuff->phys_width_mm;
        qfbuff->phys_width_mm  = qfbuff->phys_height_mm;
        qfbuff->phys_height_mm = temp;
    }
    qfbuff->rotation = rotation;

    if (extra->fb_rotate)
        extra->fb_rotate( extra->fb_opaque, rotation );
}

void
qframebuffer_poll( QFrameBuffer* qfbuff )
{
    QFrameBufferExtra*  extra = qfbuff->extra;

    if (extra && extra->fb_poll)
        extra->fb_poll( extra->fb_opaque );
}


extern void
qframebuffer_done( QFrameBuffer*   qfbuff )
{
    QFrameBufferExtra*  extra = qfbuff->extra;

    if (extra) {
        if (extra->pr_detach)
            extra->pr_detach( extra->pr_opaque );

        if (extra->fb_done)
            extra->fb_done( extra->fb_opaque );
    }

    free( qfbuff->pixels );
    free( qfbuff->extra );
    memset( qfbuff, 0, sizeof(*qfbuff) );
}


#define  MAX_FRAME_BUFFERS  8

static QFrameBuffer* framebuffer_fifo[ MAX_FRAME_BUFFERS ];
static int           framebuffer_fifo_rpos;
static int           framebuffer_fifo_count;

void
qframebuffer_fifo_add( QFrameBuffer*  qfbuff )
{
    if (framebuffer_fifo_count >= MAX_FRAME_BUFFERS)
        return;

    framebuffer_fifo[ framebuffer_fifo_count++ ] = qfbuff;
}


QFrameBuffer*
qframebuffer_fifo_get( void )
{
    if (framebuffer_fifo_rpos >= framebuffer_fifo_count)
        return NULL;

    return framebuffer_fifo[ framebuffer_fifo_rpos++ ];
}


void
qframebuffer_check_updates( void )
{
    int  nn;
    for (nn = 0; nn < framebuffer_fifo_count; nn++) {
        QFrameBuffer*       q     = framebuffer_fifo[nn];
        QFrameBufferExtra*  extra = q->extra;

        if (extra->pr_check)
            extra->pr_check( extra->pr_opaque );
    }
}

void
qframebuffer_pulse( void )
{
    int  nn;
    for (nn = 0; nn < framebuffer_fifo_count; nn++) {
        qframebuffer_poll(framebuffer_fifo[nn]);
    }
}

void
qframebuffer_invalidate_all( void )
{
    int  nn;
    for (nn = 0; nn < framebuffer_fifo_count; nn++) {
        QFrameBuffer*       q     = framebuffer_fifo[nn];
        QFrameBufferExtra*  extra = q->extra;

        if (extra->pr_invalidate)
            extra->pr_invalidate( extra->pr_opaque );
    }
}
