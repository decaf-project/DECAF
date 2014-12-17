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
#ifndef _ANDROID_SKIN_REGION_H
#define _ANDROID_SKIN_REGION_H

#include "android/skin/rect.h"

typedef struct SkinRegion  SkinRegion;

extern void  skin_region_init_empty( SkinRegion*  r );
extern void  skin_region_init( SkinRegion*  r, int  x1, int  y1, int  x2, int  y2 );
extern void  skin_region_init_rect( SkinRegion*  r, SkinRect*  rect );
extern void  skin_region_init_box( SkinRegion*  r, SkinBox*  box );
extern void  skin_region_init_copy( SkinRegion*  r, SkinRegion*  r2 );
extern void  skin_region_reset( SkinRegion*  r );

/* finalize region, then copy src into it */
extern void  skin_region_copy( SkinRegion*  r, SkinRegion*  src );

/* compare two regions for equality */
extern int   skin_region_equals( SkinRegion*  r1, SkinRegion*  r2 );

/* swap two regions */
extern void  skin_region_swap( SkinRegion*  r, SkinRegion*  r2 );

extern int   skin_region_is_empty( SkinRegion*  r );
extern int   skin_region_is_rect( SkinRegion*  r );
extern int   skin_region_is_complex( SkinRegion*  r );
extern void  skin_region_get_bounds( SkinRegion*  r, SkinRect*  bounds );

extern void  skin_region_translate( SkinRegion*  r, int  dx, int  dy );

extern SkinOverlap  skin_region_contains( SkinRegion*  r, int  x, int  y );

extern SkinOverlap  skin_region_contains_rect( SkinRegion*  r,
                                               SkinRect*    rect );

extern SkinOverlap  skin_region_contains_box( SkinRegion*  r, SkinBox*  b );

/* returns overlap mode for "is r2 inside r1" */
extern  SkinOverlap  skin_region_test_intersect( SkinRegion*  r1,
                                                 SkinRegion*  r2 );

/* performs r = (intersect r r2), returns true if the resulting region
   is not empty */
extern int  skin_region_intersect     ( SkinRegion*  r, SkinRegion*  r2 );
extern int  skin_region_intersect_rect( SkinRegion*  r, SkinRect*    rect );

/* performs r = (intersect r (region+_from_rect rect)), returns true iff
   the resulting region is not empty */

/* performs r = (union r r2) */
extern void skin_region_union     ( SkinRegion*  r, SkinRegion*  r2 );
extern void skin_region_union_rect( SkinRegion*  r, SkinRect*  rect );

/* performs r = (difference r r2) */
extern void skin_region_substract     ( SkinRegion*  r, SkinRegion*  r2 );
extern void skin_region_substract_rect( SkinRegion*  r, SkinRect*  rect );

/* performs r = (xor r r2) */
extern void skin_region_xor( SkinRegion*  r, SkinRegion*  r2 );

typedef struct SkinRegionIterator  SkinRegionIterator;

/* iterator */
extern void  skin_region_iterator_init( SkinRegionIterator*  iter,
                                        SkinRegion*          r );

extern int   skin_region_iterator_next( SkinRegionIterator*  iter,
                                        SkinRect            *rect );

extern int   skin_rection_iterator_next_box( SkinRegionIterator*  iter,
                                             SkinBox             *box );

/* the following should be considered private definitions. they're only here
   to allow clients to allocate SkinRegion objects themselves... */

typedef signed short   SkinRegionRun;
#define SKIN_REGION_SENTINEL  0x7fff

struct SkinRegion
{
    SkinRect        bounds;
    SkinRegionRun*  runs;
};

struct SkinRegionIterator
{
    SkinRegion*     region;
    SkinRegionRun*  band;
    SkinRegionRun*  span;
};

#endif /* _ANDROID_SKIN_REGION_H */
