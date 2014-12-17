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
#ifndef _ANDROID_SKIN_IMAGE_H
#define _ANDROID_SKIN_IMAGE_H

#include "android/android.h"
#include <SDL.h>
#include "android/skin/rect.h"

/* helper functions */

extern SDL_Surface*    sdl_surface_from_argb32( void*  base, int  w, int  h );

/* skin image file objects */

/* opaque skin image type. all skin images are placed in a simple MRU cache
 * to limit the emulator's memory usage, with the exception of 'clones' created
 * with skin_image_clone() or skin_image_clone_blend()
 */
typedef struct SkinImage   SkinImage;

/* a descriptor for a given skin image */
typedef struct SkinImageDesc {
    const char*      path;      /* image file path (must be .png) */
    AndroidRotation  rotation;  /* rotation */
    int              blend;     /* blending, 0..256 value */
} SkinImageDesc;

#define  SKIN_BLEND_NONE   0
#define  SKIN_BLEND_HALF   128
#define  SKIN_BLEND_FULL   256

/* a special value returned when an image cannot be properly loaded */
extern SkinImage*    SKIN_IMAGE_NONE;

/* return the SDL_Surface* pointer of a given skin image */
extern SDL_Surface*  skin_image_surface( SkinImage*  image );
extern int           skin_image_w      ( SkinImage*  image );
extern int           skin_image_h      ( SkinImage*  image );
extern int           skin_image_org_w  ( SkinImage*  image );
extern int           skin_image_org_h  ( SkinImage*  image );

/* get an image from the cache (load it from the file if necessary).
 * returns SKIN_IMAGE_NONE in case of error. cannot return NULL
 * this function also increments the reference count of the skin image,
 * use "skin_image_unref()" when you don't need it anymore
 */
extern SkinImage*    skin_image_find( SkinImageDesc*  desc );

extern SkinImage*    skin_image_find_simple( const char*  path );

/* increment the reference count of a given skin image,
 * don't do anything if 'image' is NULL */
extern SkinImage*    skin_image_ref( SkinImage*  image );

/* decrement the reference count of a given skin image. if
 * the count reaches 0, the image becomes eligible for cache flushing.
 * unless it was created through a skin_image_clone... function, where
 * it is immediately discarded...
 */
extern void          skin_image_unref( SkinImage**  pimage );

/* get the rotation of a given image. this decrements the reference count
 * of the source after returning the target, whose reference count is incremented
 */
extern SkinImage*    skin_image_rotate( SkinImage*  source, SkinRotation  rotation );

/* create a skin image clone. the clone is not part of the cache and will
 * be destroyed immediately when its reference count reaches 0. this is useful
 * if you need to modify the content of the clone (e.g. blending)
 */
extern SkinImage*    skin_image_clone( SkinImage*  source );

/* create a skin image clone, the clone is a rotated version of a source image
 */
extern SkinImage*    skin_image_clone_full( SkinImage*       source,
                                            SkinRotation     rotation,
                                            int              blend );

/* apply blending to a source skin image and copy the result to a target clone image */
extern void          skin_image_blend_clone( SkinImage*  clone, SkinImage*  source, int  blend );

#endif /* _ANDROID_SKIN_IMAGE_H */
