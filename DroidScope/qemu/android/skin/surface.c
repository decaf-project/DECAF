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
#include "android/skin/surface.h"
#include "android/skin/argb.h"
#include <SDL.h>

#define  DEBUG  1

#if DEBUG
#include "android/utils/debug.h"
#define  D(...)   VERBOSE_PRINT(surface,__VA_ARGS__)
#else
#define  D(...)   ((void)0)
#endif

struct SkinSurface {
    int                  refcount;
    uint32_t*            pixels;
    SDL_Surface*         surface;
    SkinSurfaceDoneFunc  done_func;
    void*                done_user;
};

static void
skin_surface_free( SkinSurface*  s )
{
    if (s->done_func) {
        s->done_func( s->done_user );
        s->done_func = NULL;
    }
    if (s->surface) {
        SDL_FreeSurface(s->surface);
        s->surface = NULL;
    }
    free(s);
}

extern SkinSurface*
skin_surface_ref( SkinSurface*  surface )
{
    if (surface)
        surface->refcount += 1;
    return surface;
}

extern void
skin_surface_unrefp( SkinSurface*  *psurface )
{
    SkinSurface*  surf = *psurface;
    if (surf) {
        if (--surf->refcount <= 0)
            skin_surface_free(surf);
        *psurface = NULL;
    }
}


void
skin_surface_set_done( SkinSurface*  s, SkinSurfaceDoneFunc  done_func, void*  done_user )
{
    s->done_func = done_func;
    s->done_user = done_user;
}

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#  define  ARGB32_R_MASK  0xff000000
#  define  ARGB32_G_MASK  0x00ff0000
#  define  ARGB32_B_MASK  0x0000ff00
#  define  ARGB32_A_MASK  0x000000ff
#else
#  define  ARGB32_R_MASK  0x000000ff
#  define  ARGB32_G_MASK  0x0000ff00
#  define  ARGB32_B_MASK  0x00ff0000
#  define  ARGB32_A_MASK  0xff000000
#endif

static SDL_Surface*
_sdl_surface_create_rgb( int  width,
                         int  height,
                         int  depth,
                         int  flags )
{
   Uint32   rmask, gmask, bmask, amask;

    if (depth == 8) {
        rmask = gmask = bmask = 0;
        amask = 0xff;
    } else if (depth == 32) {
        rmask = ARGB32_R_MASK;
        gmask = ARGB32_G_MASK;
        bmask = ARGB32_B_MASK;
        amask = ARGB32_A_MASK;
    } else
        return NULL;

    return SDL_CreateRGBSurface( flags, width, height, depth,
                                 rmask, gmask, bmask, amask );
}


static SDL_Surface*
_sdl_surface_create_rgb_from( int   width,
                              int   height,
                              int   pitch,
                              void* pixels,
                              int   depth )
{
   Uint32   rmask, gmask, bmask, amask;

    if (depth == 8) {
        rmask = gmask = bmask = 0;
        amask = 0xff;
    } else if (depth == 32) {
        rmask = ARGB32_R_MASK;
        gmask = ARGB32_G_MASK;
        bmask = ARGB32_B_MASK;
        amask = ARGB32_A_MASK;
    } else
        return NULL;

    return SDL_CreateRGBSurfaceFrom( pixels, width, height, pitch, depth,
                                     rmask, gmask, bmask, amask );
}


static SkinSurface*
_skin_surface_create( SDL_Surface*  surface,
                      void*         pixels )
{
    SkinSurface*  s = malloc(sizeof(*s));
    if (s != NULL) {
        s->refcount = 1;
        s->pixels   = pixels;
        s->surface  = surface;
        s->done_func = NULL;
        s->done_user = NULL;
    }
    else {
        SDL_FreeSurface(surface);
        free(pixels);
        D( "not enough memory to allocate new skin surface !" );
    }
    return  s;
}


SkinSurface*
skin_surface_create_fast( int  w, int  h )
{
    SDL_Surface*  surface;

    surface = _sdl_surface_create_rgb( w, h, 32, SDL_HWSURFACE );
    if (surface == NULL) {
        surface = _sdl_surface_create_rgb( w, h, 32, SDL_SWSURFACE );
        if (surface == NULL) {
            D( "could not create fast %dx%d ARGB32 surface: %s",
               w, h, SDL_GetError() );
            return NULL;
        }
    }
    return _skin_surface_create( surface, NULL );
}


SkinSurface*
skin_surface_create_slow( int  w, int  h )
{
    SDL_Surface*  surface;

    surface = _sdl_surface_create_rgb( w, h, 32, SDL_SWSURFACE );
    if (surface == NULL) {
        D( "could not create slow %dx%d ARGB32 surface: %s",
            w, h, SDL_GetError() );
        return NULL;
    }
    return _skin_surface_create( surface, NULL );
}


SkinSurface*
skin_surface_create_argb32_from(
                        int                  w,
                        int                  h,
                        int                  pitch,
                        uint32_t*            pixels,
                        int                  do_copy )
{
    SDL_Surface*  surface;
    uint32_t*     pixcopy = NULL;

    if (do_copy) {
        size_t  size = h*pitch;
        pixcopy = malloc( size );
        if (pixcopy == NULL && size > 0) {
            D( "not enough memory to create %dx%d ARGB32 surface",
               w, h );
            return NULL;
        }
        memcpy( pixcopy, pixels, size );
    }

    surface = _sdl_surface_create_rgb_from( w, h, pitch,
                                            pixcopy ? pixcopy : pixels,
                                            32 );
    if (surface == NULL) {
        D( "could not create %dx%d slow ARGB32 surface: %s",
            w, h, SDL_GetError() );
        return NULL;
    }
    return _skin_surface_create( surface, pixcopy );
}




extern int
skin_surface_lock( SkinSurface*  s, SkinSurfacePixels  *pix )
{
    if (!s || !s->surface) {
        D( "error: trying to lock stale surface %p", s );
        return -1;
    }
    if ( SDL_LockSurface( s->surface ) != 0 ) {
        D( "could not lock surface %p: %s", s, SDL_GetError() );
        return -1;
    }
    pix->w      = s->surface->w;
    pix->h      = s->surface->h;
    pix->pitch  = s->surface->pitch;
    pix->pixels = s->surface->pixels;
    return 0;
}

/* unlock a slow surface that was previously locked */
extern void
skin_surface_unlock( SkinSurface*  s )
{
    if (s && s->surface)
        SDL_UnlockSurface( s->surface );
}


#if 0
static uint32_t
skin_surface_map_argb( SkinSurface*  s, uint32_t  c )
{
    if (s && s->surface) {
        return SDL_MapRGBA( s->surface->format,
                            ((c) >> 16) & 255,
                            ((c) >> 8) & 255,
                            ((c) & 255),
                            ((c) >> 24) & 255 );
    }
    return 0x00000000;
}
#endif

typedef struct {
    int   x;
    int   y;
    int   w;
    int   h;
    int   sx;
    int   sy;

    uint8_t*      dst_line;
    int           dst_pitch;
    SDL_Surface*  dst_lock;

    uint8_t*      src_line;
    int           src_pitch;
    SDL_Surface*  src_lock;
    uint32_t      src_color;

} SkinBlit;


static int
skin_blit_init_fill( SkinBlit*     blit,
                     SkinSurface*  dst,
                     SkinRect*     dst_rect,
                     uint32_t      color )
{
    int  x = dst_rect->pos.x;
    int  y = dst_rect->pos.y;
    int  w = dst_rect->size.w;
    int  h = dst_rect->size.h;
    int  delta;

    if (x < 0) {
        w += x;
        x  = 0;
    }
    delta = (x + w) - dst->surface->w;
    if (delta > 0)
        w -= delta;

    if (y < 0) {
        h += y;
        y  = 0;
    }
    delta = (y + h) - dst->surface->h;
    if (delta > 0)
        h -= delta;

    if (w <= 0 || h <= 0)
        return 0;

    blit->x = x;
    blit->y = y;
    blit->w = w;
    blit->h = h;

    if ( !SDL_LockSurface(dst->surface) )
        return 0;

    blit->dst_lock  = dst->surface;
    blit->dst_pitch = dst->surface->pitch;
    blit->dst_line  = dst->surface->pixels + y*blit->dst_pitch;

    blit->src_lock  = NULL;
    blit->src_color = color;

    return 1;
}

static int
skin_blit_init_blit( SkinBlit*     blit,
                     SkinSurface*  dst,
                     SkinPos*      dst_pos,
                     SkinSurface*  src,
                     SkinRect*     src_rect )
{
    int  x  = dst_pos->x;
    int  y  = dst_pos->y;
    int  sx = src_rect->pos.x;
    int  sy = src_rect->pos.y;
    int  w  = src_rect->size.w;
    int  h  = src_rect->size.h;
    int  delta;

    if (x < 0) {
        w  += x;
        sx -= x;
        x   = 0;
    }
    if (sx < 0) {
        w  += sx;
        x  -= sx;
        sx  = 0;
    }

    delta = (x + w) - dst->surface->w;
    if (delta > 0)
        w -= delta;

    delta = (sx + w) - src->surface->w;
    if (delta > 0)
        w -= delta;

    if (y < 0) {
        h  += y;
        sy += y;
        y   = 0;
    }
    if (sy < 0) {
        h  += sy;
        y  -= sy;
        sy  = 0;
    }
    delta = (y + h) - dst->surface->h;
    if (delta > 0)
        h -= delta;

    delta = (sy + h) - src->surface->h;

    if (w <= 0 || h <= 0)
        return 0;

    blit->x = x;
    blit->y = y;
    blit->w = w;
    blit->h = h;

    blit->sx = sx;
    blit->sy = sy;

    if ( !SDL_LockSurface(dst->surface) )
        return 0;

    blit->dst_lock  = dst->surface;
    blit->dst_pitch = dst->surface->pitch;
    blit->dst_line  = (uint8_t*) dst->surface->pixels + y*blit->dst_pitch;

    if ( !SDL_LockSurface(src->surface) ) {
        SDL_UnlockSurface(dst->surface);
        return 0;
    }

    blit->src_lock  = src->surface;
    blit->src_pitch = src->surface->pitch;
    blit->src_line  = (uint8_t*) src->surface->pixels + sy*blit->src_pitch;

    return 1;
}

static void
skin_blit_done( SkinBlit*  blit )
{
    if (blit->src_lock)
        SDL_UnlockSurface( blit->src_lock );
    if (blit->dst_lock)
        SDL_UnlockSurface( blit->dst_lock );
    ARGB_DONE;
}

typedef void (*SkinLineFillFunc)( uint32_t*  dst, uint32_t  color, int  len );
typedef void (*SkinLineBlitFunc)( uint32_t*  dst, const uint32_t*  src,  int  len );

static void
skin_line_fill_copy( uint32_t*  dst, uint32_t  color, int  len )
{
    uint32_t*  end = dst + len;

    while (dst + 4 <= end) {
        dst[0] = dst[1] = dst[2] = dst[3] = color;
        dst   += 4;
    }
    while (dst < end) {
        dst[0] = color;
        dst   += 1;
    }
}

static void
skin_line_fill_srcover( uint32_t*  dst, uint32_t  color, int  len )
{
    uint32_t*  end = dst + len;
    uint32_t   alpha = (color >> 24);

    if (alpha == 255)
    {
        skin_line_fill_copy(dst, color, len);
    }
    else
    {
        ARGB_DECL(src_c);
        ARGB_DECL_ZERO();

        alpha  = 255 - alpha;
        alpha += (alpha >> 7);

        ARGB_UNPACK(src_c,color);

        for ( ; dst < end; dst++ )
        {
            ARGB_DECL(dst_c);

            ARGB_READ(dst_c,dst);
            ARGB_MULSHIFT(dst_c,dst_c,alpha,8);
            ARGB_ADD(dst_c,src_c);
            ARGB_WRITE(dst_c,dst);
        }
    }
}

static void
skin_line_fill_dstover( uint32_t*  dst, uint32_t  color, int  len )
{
    uint32_t*  end = dst + len;
    ARGB_DECL(src_c);
    ARGB_DECL_ZERO();

    ARGB_UNPACK(src_c,color);

    for ( ; dst < end; dst++ )
    {
        ARGB_DECL(dst_c);
        ARGB_DECL(val);

        uint32_t   alpha;

        ARGB_READ(dst_c,dst);
        alpha = 256 - (dst[0] >> 24);
        ARGB_MULSHIFT(val,src_c,alpha,8);
        ARGB_ADD(val,dst_c);
        ARGB_WRITE(val,dst);
    }
}

extern void
skin_surface_fill( SkinSurface*  dst,
                   SkinRect*     rect,
                   uint32_t      argb_premul,
                   SkinBlitOp    blitop )
{
    SkinLineFillFunc  fill;
    SkinBlit          blit[1];

    switch (blitop) {
        case SKIN_BLIT_COPY:    fill = skin_line_fill_copy; break;
        case SKIN_BLIT_SRCOVER: fill = skin_line_fill_srcover; break;
        case SKIN_BLIT_DSTOVER: fill = skin_line_fill_dstover; break;
        default: return;
    }

    if ( skin_blit_init_fill( blit, dst, rect, argb_premul ) ) {
        uint8_t*   line  = blit->dst_line;
        int        pitch = blit->dst_pitch;
        uint8_t*   end   = line + pitch*blit->h;

        for ( ; line != end; line += pitch )
            fill( (uint32_t*)line + blit->x, argb_premul, blit->w );
    }
}


static void
skin_line_blit_copy( uint32_t*  dst, const uint32_t*  src, int  len )
{
    memcpy( (char*)dst, (const char*)src, len*4 );
}



static void
skin_line_blit_srcover( uint32_t*  dst, const uint32_t*  src, int  len )
{
    uint32_t*  end = dst + len;
    ARGB_DECL_ZERO();

    for ( ; dst < end; dst++ ) {
        ARGB_DECL(s);
        ARGB_DECL(d);
        ARGB_DECL(v);
        uint32_t  alpha;

        ARGB_READ(s,src);
        alpha = (src[0] >> 24);
        if (alpha > 0) {
            ARGB_READ(d,dst);
            alpha = 256 - alpha;
            ARGB_MULSHIFT(v,d,alpha,8);
            ARGB_ADD(v,d);
            ARGB_WRITE(v,dst);
        }
    }
}

static void
skin_line_blit_dstover( uint32_t*  dst, const uint32_t*  src, int  len )
{
    uint32_t*  end = dst + len;
    ARGB_DECL_ZERO();

    for ( ; dst < end; dst++ ) {
        ARGB_DECL(s);
        ARGB_DECL(d);
        ARGB_DECL(v);
        uint32_t  alpha;

        ARGB_READ(d,dst);
        alpha = (dst[0] >> 24);
        if (alpha < 255) {
            ARGB_READ(s,src);
            alpha = 256 - alpha;
            ARGB_MULSHIFT(v,s,alpha,8);
            ARGB_ADD(v,s);
            ARGB_WRITE(v,dst);
        }
    }
}


extern void
skin_surface_blit( SkinSurface*  dst,
                   SkinPos*      dst_pos,
                   SkinSurface*  src,
                   SkinRect*     src_rect,
                   SkinBlitOp    blitop )
{
    SkinLineBlitFunc  func;
    SkinBlit          blit[1];

    switch (blitop) {
        case SKIN_BLIT_COPY:    func = skin_line_blit_copy; break;
        case SKIN_BLIT_SRCOVER: func = skin_line_blit_srcover; break;
        case SKIN_BLIT_DSTOVER: func = skin_line_blit_dstover; break;
        default: return;
    }

    if ( skin_blit_init_blit( blit, dst, dst_pos, src, src_rect ) ) {
        uint8_t*   line   = blit->dst_line;
        uint8_t*   sline  = blit->src_line;
        int        pitch  = blit->dst_pitch;
        int        spitch = blit->src_pitch;
        uint8_t*   end    = line + pitch*blit->h;

        for ( ; line != end; line += pitch, sline += spitch )
            func( (uint32_t*)line + blit->x, (uint32_t*)sline + blit->sx, blit->w );

        skin_blit_done(blit);
    }
}
