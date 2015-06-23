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
#ifndef _ANDROID_UTILS_JPEG_COMPRESS_H
#define _ANDROID_UTILS_JPEG_COMPRESS_H

/*
 * Contains declaration of utility routines that compress an RGB bitmap into
 * a JPEG image.
 *
 * NOTE: This code uses a jpeglib library located in distrib/jpeg-6b. It's a
 * 3-rd party library that uses its own type definitions that are different from
 * the ones that are use elsewhere in the emulator code. For instance, in the
 * emulator built for Windows, sizeof(bool) = 1, while in the jpeglib sizeof(bool) = 4.
 * So, to simplify dealing with these issues, all the code that uses jpeglib should
 * be compiled separately, and should include only headers that are used to compile
 * jpeglib.
 */


/* Declares descriptor for a JPEG compression. */
typedef struct AJPEGDesc AJPEGDesc;

/* Creates a descriptor that will be used for compression.
 * Param:
 *  header_size - Number of bytes to allocate for a custom header that should
 *      preceed the actual JPEG buffer. This is useful when sending JPEG
 *      somewhere else along with some extra data about the compressed image.
 *  cunk_size - Number of bytes to increment the compressed buffer with each time
 *      compressor requests more memory.
 * Return:
 *  Initialized compression descriptor.
 */
extern AJPEGDesc* jpeg_compressor_create(int header_size, int chunk_size);

/* Destroys compressor descriptor.
 * Param:
 *  dsc - Compressin descriptor, obtained with jpeg_compressor_create.
 */
extern void jpeg_compressor_destroy(AJPEGDesc* dsc);

/* Returns compressed data size.
 * Param:
 *  dsc - Compression descriptor, obtained with jpeg_compressor_create.
 * Return:
 *  Compressed data size.
 */
extern int jpeg_compressor_get_jpeg_size(const AJPEGDesc* dsc);

/* Returns compressed buffer.
 * Param:
 *  dsc - Compression descriptor, obtained with jpeg_compressor_create.
 * Return:
 *  Compressed buffer. NOTE: if 'header_size' parameter passed to the jpeg_compressor_create
 *  for this descriptor was not zero, this routine returns a pointer to the custom
 *  header. Compressed data follows immediately after that header.
 */
extern void* jpeg_compressor_get_buffer(const AJPEGDesc* dsc);

/* Returns size of the custom header placed before the compressed data.
 * Param:
 *  dsc - Compression descriptor, obtained with jpeg_compressor_create.
 * Return:
 *  Size of the custom header placed before the compressed data.
 */
extern int jpeg_compressor_get_header_size(const AJPEGDesc* dsc);

/* Compresses a framebuffer region into JPEG image.
 * Param:
 *  dsc - Compression descriptor, obtained with jpeg_compressor_create.
 *  x, y, w, h - Coordinates and sizes of framebuffer region to compress.
 *  num_lines - Number of lines in the framebuffer (true height).
 *  bpp - Number of bytes per pixel in the framebuffer.
 *  bpl - Number of bytes per line in the framebuffer.
 *  fb - Beginning of the framebuffer.
 *  jpeg_quality JPEG compression quality. A number from 1 to 100. Note that
 *      value 10 provides pretty decent image for the purpose of multi-touch
 *      emulation.
 *  ydir - Indicates direction in which lines are arranged in the framebuffer. If
 *      this value is negative, lines are arranged in bottom-up format (i.e. the
 *      bottom line is at the beginning of the buffer).
 */
extern void jpeg_compressor_compress_fb(AJPEGDesc* dsc,
                                        int x, int y, int w, int h,
                                        int num_lines,
                                        int bpp, int bpl,
                                        const uint8_t* fb,
                                        int jpeg_quality,
                                        int ydir);

#endif  /* _ANDROID_UTILS_JPEG_COMPRESS_H */
