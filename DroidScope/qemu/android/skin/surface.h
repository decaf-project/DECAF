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
#ifndef _ANDROID_SKIN_SURFACE_H
#define _ANDROID_SKIN_SURFACE_H

#include "android/skin/region.h"
#include <stdint.h>

/* a SkinSurface models a 32-bit ARGB pixel image that can be blitted to or from
 */
typedef struct SkinSurface  SkinSurface;

/* increment surface's reference count */
extern SkinSurface*  skin_surface_ref( SkinSurface*  surface );

/* decrement a surface's reference count. takes the surface's address as parameter.
   it will be set to NULL on exit */
extern void          skin_surface_unrefp( SkinSurface*  *psurface );

/* sets a callback that will be called when the surface is destroyed.
 * used NULL for done_func to disable it
 */
typedef void (*SkinSurfaceDoneFunc)( void*  user );

extern void  skin_surface_set_done( SkinSurface*  s, SkinSurfaceDoneFunc  done_func, void*  done_user );


/* there are two kinds of surfaces:

   - fast surfaces, whose content can be placed in video memory or
     RLE-compressed for faster blitting and blending. the pixel format
     used internally might be something very different from ARGB32.

   - slow surfaces, whose content (pixel buffer) can be accessed and modified
     with _lock()/_unlock() but may be blitted far slower since they reside in
     system memory.
*/

/* create a 'fast' surface that contains a copy of an input ARGB32 pixmap */
extern SkinSurface*  skin_surface_create_fast( int  w, int  h );

/* create an empty 'slow' surface containing an ARGB32 pixmap */
extern SkinSurface*  skin_surface_create_slow( int  w, int  h );

/* create a 'slow' surface from a given pixel buffer. if 'do_copy' is TRUE, then
 * the content of 'pixels' is copied into a heap-allocated buffer. otherwise
 * the data will be used directly.
 */
extern SkinSurface*  skin_surface_create_argb32_from(
                            int                  w,
                            int                  h,
                            int                  pitch,
                            uint32_t*            pixels,
                            int                  do_copy );

/* surface pixels information for slow surfaces */
typedef struct {
    int         w;
    int         h;
    int         pitch;
    uint32_t*   pixels;
} SkinSurfacePixels;

/* lock a slow surface, and returns its pixel information.
   returns 0 in case of success, -1 otherwise */
extern int     skin_surface_lock  ( SkinSurface*  s, SkinSurfacePixels  *pix );

/* unlock a slow surface that was previously locked */
extern void    skin_surface_unlock( SkinSurface*  s );

/* list of composition operators for the blit routine */
typedef enum {
    SKIN_BLIT_COPY = 0,
    SKIN_BLIT_SRCOVER,
    SKIN_BLIT_DSTOVER,
} SkinBlitOp;


/* blit a surface into another one */
extern void    skin_surface_blit( SkinSurface*  dst,
                                  SkinPos*      dst_pos,
                                  SkinSurface*  src,
                                  SkinRect*     src_rect,
                                  SkinBlitOp    blitop );

/* blit a colored rectangle into a destination surface */
extern void    skin_surface_fill( SkinSurface*  dst,
                                  SkinRect*     rect,
                                  uint32_t      argb_premul,
                                  SkinBlitOp    blitop );

#endif /* _ANDROID_SKIN_SURFACE_H */
