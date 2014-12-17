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
#include "android/skin/rect.h"
#include <limits.h>

#define  SKIN_POS_INITIALIZER   { 0, 0 }

void
skin_pos_rotate( SkinPos*  dst, SkinPos*  src, SkinRotation  rotation )
{
    int  x = src->x;
    int  y = src->y;

    switch ( rotation & 3 ) {
    case SKIN_ROTATION_0:
        dst->x = x;
        dst->y = y;
        break;

    case SKIN_ROTATION_90:
        dst->x = -y;
        dst->y =  x;
        break;

    case SKIN_ROTATION_180:
        dst->x = -x;
        dst->y = -y;
        break;

    default:
        dst->x =  y;
        dst->y = -x;
    }
}


#define  SKIN_SIZE_INITIALIZER  { 0, 0 }

int
skin_size_contains( SkinSize*  size, int  x, int  y )
{
    return ( (unsigned) x < (unsigned) size->w &&
             (unsigned) y < (unsigned) size->h );
}

void
skin_size_rotate( SkinSize*  dst, SkinSize*  src, SkinRotation  rot )
{
    int  w = src->w;
    int  h = src->h;

    if ((rot & 1) != 0) {
        dst->w = h;
        dst->h = w;
    } else {
        dst->w = w;
        dst->h = h;
    }
}

/** SKIN RECTANGLES
 **/
#define  SKIN_RECT_INITIALIZER  { SKIN_POS_INITIALIZER, SKIN_SIZE_INITIALIZER }

void
skin_rect_init( SkinRect*  r, int x, int  y, int  w, int  h )
{
    if (w < 0 || h < 0)
        x = y = w = h = 0;

    r->pos.x  = x;
    r->pos.y  = y;
    r->size.w = w;
    r->size.h = h;
}


void
skin_rect_translate( SkinRect*  r, int  dx, int  dy )
{
    r->pos.x += dx;
    r->pos.y += dy;
}


void
skin_rect_rotate( SkinRect*  dst, SkinRect*  src, SkinRotation  rot )
{
    int  x, y, w, h;

    switch (rot & 3) {
        case SKIN_ROTATION_90:
            x = src->pos.x;
            y = src->pos.y;
            w = src->size.w;
            h = src->size.h;
            dst->pos.x  = -(y + h);
            dst->pos.y  = x;
            dst->size.w = h;
            dst->size.h = w;
            break;

        case SKIN_ROTATION_180:
            dst->pos.x = -(src->pos.x + src->size.w);
            dst->pos.y = -(src->pos.y + src->size.h);
            dst->size  = src->size;
            break;

        case SKIN_ROTATION_270:
            x = src->pos.x;
            y = src->pos.y;
            w = src->size.w;
            h = src->size.h;
            dst->pos.x  = y;
            dst->pos.y  = -(x + w);
            dst->size.w = h;
            dst->size.h = w;
            break;

        default:
            dst[0] = src[0];
    }
}


int
skin_rect_contains( SkinRect*  r, int  x, int  y )
{
    return ( (unsigned)(x - r->pos.x) < (unsigned)r->size.w &&
             (unsigned)(y - r->pos.y) < (unsigned)r->size.h );
}

SkinOverlap
skin_rect_contains_rect( SkinRect  *r1, SkinRect  *r2 )
{
    SkinBox  a, b;

    skin_box_from_rect( &a, r1 );
    skin_box_from_rect( &b, r2 );

    if (a.x2 <= b.x1 || b.x2 <= a.x1 || a.y2 <= b.y1 || b.y2 <= a.y1) {
        return SKIN_OUTSIDE;
    }

    if (b.x1 >= a.x1 && b.x2 <= a.x2 && b.y1 >= a.y1 && b.y2 <= a.y2) {
        return SKIN_INSIDE;
    }

    return SKIN_OVERLAP;
}


int
skin_rect_intersect( SkinRect*  result, SkinRect*  r1, SkinRect*  r2 )
{
    SkinBox  a, b, r;

    skin_box_from_rect( &a, r1 );
    skin_box_from_rect( &b, r2 );

    if (a.x2 <= b.x1 || b.x2 <= a.x1 || a.y2 <= b.y1 || b.y2 <= a.y1) {
        result->pos.x = result->pos.y = result->size.w = result->size.h = 0;
        return 0;
    }

    r.x1 = (a.x1 > b.x1) ? a.x1 : b.x1;
    r.x2 = (a.x2 < b.x2) ? a.x2 : b.x2;
    r.y1 = (a.y1 > b.y1) ? a.y1 : b.y1;
    r.y2 = (a.y2 < b.y2) ? a.y2 : b.y2;

    skin_box_to_rect( &r, result );
    return 1;
}

int
skin_rect_equals( SkinRect*  r1, SkinRect*  r2 )
{
    return (r1->pos.x  == r2->pos.x  && r1->pos.y  == r2->pos.y &&
            r1->size.w == r2->size.w && r2->size.h == r2->size.h);
}

/** SKIN BOXES
 **/
void
skin_box_minmax_init( SkinBox*  box )
{
    box->x1 = box->y1 = INT_MAX;
    box->x2 = box->y2 = INT_MIN;
}

void
skin_box_minmax_update( SkinBox*  a, SkinRect*  r )
{
    SkinBox  b[1];

    skin_box_from_rect(b, r);

    if (b->x1 < a->x1) a->x1 = b->x1;
    if (b->y1 < a->y1) a->y1 = b->y1;
    if (b->x2 > a->x2) a->x2 = b->x2;
    if (b->y2 > a->y2) a->y2 = b->y2;
}

int
skin_box_minmax_to_rect( SkinBox*  box, SkinRect*  r )
{
    if (box->x1 > box->x2) {
        r->pos.x = r->pos.y = r->size.w = r->size.h = 0;
        return 0;
    }
    skin_box_to_rect( box, r );
    return 1;
}

void
skin_box_from_rect( SkinBox*  box, SkinRect*  r )
{
    box->x1 = r->pos.x;
    box->y1 = r->pos.y;
    box->x2 = r->size.w + box->x1;
    box->y2 = r->size.h + box->y1;
}

void
skin_box_to_rect( SkinBox*  box, SkinRect*  r )
{
    r->pos.x  = box->x1;
    r->pos.y  = box->y1;
    r->size.w = box->x2 - box->x1;
    r->size.h = box->y2 - box->y1;
}

