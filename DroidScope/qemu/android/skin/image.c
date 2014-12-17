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
#include "android/skin/image.h"
#include "android/resource.h"
#include <assert.h>
#include <limits.h>

#define  DEBUG  0

#if DEBUG
static void D(const char*  fmt, ...)
{
    va_list  args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}
#else
#define  D(...)  do{}while(0)
#endif

/********************************************************************************/
/********************************************************************************/
/*****                                                                      *****/
/*****            U T I L I T Y   F U N C T I O N S                         *****/
/*****                                                                      *****/
/********************************************************************************/
/********************************************************************************/

SDL_Surface*
sdl_surface_from_argb32( void*  base, int  w, int  h )
{
    return SDL_CreateRGBSurfaceFrom(
                        base, w, h, 32, w*4,
#if HOST_WORDS_BIGENDIAN
                        0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000
#else
                        0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000
#endif
                        );
}

static void*
rotate_image( void*  data, unsigned  width, unsigned  height,  SkinRotation  rotation )
{
    void*  result;

    result = malloc( width*height*4 );
    if (result == NULL)
        return NULL;

    switch (rotation & 3)
    {
    case SKIN_ROTATION_0:
        memcpy( (char*)result, (const char*)data, width*height*4 );
        break;

    case SKIN_ROTATION_270:
        {
            unsigned*  start    = (unsigned*)data;
            unsigned*  src_line = start + (width-1);
            unsigned*  dst_line = (unsigned*)result;
            unsigned   hh;

            for (hh = width; hh > 0; hh--)
            {
                unsigned*  src   = src_line;
                unsigned*  dst   = dst_line;
                unsigned   count = height;

                for ( ; count > 0; count-- ) {
                    dst[0] = src[0];
                    dst   += 1;
                    src   += width;
                }

                src_line -= 1;
                dst_line += height;
            }
        }
        break;

    case SKIN_ROTATION_180:
        {
            unsigned*  start    = (unsigned*)data;
            unsigned*  src_line = start + width*(height-1);
            unsigned*  dst_line = (unsigned*)result;
            unsigned   hh;

            for (hh = height; hh > 0; hh--)
            {
                unsigned*  src = src_line + (width-1);
                unsigned*  dst = dst_line;

                while (src >= src_line)
                    *dst++ = *src--;

                dst_line += width;
                src_line -= width;
            }
        }
        break;

    case SKIN_ROTATION_90:
        {
            unsigned*  start    = (unsigned*)data;
            unsigned*  src_line = start + width*(height-1);
            unsigned*  dst_line = (unsigned*)result ;
            unsigned   hh;

            for (hh = width; hh > 0; hh--)
            {
                unsigned*  src = src_line;
                unsigned*  dst = dst_line;
                unsigned   count;

                for (count = height; count > 0; count--) {
                    dst[0] = src[0];
                    dst   += 1;
                    src   -= width;
                }

                dst_line += height;
                src_line += 1;
            }
        }
        break;

    default:
        ;
    }

    return result;
}


static void
blend_image( unsigned*  dst_pixels,
             unsigned*  src_pixels,
             unsigned   w,
             unsigned   h,
             int        alpha )
{
    unsigned*  dst     = dst_pixels;
    unsigned*  dst_end = dst + w*h;
    unsigned*  src     = src_pixels;

    for ( ; dst < dst_end; dst++, src++ )
    {
        {
            unsigned  ag = (src[0] >> 8) & 0xff00ff;
            unsigned  rb =  src[0]       & 0xff00ff;

            ag = (ag*alpha) & 0xff00ff00;
            rb = ((rb*alpha) >> 8) & 0x00ff00ff;

            dst[0] = ag | rb;
        }
    }
}


static unsigned
skin_image_desc_hash( SkinImageDesc*  desc )
{
    unsigned  h = 0;
    int       n;

    for (n = 0; desc->path[n] != 0; n++) {
        int  c = desc->path[n];
        h = h*33 + c;
    }
    h += desc->rotation*1573;
    h += desc->blend * 7;

    return  h;
}


static int
skin_image_desc_equal( SkinImageDesc*  a,
                       SkinImageDesc*  b )
{
    return (a->rotation == b->rotation &&
            a->blend    == b->blend    &&
            !strcmp(a->path, b->path));
}

/********************************************************************************/
/********************************************************************************/
/*****                                                                      *****/
/*****            S K I N   I M A G E S                                     *****/
/*****                                                                      *****/
/********************************************************************************/
/********************************************************************************/

enum {
    SKIN_IMAGE_CLONE = (1 << 0)   /* this image is a clone */
};

struct SkinImage {
    unsigned         hash;
    SkinImage*       link;
    int              ref_count;
    SkinImage*       next;
    SkinImage*       prev;
    SDL_Surface*     surface;
    unsigned         flags;
    unsigned         w, h;
    void*            pixels;  /* 32-bit ARGB */
    SkinImageDesc    desc;
};




static const SkinImage  _no_image[1] = {
    { 0, NULL, 0, NULL, NULL, NULL, 0, 0, 0, NULL, { "<none>", SKIN_ROTATION_0, 0 } }
};

SkinImage*  SKIN_IMAGE_NONE = (SkinImage*)&_no_image;

static void
skin_image_free( SkinImage*  image )
{
    if (image && image != _no_image)
    {
        if (image->surface) {
            SDL_FreeSurface(image->surface);
            image->surface = NULL;
        }

        if (image->pixels) {
            free( image->pixels );
            image->pixels = NULL;
        }

        free(image);
    }
}


static SkinImage*
skin_image_alloc( SkinImageDesc*  desc, unsigned  hash )
{
    int         len   = strlen(desc->path);
    SkinImage*  image = calloc(1, sizeof(*image) + len + 1);

    if (image) {
        image->desc = desc[0];
        image->desc.path = (const char*)(image + 1);
        memcpy( (char*)image->desc.path, desc->path, len );
        ((char*)image->desc.path)[len] = 0;

        image->hash      = hash;
        image->next      = image->prev = image;
        image->ref_count = 1;
    }
    return image;
}


extern void *loadpng(const char *fn, unsigned *_width, unsigned *_height);
extern void *readpng(const unsigned char*  base, size_t  size, unsigned *_width, unsigned *_height);

static int
skin_image_load( SkinImage*  image )
{
    void*     data;
    unsigned  w, h;
    const char*  path = image->desc.path;

    if (path[0] == ':') {
        size_t                size;
        const unsigned char*  base;

        if (path[1] == '/' || path[1] == '\\')
            path += 1;

        base = android_resource_find( path+1, &size );
        if (base == NULL) {
            fprintf(stderr, "failed to locate built-in image file '%s'\n", path );
            return -1;
        }

        data = readpng(base, size, &w, &h);
        if (data == NULL) {
            fprintf(stderr, "failed to load built-in image file '%s'\n", path );
            return -1;
        }
    } else {
        data = loadpng(path, &w, &h);
        if (data == NULL) {
            fprintf(stderr, "failed to load image file '%s'\n", path );
            return -1;
        }
    }

   /* the data is loaded into memory as RGBA bytes by libpng. we want to manage
    * the values as 32-bit ARGB pixels, so swap the bytes accordingly depending
    * on our CPU endianess
    */
    {
        unsigned*  d     = data;
        unsigned*  d_end = d + w*h;

        for ( ; d < d_end; d++ ) {
            unsigned  pix = d[0];
#if HOST_WORDS_BIGENDIAN
            /* R,G,B,A read as RGBA => ARGB */
            pix = ((pix >> 8) & 0xffffff) | (pix << 24);
#else
            /* R,G,B,A read as ABGR => ARGB */
            pix = (pix & 0xff00ff00) | ((pix >> 16) & 0xff) | ((pix & 0xff) << 16);
#endif
            d[0] = pix;
        }
    }

    image->pixels = data;
    image->w      = w;
    image->h      = h;

    image->surface = sdl_surface_from_argb32( image->pixels, w, h );
    if (image->surface == NULL) {
        fprintf(stderr, "failed to create SDL surface for '%s' image\n", path);
        return -1;
    }
    return 0;
}


/* simple hash table for images */

#define  NUM_BUCKETS  64

typedef struct {
    SkinImage*     buckets[ NUM_BUCKETS ];
    SkinImage      mru_head;
    int            num_images;
    unsigned long  total_pixels;
    unsigned long  max_pixels;
    unsigned long  total_images;
} SkinImageCache;


static void
skin_image_cache_init( SkinImageCache*  cache )
{
    memset(cache, 0, sizeof(*cache));
#if DEBUG
    cache->max_pixels = 1;
#else
    cache->max_pixels = 4*1024*1024;  /* limit image cache to 4 MB */
#endif
    cache->mru_head.next = cache->mru_head.prev = &cache->mru_head;
}


static void
skin_image_cache_remove( SkinImageCache*  cache,
                         SkinImage*       image )
{
    /* remove from hash table */
    SkinImage**  pnode = cache->buckets + (image->hash & (NUM_BUCKETS-1));
    SkinImage*   node;

    for (;;) {
        node = *pnode;
        assert(node != NULL);
        if (node == NULL)  /* should not happen */
            break;
        if (node == image) {
            *pnode = node->link;
            break;
        }
        pnode = &node->link;
    }

    D( "skin_image_cache: remove '%s' (rot=%d), %d pixels\n",
       node->desc.path, node->desc.rotation, node->w*node->h );

    /* remove from mru list */
    image->prev->next = image->next;
    image->next->prev = image->prev;

    cache->total_pixels -= image->w*image->h;
    cache->total_images -= 1;
}


static SkinImage*
skin_image_cache_raise( SkinImageCache*  cache,
                        SkinImage*       image )
{
    if (image != cache->mru_head.next) {
        SkinImage*  prev = image->prev;
        SkinImage*  next = image->next;

        /* remove from mru list */
        prev->next = next;
        next->prev = prev;

        /* add to top */
        image->prev = &cache->mru_head;
        image->next = image->prev->next;
        image->prev->next = image;
        image->next->prev = image;
    }
    return image;
}


static void
skin_image_cache_flush( SkinImageCache*  cache )
{
    SkinImage*     image = cache->mru_head.prev;
    int            count = 0;

    D("skin_image_cache_flush: starting\n");
    while (cache->total_pixels > cache->max_pixels &&
           image != &cache->mru_head)
    {
        SkinImage*  prev = image->prev;

        if (image->ref_count == 0) {
            skin_image_cache_remove(cache, image);
            count += 1;
        }
        image = prev;
    }
    D("skin_image_cache_flush: finished, %d images flushed\n", count);
}


static SkinImage**
skin_image_lookup_p( SkinImageCache*   cache,
                     SkinImageDesc*    desc,
                     unsigned         *phash )
{
    unsigned     h     = skin_image_desc_hash(desc);
    unsigned     index = h & (NUM_BUCKETS-1);
    SkinImage**  pnode = &cache->buckets[index];
    for (;;) {
        SkinImage*  node = *pnode;
        if (node == NULL)
            break;
        if (node->hash == h && skin_image_desc_equal(desc, &node->desc))
            break;
        pnode = &node->link;
    }
    *phash = h;
    return  pnode;
}


static SkinImage*
skin_image_create( SkinImageDesc*  desc, unsigned  hash )
{
    SkinImage*  node;

    node = skin_image_alloc( desc, hash );
    if (node == NULL)
        return SKIN_IMAGE_NONE;

    if (desc->rotation == SKIN_ROTATION_0 &&
        desc->blend    == SKIN_BLEND_FULL)
    {
        if (skin_image_load(node) < 0) {
            skin_image_free(node);
            return SKIN_IMAGE_NONE;
        }
    }
    else
    {
        SkinImageDesc  desc0 = desc[0];
        SkinImage*     parent;

        desc0.rotation = SKIN_ROTATION_0;
        desc0.blend    = SKIN_BLEND_FULL;

        parent = skin_image_find( &desc0 );
        if (parent == SKIN_IMAGE_NONE)
            return SKIN_IMAGE_NONE;

        SDL_LockSurface(parent->surface);

        if (desc->rotation == SKIN_ROTATION_90 ||
            desc->rotation == SKIN_ROTATION_270)
        {
            node->w = parent->h;
            node->h = parent->w;
        } else {
            node->w = parent->w;
            node->h = parent->h;
        }

        node->pixels = rotate_image( parent->pixels, parent->w, parent->h,
                                    desc->rotation );

        SDL_UnlockSurface(parent->surface);
        skin_image_unref(&parent);

        if (node->pixels  == NULL) {
            skin_image_free(node);
            return SKIN_IMAGE_NONE;
        }

        if (desc->blend != SKIN_BLEND_FULL)
            blend_image( node->pixels, node->pixels, node->w, node->h, desc->blend );

        node->surface = sdl_surface_from_argb32( node->pixels, node->w, node->h );
        if (node->surface == NULL) {
            skin_image_free(node);
            return SKIN_IMAGE_NONE;
        }
    }
    return node;
}


static SkinImageCache   _image_cache[1];
static int              _image_cache_init;

SkinImage*
skin_image_find( SkinImageDesc*  desc )
{
    SkinImageCache*  cache = _image_cache;
    unsigned         hash;
    SkinImage**      pnode = skin_image_lookup_p( cache, desc, &hash );
    SkinImage*       node  = *pnode;

    if (!_image_cache_init) {
        _image_cache_init = 1;
        skin_image_cache_init(cache);
    }

    if (node) {
        node->ref_count += 1;
        return skin_image_cache_raise( cache, node );
    }
    node = skin_image_create( desc, hash );
    if (node == SKIN_IMAGE_NONE)
        return node;

    /* add to hash table */
    node->link = *pnode;
    *pnode     = node;

    /* add to mru list */
    skin_image_cache_raise( cache, node );

    D( "skin_image_cache: add '%s' (rot=%d), %d pixels\n",
       node->desc.path, node->desc.rotation, node->w*node->h );

    cache->total_pixels += node->w*node->h;
    if (cache->total_pixels > cache->max_pixels)
        skin_image_cache_flush( cache );

    return node;
}


SkinImage*
skin_image_find_simple( const char*  path )
{
    SkinImageDesc  desc;

    desc.path     = path;
    desc.rotation = SKIN_ROTATION_0;
    desc.blend    = SKIN_BLEND_FULL;

    return skin_image_find( &desc );
}


SkinImage*
skin_image_ref( SkinImage*  image )
{
    if (image && image != _no_image)
        image->ref_count += 1;

    return image;
}


void
skin_image_unref( SkinImage**  pimage )
{
    SkinImage*  image = *pimage;

    if (image) {
        if (image != _no_image && --image->ref_count == 0) {
            if ((image->flags & SKIN_IMAGE_CLONE) != 0) {
                skin_image_free(image);
            }
        }
        *pimage = NULL;
    }
}


SkinImage*
skin_image_rotate( SkinImage*  source, SkinRotation  rotation )
{
    SkinImageDesc  desc;
    SkinImage*     image;

    if (source == _no_image || source->desc.rotation == rotation)
        return source;

    desc          = source->desc;
    desc.rotation = rotation;
    image         = skin_image_find( &desc );
    skin_image_unref( &source );
    return image;
}


SkinImage*
skin_image_clone( SkinImage*  source )
{
    SkinImage*   image;

    if (source == NULL || source == _no_image)
        return SKIN_IMAGE_NONE;

    image = calloc(1,sizeof(*image));
    if (image == NULL)
        goto Fail;

    image->desc  = source->desc;
    image->hash  = source->hash;
    image->flags = SKIN_IMAGE_CLONE;
    image->w     = source->w;
    image->h     = source->h;
    image->pixels = rotate_image( source->pixels, source->w, source->h,
                                  SKIN_ROTATION_0 );
    if (image->pixels == NULL)
        goto Fail;

    image->surface = sdl_surface_from_argb32( image->pixels, image->w, image->h );
    if (image->surface == NULL)
        goto Fail;

    return image;
Fail:
    if (image != NULL)
        skin_image_free(image);
    return SKIN_IMAGE_NONE;
}

SkinImage*
skin_image_clone_full( SkinImage*    source,
                       SkinRotation  rotation,
                       int           blend )
{
    SkinImageDesc   desc;
    SkinImage*      clone;

    if (source == NULL || source == SKIN_IMAGE_NONE)
        return SKIN_IMAGE_NONE;

    if (rotation == SKIN_ROTATION_0 &&
        blend    == SKIN_BLEND_FULL)
    {
        return skin_image_clone(source);
    }

    desc.path     = source->desc.path;
    desc.rotation = rotation;
    desc.blend    = blend;

    clone = skin_image_create( &desc, 0 );
    if (clone != SKIN_IMAGE_NONE)
        clone->flags |= SKIN_IMAGE_CLONE;

    return clone;
}

/* apply blending to a source skin image and copy the result to a target clone image */
extern void
skin_image_blend_clone( SkinImage*  clone, SkinImage*  source, int  blend )
{
    SDL_LockSurface( clone->surface );
    blend_image( clone->pixels, source->pixels, source->w, source->h, blend );
    SDL_UnlockSurface( clone->surface );
    SDL_SetAlpha( clone->surface, SDL_SRCALPHA, 255 );
}

int
skin_image_w( SkinImage*  image )
{
    return  image ? image->w : 0;
}

int
skin_image_h( SkinImage*  image )
{
    return  image ? image->h : 0;
}

int
skin_image_org_w( SkinImage*  image )
{
    if (image) {
        if (image->desc.rotation == SKIN_ROTATION_90 ||
            image->desc.rotation == SKIN_ROTATION_270)
            return image->h;
        else
            return image->w;
    }
    return 0;
}

int
skin_image_org_h( SkinImage*  image )
{
    if (image) {
        if (image->desc.rotation == SKIN_ROTATION_90 ||
            image->desc.rotation == SKIN_ROTATION_270)
            return image->w;
        else
            return image->h;
    }
    return 0;
}

SDL_Surface*
skin_image_surface( SkinImage*  image )
{
    return image ? image->surface : NULL;
}
