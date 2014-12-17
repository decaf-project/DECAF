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
#include "android/skin/composer.h"
#include <stddef.h>
#include "android/utils/system.h"

/* forwards */
static void  skin_plate_get_region       ( SkinPlate*  p, SkinRegion  *pregion );
static void  skin_plate_get_opaque_region( SkinPlate*  p, SkinRegion  *pregion );

/* recompute region if needed */
static void
skin_plate_ensure_region( SkinPlate*  p )
{
    if (p->any.type == SKIN_PLATE_SURFACE || p->group.hasRegion)
        return;
    else {
        int  n, count = areflist_count( p->group.children );

        skin_region_reset(p->any.region);

        for (n = 0; n < count; n++) {
            SkinRegion  r[1];
            SkinPlate*  child = areflist_get( p->group.children, n );

            skin_plate_get_region( child, r );
            skin_region_translate( r, child->any.pos.x, child->any.pos.y );
            skin_region_union( p->any.region, r );
        }

        p->group.hasRegion = 1;
    }
}

/* return region in 'region' */
static void
skin_plate_get_region( SkinPlate*  p, SkinRegion*  region )
{
    if ( p->any.type != SKIN_PLATE_SURFACE && !p->group.hasRegion ) {
        skin_plate_ensure_region(p);
    }
    skin_region_init_copy( region, p->any.region );
}


/* recompute opaque region is needed */
static void
skin_plate_ensure_opaque_region( SkinPlate*  p )
{
    if (p->any.type != SKIN_PLATE_SURFACE && !p->group.hasOpaqueRegion) {
        int  n, count = areflist_count( p->group.children );

        skin_region_reset(p->group.opaqueRegion);

        for (n = 0; n < count; n++) {
            SkinRegion  r[1];
            SkinPlate*  child = areflist_get( p->group.children, n );

            skin_plate_get_opaque_region(child, r);
            skin_region_translate(r, child->any.pos.x, child->any.pos.y);
            skin_region_union( p->group.opaqueRegion, r);
        }

        p->group.hasOpaqueRegion = 1;
    }
}


/* return opaque pixels region */
static void
skin_plate_get_opaque_region( SkinPlate*  p, SkinRegion  *pregion )
{
    if ( p->any.type == SKIN_PLATE_SURFACE ) {
        if (p->any.isOpaque)
            skin_region_init_copy(pregion, p->any.region);
        else
            skin_region_reset(pregion);
    } else {
        skin_plate_ensure_opaque_region(p);
        skin_region_init_copy(pregion, p->group.opaqueRegion);
    }
}


/* invalidate region in parent groups */
static void
skin_plate_invalidate_parent( SkinPlate*  p )
{
    if (!p->any.isVisible)
        return;

    while (p) {
        if (p->any.type != SKIN_PLATE_SURFACE) {
            p->group.hasRegion       = 0;
            p->group.hasOpaqueRegion = 0;
        }
        p = p->any.parent;
    }
}


static void
skin_plate_invalidate_( SkinPlate*  p, SkinRegion*  r, SkinPlate*  child )
{
    if (p->any.type != SKIN_PLATE_SURFACE) {
        int  n = areflist_count( p->group.children );
        if (child != NULL) {
            n = areflist_indexOf( p->group.children, child );
        }
        while (n > 0) {
            n -= 1;
            child = areflist_get( p->group.children, n );
            skin_region_translate( r, child->any.pos.x, child->any.pos.y );
            skin_plate_invalidate_( p, r, NULL );
            skin_region_translate( r, -child->any.pos.x, -child->any.pos.y );
            if (skin_region_is_empty(r))
                return;
        }
        if (p->any.type != SKIN_PLATE_SPACE) {
            SkinPlate*  parent = p->any.parent;
            skin_region_translate(r, parent->any.pos.x, parent->any.pos.y );
            skin_plate_invalidate_(parent, r, p);
        } else {
            /* send to viewports */
            int  n, count = areflist_count( p->space.viewports );
            for (n = 0; n < count; n++) {
                SkinViewport*  v = areflist_get( p->space.viewports, n );
                skin_viewport_invalidate(v, r);
            }
        }
    }
}

static void
skin_plate_invalidate_region( SkinPlate*  p )
{
    SkinRegion  r[1];

    skin_plate_get_region( p, r );
    skin_plate_invalidate_(p->any.parent, r, p);
    skin_region_reset(r);
}

/* change visibility */
void
skin_plate_set_visible( SkinPlate*  p, int  isVisible )
{
    isVisible = !!isVisible;
    if (isVisible == p->any.isVisible)
        return;

    skin_plate_invalidate_parent(p);
    skin_plate_invalidate_region(p);
    p->any.isVisible = isVisible;
}

void
skin_plate_set_opaque( SkinPlate*  p, int  isOpaque )
{
    isOpaque = !!isOpaque;
    if (isOpaque == p->any.isOpaque)
        return;

    skin_plate_invalidate_parent(p);
    skin_plate_invalidate_region(p);
    p->any.isOpaque = isOpaque;
}



extern SkinPlate*
skin_plate_surface( SkinPlate*         parent,
                    SkinPos*           pos,
                    SkinRegion*        region,
                    void*              surface,
                    SkinPlateDrawFunc  draw,
                    SkinPlateDoneFunc  done )
{
    SkinPlate*  p;

    ANEW0(p);
    p->any.type      = SKIN_PLATE_SURFACE;
    p->any.parent    = parent;
    p->any.pos.x     = pos->x;
    p->any.pos.y     = pos->y;
    p->any.isVisible = 1;
    p->any.isOpaque  = 1;

    skin_region_init_copy( p->any.region, region );
    return p;
}


SkinPlate*
skin_plate_group( SkinPlate*  parent, SkinPos*  pos )
{
    SkinRegion  r[1];
    SkinPlate*  p;

    skin_region_reset(r);
    p = skin_plate_surface( parent, pos, r, NULL, NULL, NULL );
    p->any.type              = SKIN_PLATE_GROUP;
    p->group.hasOpaqueRegion = 0;
    skin_region_init_empty( p->group.opaqueRegion );

    areflist_init( p->group.children );
    return p;
}


SkinPlate*
skin_plate_space( void )
{
    SkinPos     pos;
    SkinPlate*  p;

    pos.x       = pos.y = 0;
    p           = skin_plate_group( NULL, &pos );
    p->any.type = SKIN_PLATE_SPACE;
    areflist_init( p->space.viewports );
    return p;
}


extern void
skin_plate_free( SkinPlate*  p )
{
    if (p->any.type >= SKIN_PLATE_SPACE) {
        while ( areflist_count( p->space.viewports ) )
            skin_viewport_free( areflist_get( p->space.viewports, 0 ) );
    }
    if (p->any.type >= SKIN_PLATE_GROUP) {
        skin_region_reset( p->group.opaqueRegion );
        p->group.hasOpaqueRegion = 0;
        p->group.hasRegion       = 0;

        while ( areflist_count( p->group.children ) )
            skin_plate_free( areflist_get( p->group.children, 0 ) );
    }
    if (p->any.type == SKIN_PLATE_SURFACE) {
        if (p->surface.done)
            p->surface.done( p->surface.user );
    }

    skin_region_reset( p->any.region );

    if (p->any.parent) {
        areflist_delFirst( p->any.parent->group.children, p );
    }
}

void
skin_plate_invalidate( SkinPlate*  plate, SkinRegion*  region )
{
    SkinRegion  r[1];
    skin_region_init_copy( r, region );
}


/* we use two regions to manage the front-to-back composition here
 *
 *  'updated' initially contains the update region, in parent coordinates
 *
 *  'drawn'   is initially empty, and will be filled with the region of translucent
 *            pixels that have been
 *
 *  for a given surface plate, we translate the regions to plate coordinates,
 *  then we do an opaque blit of 'intersection(updated,region)', then removing it from 'updated'
 *
 *  after that, we make a DSTOVER blit of 'intersection(drawn,region)'
 *  if the plate is not opaque, we add this intersection to 'drawn'
 *
 */
static void
skin_plate_redraw( SkinPlate*  plate, SkinRegion*  updated, SkinRegion*  drawn, SkinPos*  apos, SkinViewport*  viewport )
{
    SkinPos  pos = plate->any.pos;

    if (!plate->any.isVisible)
        return;

    if (skin_region_is_empty(updated) && skin_region_is_empty(drawn))
        return;

    /* translate regions to plate coordinates */
    skin_region_translate( updated, pos.x, pos.y );
    skin_region_translate( drawn,   pos.y, pos.y );
    apos->x += pos.x;
    apos->y += pos.y;

    if (plate->any.type == SKIN_PLATE_SURFACE) {
        SkinRegion  r[1];

        /* inter(updated,region) => opaque blit + remove 'region' from 'updated'*/
        skin_plate_get_region(plate, r);
        skin_region_intersect(r, updated);
        if (!skin_region_is_empty(r)) {
            plate->surface.draw( plate->surface.user, r, apos, viewport, 1 );
            skin_region_substract(updated, r);
            skin_region_reset(r);
        }

        /* inter(drawn,region) => DSTOVER blit + if non-opaque add it to 'drawn' */
        skin_plate_get_region(plate, r);
        skin_region_intersect(r, drawn);
        if (!skin_region_is_empty(r)) {
            plate->surface.draw( plate->surface.user, r, apos, viewport, 0);
            if (!plate->any.isOpaque)
                skin_region_union(drawn, r);
            skin_region_reset(r);
        }

    } else {
        int  n, count = areflist_count(plate->group.children);
        for (n = 0; n < count; n++) {
            SkinPos  pos;

            pos.x = apos->x + plate->any.pos.x;
            pos.y = apos->y + plate->any.pos.y;

            skin_plate_redraw( areflist_get(plate->group.children, n ), updated, drawn, &pos, viewport );
            if (skin_region_is_empty(updated) && skin_region_is_empty(drawn))
                break;
        }
    }

    /* convert back to parent coordinates */
    apos->x -= pos.x;
    apos->y -= pos.y;
    skin_region_translate( updated, -pos.x, -pos.y );
    skin_region_translate( drawn,   -pos.x, -pos.y );
}


extern SkinViewport*
skin_viewport( SkinPlate*  space, SkinRect*  rect, void*  surface, int  sx, int  sy )
{
    SkinViewport*  v;

    ANEW0(v);
    v->space   = space;
    v->rect    = rect[0];
    v->spos.x  = sx;
    v->spos.y  = sy;
    v->surface = surface;

    skin_region_init_empty( v->update );
    return v;
}

extern void
skin_viewport_free( SkinViewport*  v )
{
    SkinPlate*  space = v->space;
    if (space != NULL) {
        areflist_delFirst( space->space.viewports, v );
        v->space = NULL;
    }
    skin_region_reset( v->update );
    AFREE(v);
}

extern void
skin_viewport_invalidate( SkinViewport*  v, SkinRegion*  region )
{
    SkinRegion  r[1];
    skin_region_init_copy(r,region);
    skin_region_translate(r, -v->spos.x, -v->spos.y);
    skin_region_intersect_rect(r,&v->rect);
    skin_region_union( v->update, r );
    skin_region_reset(r);
}

extern void
skin_viewport_redraw( SkinViewport*  v )
{
    if (v->space && !skin_region_is_empty(v->update)) {
        SkinRegion  update[1];
        SkinRegion  drawn[1];
        SkinPos     apos;

        skin_region_copy(update, v->update);
        skin_region_reset(drawn);
        skin_region_reset( v->update );

        apos.x = apos.y = 0;
        skin_plate_redraw( v->space, update, drawn, &apos, v );

        skin_region_reset(update);
        skin_region_reset(drawn);
    }
}
