/* Copyright (C) 2011 The Android Open Source Project
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

#include <stdint.h>
#include "jinclude.h"
#include "jpeglib.h"
#include "jpeg-compress.h"
#include "panic.h"

/* Implements JPEG destination manager's init_destination routine. */
static void _on_init_destination(j_compress_ptr cinfo);
/* Implements JPEG destination manager's empty_output_buffer routine. */
static boolean _on_empty_output_buffer(j_compress_ptr cinfo);
/* Implements JPEG destination manager's term_destination routine. */
static void _on_term_destination(j_compress_ptr cinfo);

/* JPEG compression descriptor. */
struct AJPEGDesc {
    /* Common JPEG compression destination manager header. */
    struct jpeg_destination_mgr     common;
    /* Buffer where to save compressed output. */
    uint8_t*                        jpeg_buf;
    /* Byte size of the 'jpeg_buf' */
    int                             size;
    /* Chunk size to increment the 'jpeg_buf' with on each allocation request. */
    int                             chunk_size;
    /* Size of the header to put in front of the compressed data. */
    int                             header_size;
};

/********************************************************************************
 *                      jpeglib callbacks.
 *******************************************************************************/

/* Implements JPEG destination manager's init_destination routine. */
static void
_on_init_destination(j_compress_ptr cinfo)
{
    AJPEGDesc* const dst = (AJPEGDesc*)cinfo->dest;
    if (dst->jpeg_buf == NULL) {
        /* This is the first time our destination manager is initialized.
         * Allocate minimal buffer. */
        dst->size = dst->chunk_size;
        dst->jpeg_buf = malloc(dst->size);
        if (dst->jpeg_buf == NULL) {
            APANIC("Unable to allocate %d bytes for JPEG compression", dst->size);
        }
    }
    /* Initialize common header with entire destination buffer. */
    dst->common.next_output_byte = dst->jpeg_buf + dst->header_size;
    dst->common.free_in_buffer = dst->size - dst->header_size;
}

/* Implements JPEG destination manager's empty_output_buffer routine.
 * Name is a bit misleading here. This routine is called by the compressor when
 * output buffer doesn't have enough free space to contain the next chunk of the
 * compressed data. So, here we should reallocate the output buffer, rather than
 * "empty" it.
 */
static boolean
_on_empty_output_buffer(j_compress_ptr cinfo)
{
    AJPEGDesc* const dst = (AJPEGDesc*)cinfo->dest;
    /* Save already compressed data size. */
    const int accumulated = jpeg_compressor_get_jpeg_size(dst);

    /* Reallocate output buffer. */
    dst->size += dst->chunk_size;
    dst->jpeg_buf = realloc(dst->jpeg_buf, dst->size);
    if (dst->jpeg_buf == NULL) {
        APANIC("Unable to allocate %d bytes for JPEG compression", dst->size);
    }

    /* Update common header. */
    dst->common.next_output_byte = dst->jpeg_buf + accumulated + dst->header_size;
    dst->common.free_in_buffer = dst->size - accumulated - dst->header_size;

    return TRUE;
}

/* Implements JPEG destination manager's term_destination routine.
 * We don't do anything here. All the cleanup will be performed when the user
 * calls jpeg_compressor_destroy. */
static void
_on_term_destination(j_compress_ptr cinfo)
{
}

/********************************************************************************
 *                      JPEG compressor API.
 *******************************************************************************/

AJPEGDesc*
jpeg_compressor_create(int header_size, int chunk_size)
{
    AJPEGDesc* dsc = (AJPEGDesc*)malloc(sizeof(AJPEGDesc));
    if (dsc == NULL) {
        APANIC("Unable to allocate JPEG compression descriptor.");
    }

    dsc->common.next_output_byte    = NULL;
    dsc->common.free_in_buffer      = 0;
    dsc->common.init_destination    = _on_init_destination;
    dsc->common.empty_output_buffer = _on_empty_output_buffer;
    dsc->common.term_destination    = _on_term_destination;
    dsc->jpeg_buf                   = NULL;
    dsc->size                       = 0;
    dsc->chunk_size                 = chunk_size;
    dsc->header_size                = header_size;
    return dsc;
}

void
jpeg_compressor_destroy(AJPEGDesc* dsc)
{
    if (dsc != NULL) {
        if (dsc->jpeg_buf != NULL) {
            free(dsc->jpeg_buf);
        }
        free(dsc);
    }
}

int
jpeg_compressor_get_jpeg_size(const AJPEGDesc* dsc)
{
    return (dsc->jpeg_buf == NULL) ? 0 :
        (uint8_t*)dsc->common.next_output_byte - dsc->jpeg_buf - dsc->header_size;
}

void*
jpeg_compressor_get_buffer(const AJPEGDesc* dsc)
{
     return dsc->jpeg_buf;
}

int
jpeg_compressor_get_header_size(const AJPEGDesc* dsc)
{
     return dsc->header_size;
}

void
jpeg_compressor_compress_fb(AJPEGDesc* dsc,
                            int x, int y, int w, int h, int num_lines,
                            int bpp, int bpl,
                            const uint8_t* fb,
                            int jpeg_quality,
                            int ydir){
    struct jpeg_compress_struct cinfo = {0};
    struct jpeg_error_mgr err_mgr;
    const int x_shift = x * bpp;

    /*
     * Initialize compressin information structure, and start compression
     */

    cinfo.err = jpeg_std_error(&err_mgr);
    jpeg_create_compress(&cinfo);
    cinfo.dest = &dsc->common;
    cinfo.image_width = w;
    cinfo.image_height = h;

    /* Decode framebuffer's pixel format. There can be only three:
     * - RGB565,
     * - RGBA8888,
     * - RGBX8888 */
    if (bpp == 2) {
        /* This is RGB565 - most commonly used pixel format for framebuffer. */
        cinfo.input_components = 2;
        cinfo.in_color_space = JCS_RGB_565;
    } else {
        /* RGBA8888, or RGBX8888 - makes no difference here. */
        cinfo.input_components = 4;
        cinfo.in_color_space = JCS_RGBA_8888;
    }
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, jpeg_quality, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    /* Line by line compress the region. */
    if (ydir >= 0) {
        while (cinfo.next_scanline < cinfo.image_height) {
            JSAMPROW rgb = (JSAMPROW)(fb + (cinfo.next_scanline + y) * bpl + x_shift);
            jpeg_write_scanlines(&cinfo, (JSAMPARRAY)&rgb, 1);
        }
    } else {
        const int y_shift = num_lines - y - 1;
        while (cinfo.next_scanline < cinfo.image_height) {
            JSAMPROW rgb = (JSAMPROW)(fb + (y_shift - cinfo.next_scanline) * bpl + x_shift);
            jpeg_write_scanlines(&cinfo, (JSAMPARRAY)&rgb, 1);
        }
    }

    /* Complete the compression. */
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
}
