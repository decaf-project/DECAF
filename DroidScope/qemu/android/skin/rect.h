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
#ifndef _ANDROID_SKIN_RECT_H
#define _ANDROID_SKIN_RECT_H

/**  Rectangles
 **/

typedef enum {
    SKIN_ROTATION_0,
    SKIN_ROTATION_90,
    SKIN_ROTATION_180,
    SKIN_ROTATION_270
} SkinRotation;

typedef struct {
    int  x, y;
} SkinPos;

extern void  skin_pos_rotate( SkinPos*  dst, SkinPos*  src, SkinRotation  rot );

typedef struct {
    int  w, h;
} SkinSize;

extern void skin_size_rotate( SkinSize*  dst, SkinSize*  src, SkinRotation  rot );
extern int  skin_size_contains( SkinSize*  size, int  x, int  y );


typedef struct {
    SkinPos   pos;
    SkinSize  size;
} SkinRect;

extern void  skin_rect_init     ( SkinRect*  r, int x, int y, int  w, int  h );
extern void  skin_rect_translate( SkinRect*  r, int  dx, int dy );
extern void  skin_rect_rotate   ( SkinRect*  dst, SkinRect*  src, SkinRotation  rotation );
extern int   skin_rect_contains ( SkinRect*  r, int  x, int  y );
extern int   skin_rect_intersect( SkinRect*  result, SkinRect*  r1, SkinRect*  r2 );
extern int   skin_rect_equals   ( SkinRect*  r1, SkinRect*  r2 );

typedef enum {
    SKIN_OUTSIDE = 0,
    SKIN_INSIDE  = 1,
    SKIN_OVERLAP = 2
} SkinOverlap;

extern SkinOverlap  skin_rect_contains_rect( SkinRect  *r1, SkinRect  *r2 );

typedef struct {
    int  x1, y1;
    int  x2, y2;
} SkinBox;

extern void  skin_box_init( SkinBox*  box, int x1, int  y1, int  x2, int  y2 );
extern void  skin_box_minmax_init( SkinBox*  box );
extern void  skin_box_minmax_update( SkinBox*  box, SkinRect*  rect );
extern int   skin_box_minmax_to_rect( SkinBox*  box, SkinRect*  rect );
extern void  skin_box_from_rect( SkinBox*  box, SkinRect*  rect );
extern void  skin_box_to_rect( SkinBox*  box, SkinRect*  rect );

#endif /* _ANDROID_SKIN_RECT_H */
