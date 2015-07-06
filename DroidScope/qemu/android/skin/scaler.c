/* Copyright (C) 2008 The Android Open Source Project
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
#include "android/skin/scaler.h"
#include <stdint.h>
#include <math.h>

struct SkinScaler {
    double  scale;
    double  xdisp, ydisp;
    double  invscale;
    int     valid;
};

static SkinScaler  _scaler0;

SkinScaler*
skin_scaler_create( void )
{
    _scaler0.scale    = 1.0;
    _scaler0.xdisp    = 0.0;
    _scaler0.ydisp    = 0.0;
    _scaler0.invscale = 1.0;
    return &_scaler0;
}

/* change the scale of a given scaler. returns 0 on success, or -1 in case of
 * problem (unsupported scale) */
int
skin_scaler_set( SkinScaler*  scaler, double  scale, double xdisp, double ydisp )
{
    /* right now, we only support scales in the 0.5 .. 1.0 range */
    if (scale < 0.1)
        scale = 0.1;
    else if (scale > 6.0)
        scale = 6.0;

    scaler->scale    = scale;
    scaler->xdisp    = xdisp;
    scaler->ydisp    = ydisp;
    scaler->invscale = 1/scale;
    scaler->valid    = 1;

    return 0;
}

void
skin_scaler_free( SkinScaler*  scaler )
{
    scaler=scaler;
}

typedef struct {
    SDL_Rect    rd;         /* destination rectangle */
    int         sx, sy;     /* source start position in 16.16 format */
    int         ix, iy;     /* source increments in 16.16 format */
    int         src_pitch;
    int         src_w;
    int         src_h;
    int         dst_pitch;
    uint8_t*    dst_line;
    uint8_t*    src_line;
    double      scale;
} ScaleOp;


#define  ARGB_SCALE_GENERIC       scale_generic
#define  ARGB_SCALE_05_TO_10      scale_05_to_10
#define  ARGB_SCALE_UP_BILINEAR   scale_up_bilinear
/* #define  ARGB_SCALE_UP_QUICK_4x4  scale_up_quick_4x4 UNUSED */

#include "android/skin/argb.h"


void
skin_scaler_get_scaled_rect( SkinScaler*  scaler,
                             SkinRect*    srect,
                             SkinRect*    drect )
{
    int sx = srect->pos.x;
    int sy = srect->pos.y;
    int sw = srect->size.w;
    int sh = srect->size.h;
    double scale = scaler->scale;

    if (!scaler->valid) {
        drect[0] = srect[0];
        return;
    }

    drect->pos.x = (int)(sx * scale + scaler->xdisp);
    drect->pos.y = (int)(sy * scale + scaler->ydisp);
    drect->size.w = (int)(ceil((sx + sw) * scale + scaler->xdisp)) - drect->pos.x;
    drect->size.h = (int)(ceil((sy + sh) * scale + scaler->ydisp)) - drect->pos.y;
}

void
skin_scaler_scale( SkinScaler*   scaler,
                   SDL_Surface*  dst_surface,
                   SDL_Surface*  src_surface,
                   int           sx,
                   int           sy,
                   int           sw,
                   int           sh )
{
    ScaleOp   op;

    if ( !scaler->valid )
        return;

    SDL_LockSurface( src_surface );
    SDL_LockSurface( dst_surface );
    {
        op.scale     = scaler->scale;
        op.src_pitch = src_surface->pitch;
        op.src_line  = src_surface->pixels;
        op.src_w     = src_surface->w;
        op.src_h     = src_surface->h;
        op.dst_pitch = dst_surface->pitch;
        op.dst_line  = dst_surface->pixels;

        /* compute the destination rectangle */
        op.rd.x = (int)(sx * scaler->scale + scaler->xdisp);
        op.rd.y = (int)(sy * scaler->scale + scaler->ydisp);
        op.rd.w = (int)(ceil((sx + sw) * scaler->scale + scaler->xdisp)) - op.rd.x;
        op.rd.h = (int)(ceil((sy + sh) * scaler->scale + scaler->ydisp)) - op.rd.y;

        /* compute the starting source position in 16.16 format
         * and the corresponding increments */
        op.sx = (int)((op.rd.x - scaler->xdisp) * scaler->invscale * 65536);
        op.sy = (int)((op.rd.y - scaler->ydisp) * scaler->invscale * 65536);

        op.ix = (int)( scaler->invscale * 65536 );
        op.iy = op.ix;

        op.dst_line += op.rd.x*4 + op.rd.y*op.dst_pitch;

        if (op.scale >= 0.5 && op.scale <= 1.0)
            scale_05_to_10( &op );
        else if (op.scale > 1.0)
            scale_up_bilinear( &op );
        else
            scale_generic( &op );
    }

    // The optimized scale functions in argb.h assume the destination is ARGB.
    // If that's not the case, do a channel reorder now.
    if (dst_surface->format->Rshift != 16 ||
        dst_surface->format->Gshift !=  8 ||
        dst_surface->format->Bshift !=  0)
    {
        uint32_t rshift = dst_surface->format->Rshift;
        uint32_t gshift = dst_surface->format->Gshift;
        uint32_t bshift = dst_surface->format->Bshift;
        uint32_t ashift = dst_surface->format->Ashift;
        uint32_t amask  = dst_surface->format->Amask; // may be 0x00
        int x, y;

        for (y = 0; y < op.rd.h; y++)
        {
            uint32_t* line = (uint32_t*)(op.dst_line + y*op.dst_pitch);
            for (x = 0; x < op.rd.w; x++) {
                uint32_t r = (line[x] & 0x00ff0000) >> 16;
                uint32_t g = (line[x] & 0x0000ff00) >>  8;
                uint32_t b = (line[x] & 0x000000ff) >>  0;
                uint32_t a = (line[x] & 0xff000000) >> 24;
                line[x] = (r << rshift) | (g << gshift) | (b << bshift) |
                          ((a << ashift) & amask);
            }
        }
    }

    SDL_UnlockSurface( dst_surface );
    SDL_UnlockSurface( src_surface );

    SDL_UpdateRects( dst_surface, 1, &op.rd );
}
