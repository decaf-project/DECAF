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
#ifndef _ANDROID_SKIN_COMPOSER_H
#define _ANDROID_SKIN_COMPOSER_H

#include "android/skin/rect.h"
#include "android/skin/region.h"
#include "android/utils/reflist.h"

/* the composer displays stacked surfaces on a target window/SDL_Surface */

typedef enum {
    SKIN_PLATE_SURFACE = 0,
    SKIN_PLATE_GROUP,
    SKIN_PLATE_SPACE
} SkinPlateType;

typedef union SkinPlate      SkinPlate;
typedef struct SkinViewport  SkinViewport;

struct SkinPlateAny {
    SkinPlateType    type;         /* class pointer */
    SkinPlate*       parent;       /* parent container */
    SkinPos          pos;          /* position relative to parent */
    SkinRegion       region[1];    /* the plate's region */
    char             isVisible;    /* flag: TRUE iff the region is visible */
    char             isOpaque;     /* flag: TRUE iff the region is opaque */
};


typedef void (*SkinPlateDrawFunc)( void*  user, SkinRegion*  region, SkinPos*  apos, SkinViewport*  viewport, int  opaque );
typedef void (*SkinPlateDoneFunc)( void*  user );

struct SkinPlateSurface {
    struct SkinPlateAny   any;
    void*                 user;
    SkinPlateDrawFunc     draw;
    SkinPlateDoneFunc     done;
};

struct SkinPlateGroup {
    struct SkinPlateAny   any;
    char                  hasRegion;
    char                  hasOpaqueRegion;
    SkinRegion            opaqueRegion[1];
    ARefList              children[1];
};

struct SkinPlateSpace {
    struct SkinPlateGroup   group;
    ARefList                viewports[1];
};


union SkinPlate {
    struct SkinPlateAny        any;
    struct SkinPlateSurface    surface;
    struct SkinPlateGroup      group;
    struct SkinPlateSpace      space;
};


extern SkinPlate*   skin_plate_surface( SkinPlate*         parent,
                                        SkinPos*           pos,
                                        SkinRegion*        region,
                                        void*              user,
                                        SkinPlateDrawFunc  draw,
                                        SkinPlateDoneFunc  done );

extern SkinPlate*   skin_plate_group( SkinPlate*  parent, SkinPos*  pos );

extern SkinPlate*   skin_plate_space( void );

extern void  skin_plate_free( SkinPlate*  plate );
extern void  skin_plate_invalidate( SkinPlate*  plate, SkinRegion*  region );
extern void  skin_plate_set_pos( SkinPlate*  plate, int  x, int  y );
extern void  skin_plate_set_visible( SkinPlate*  plate, int  isVisible );
extern void  skin_plate_set_opaque( SkinPlate* plate, int  isOpaque );

struct SkinViewport {
    SkinPlate*  space;
    SkinRect    rect;
    void*       surface;
    SkinPos     spos;
    SkinRegion  update[1];
};

extern SkinViewport*  skin_viewport( SkinPlate*  space, SkinRect*  rect, void*  surface, int  sx, int  sy );
extern void           skin_viewport_free( SkinViewport*  v );
extern void           skin_viewport_invalidate( SkinViewport*  v, SkinRegion*  r );
extern void           skin_viewport_redraw( SkinViewport*  v );

#endif /* _ANDROID_SKIN_COMPOSER_H */
