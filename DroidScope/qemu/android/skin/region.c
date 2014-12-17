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
#include "android/skin/region.h"
#include <limits.h>
#include <string.h>
#include <stdlib.h>  /* malloc/free */

/*************************************************************************
 *************************************************************************
 ****
 ****  ASSERTION SUPPORT
 ****
 ****
 ****/

#ifdef UNIT_TEST
#include <stdlib.h>
#include <stdio.h>
static void
_rpanic(void)
{
    *((char*)(void*)0) = 1;  /* should SEGFAULT */
    /* put a breakpoint here */
    exit(1);
}

#define  RASSERT(cond)   \
  ({ if (!(cond)) { fprintf(stderr, "%s:%d:%s: assertion failed: %s", \
      __FILE__, __LINE__,  __FUNCTION__, #cond ); _rpanic(); } })

#else
#define  RASSERT(cond)  ((void)0)
#endif


/*************************************************************************
 *************************************************************************
 ****
 ****  IMPLEMENTATION DETAILS
 ****
 ****
 ****/

/* this implementation of regions encodes the the region's spans with the
   following format:

   region   ::= yband+ YSENTINEL
   yband    ::= YTOP YBOTTOM scanline
   scanline ::= span+  XSENTINEL
   span     ::= XLEFT XRIGHT

   XSENTINEL ::= 0x7fff
   YSENTINEL :=  0x7fff

   all values are sorted in increasing order, which means that:

    - YTOP1 < YBOTTOM1 <= YTOP2 < YBOTTOM2 <= .... < YSENTINEL
    - XLEFT1 < XRIGHT1 < XLEFT2 < XRIGHT2 < .... < XSENTINEL
      (in a given scanline)
*/

/* convenience shortbuts */
typedef SkinRegionRun   Run;
typedef SkinRegion      Region;

#define  RUNS_RECT_COUNT  6   /* YTOP YBOT XLEFT XRIGHT XSENTINEL YSENTINEL */

#define  XSENTINEL  SKIN_REGION_SENTINEL
#define  YSENTINEL  SKIN_REGION_SENTINEL

#define  RUNS_EMPTY   ((Run*)(void*)(-1))
#define  RUNS_RECT    ((Run*)(void*)(0))

static __inline__ int
region_isEmpty( Region*  r )
{
    return r->runs == RUNS_EMPTY;
}

static __inline__ int
region_isRect( Region*  r )
{
    return r->runs == RUNS_RECT;
}

static __inline__ int
region_isComplex( Region*  r )
{
    return r->runs != RUNS_EMPTY && r->runs != RUNS_RECT;
}

/**  RunStore: ref-counted storage for runs
 **/

typedef struct {
    int  refcount;
    int  count;
} RunStore;

static void
runstore_free( RunStore*  s )
{
    free(s);
}

static RunStore*
runstore_alloc( int   count )
{
    RunStore*  s = malloc( sizeof(*s) + sizeof(Run)*count );
    RASSERT(s != NULL);
    s->count    = count;
    s->refcount = 1;
    return s;
}

static RunStore*
runstore_edit( RunStore*  s )
{
    RunStore*  s2;

    if (s->refcount == 1)
        return s;

    s2 = runstore_alloc( s->count );
    if (s2) {
        memcpy( s2, s, sizeof(*s) + s->count*sizeof(Run) );
        s->refcount -= 1;
        s2->refcount = 1;
    }
    return s2;
}

static void
runstore_unrefp( RunStore*  *ps )
{
    RunStore*  s = *ps;
    if (s != NULL) {
        if (s->refcount <= 0)
            runstore_free(s);
        *ps = NULL;
    }
}

static RunStore*
runstore_ref( RunStore*  s )
{
    if (s) s->refcount += 1;
    return s;
}

static __inline__ RunStore*
runstore_from_runs( Run*  runs )
{
    RASSERT(runs != RUNS_EMPTY);
    RASSERT(runs != RUNS_RECT);
    return (RunStore*)runs - 1;
}

static __inline__ Run*
runstore_to_runs( RunStore*  s )
{
    RASSERT(s != NULL);
    return (Run*)(s + 1);
}

static Run*
region_edit( Region*  r )
{
    RunStore*  s;

    RASSERT(region_isComplex(r));

    s = runstore_from_runs(r->runs);
    s = runstore_edit(s);
    r->runs = runstore_to_runs(s);
    return r->runs;
}

/** Run parsing
 **/

static Run*
runs_next_scanline( Run*  runs )
{
    RASSERT(runs[0] != YSENTINEL && runs[1] != YSENTINEL );
    runs += 2;
    do { runs += 1; } while (runs[-1] != XSENTINEL);
    return runs;
}

static Run*
runs_find_y( Run*  runs, int  y )
{
    do {
        int  ybot, ytop = runs[0];

        if (y < ytop)
            return NULL;

        ybot = runs[1];
        if (y < ybot)
            return runs;

        runs = runs_next_scanline( runs );
    } while (runs[0] != YSENTINEL);

    return NULL;
}

static void
runs_set_rect( Run*  runs, SkinRect*  rect )
{
    runs[0] = rect->pos.y;
    runs[1] = rect->pos.y + rect->size.h;
    runs[2] = rect->pos.x;
    runs[3] = rect->pos.x + rect->size.w;
    runs[4] = XSENTINEL;
    runs[5] = YSENTINEL;
}

static Run*
runs_copy_scanline( Run*  dst, Run*  src )
{
    RASSERT(src[0] != YSENTINEL);
    RASSERT(src[1] != YSENTINEL);
    dst[0] = src[0];
    dst[1] = src[1];
    src += 2;
    dst += 2;
    do { *dst++ = *src++; } while (src[-1] != XSENTINEL);
    return dst;
}

static Run*
runs_copy_scanline_adj( Run*  dst, Run*  src, int  ytop, int  ybot )
{
    Run*  runs2 = runs_copy_scanline( dst, src );
    dst[0] = (Run) ytop;
    dst[1] = (Run) ybot;
    return runs2;
}

static __inline__ Run*
runs_add_span( Run*  dst, int  left, int  right )
{
    dst[0] = (Run) left;
    dst[1] = (Run) right;
    return dst + 2;
}

static __inline__ int
runs_get_count( Run*  runs )
{
    RunStore*  s = runstore_from_runs(runs);
    return s->count;
}


static void
runs_coalesce_band( Run*  *psrc_spans, Run*  *pdst_spans, SkinBox*  minmax )
{
    Run*  sspan  = *psrc_spans;
    Run*  dspan  = *pdst_spans;
    int   pleft  = sspan[0];
    int   pright = sspan[1];
    int   xleft, xright;

    RASSERT(pleft != XSENTINEL);
    RASSERT(pright != XSENTINEL);
    RASSERT(pleft < pright);

    if (pleft < minmax->x1) minmax->x1 = pleft;

    sspan += 2;
    xleft  = sspan[0];

    while (xleft != XSENTINEL)
    {
        xright = sspan[1];
        RASSERT(xright != XSENTINEL);
        RASSERT(xleft < xright);

        if (xleft == pright) {
            pright = xright;
        } else {
            dspan[0] = (Run) pleft;
            dspan[1] = (Run) pright;
            dspan   += 2;
        }
        sspan += 2;
        xleft = sspan[0];
    }
    dspan[0] = (Run) pleft;
    dspan[1] = (Run) pright;
    dspan[2] = XSENTINEL;
    dspan   += 3;
    sspan   += 1;  /* skip XSENTINEL */

    if (pright > minmax->x2) minmax->x2 = pright;

    *psrc_spans = sspan;
    *pdst_spans = dspan;
}


static int
runs_coalesce( Run*  dst, Run*  src, SkinBox*  minmax )
{
    Run*  prev = NULL;
    Run*  dst0 = dst;
    int   ytop = src[0];
    int   ybot;

    while (ytop != YSENTINEL)
    {
        Run*  sspan = src + 2;
        Run*  dspan = dst + 2;

        ybot = src[1];
        RASSERT( ytop < ybot );
        RASSERT( ybot != YSENTINEL );
        RASSERT( src[2] != XSENTINEL );

        if (ytop < minmax->y1) minmax->y1 = ytop;
        if (ybot > minmax->y2) minmax->y2 = ybot;

        dst[0] = (Run) ytop;
        dst[1] = (Run) ybot;

        runs_coalesce_band( &sspan, &dspan, minmax );

        if (prev && prev[1] == dst[0] && (dst-prev) == (dspan-dst) &&
            !memcmp(prev+2, dst+2, (dspan-dst-2)*sizeof(Run)))
        {
            /* coalesce two identical bands */
            prev[1] = dst[1];
        }
        else
        {
            prev = dst;
            dst  = dspan;
        }
        src  = sspan;
        ytop = src[0];
    }
    dst[0] = YSENTINEL;
    return (dst + 1 - dst0);
}

/*************************************************************************
 *************************************************************************
 ****
 ****  PUBLIC API
 ****
 ****/

void
skin_region_init_empty( SkinRegion*  r )
{
    /* empty region */
    r->bounds.pos.x  = r->bounds.pos.y = 0;
    r->bounds.size.w = r->bounds.size.h = 0;
    r->runs = RUNS_EMPTY;
}

void
skin_region_init( SkinRegion*  r, int  x1, int  y1, int  x2, int  y2 )
{
    if (x1 >= x2 || y1 >= y2) {
        skin_region_init_empty(r);
        return;
    }
    r->bounds.pos.x  = x1;
    r->bounds.pos.y  = y1;
    r->bounds.size.w = x2 - x1;
    r->bounds.size.h = y2 - y1;
    r->runs = RUNS_RECT;
}

void
skin_region_init_rect( SkinRegion*  r, SkinRect*  rect )
{
    if (rect == NULL || rect->size.w <= 0 || rect->size.h <= 0) {
        skin_region_init_empty(r);
        return;
    }
    r->bounds = rect[0];
    r->runs   = RUNS_RECT;
}

void
skin_region_init_box( SkinRegion*  r, SkinBox*  box )
{
    if (box == NULL || box->x1 >= box->x2 || box->y1 >= box->y2) {
        skin_region_init_empty(r);
        return;
    }
    r->bounds.pos.x  = box->x1;
    r->bounds.pos.y  = box->y1;
    r->bounds.size.w = box->x2 - box->x1;
    r->bounds.size.h = box->y2 - box->y1;
    r->runs = RUNS_RECT;
}

void
skin_region_init_copy( SkinRegion*  r, SkinRegion*  src )
{
    if (src == NULL || region_isEmpty(src))
        skin_region_init_empty(r);
    else {
        r[0] = src[0];
        if (region_isComplex(src)) {
            RunStore*  s = runstore_from_runs(r->runs);
            runstore_ref(s);
        }
    }
}


void
skin_region_reset( SkinRegion*  r )
{
    if (r != NULL) {
        if (region_isComplex(r)) {
            RunStore*  s = runstore_from_runs(r->runs);
            runstore_unrefp( &s );
        }
        skin_region_init_empty(r);
    }
}



void
skin_region_copy( SkinRegion*  r, SkinRegion*  src )
{
    skin_region_reset(r);
    skin_region_init_copy(r, src);
}


int
skin_region_equals( SkinRegion*  r1, SkinRegion*  r2 )
{
    Run       *runs1, *runs2;
    RunStore  *store1, *store2;

    if (r1 == r2)
        return 1;

    if (!skin_rect_equals( &r1->bounds, &r2->bounds ))
        return 0;

    runs1 = r1->runs;
    runs2 = r2->runs;

    if (runs1 == runs2)  /* empties and rects */
        return 1;

    if ( !region_isComplex(r1) || !region_isComplex(r2) )
        return 0;

    store1 = runstore_from_runs(runs1);
    store2 = runstore_from_runs(runs2);

    if (store1->count == store2->count &&
        !memcmp( (char*)runs1, (char*)runs2, store1->count*sizeof(Run) ) )
        return 1;

    return 0;
}

void
skin_region_translate( SkinRegion*  r, int  dx, int  dy )
{
    Run*  runs;

    if (region_isEmpty(r))
        return;

    skin_rect_translate( &r->bounds, dx, dy );
    if (region_isRect(r))
        return;

    runs = region_edit(r);
    while (runs[0] != YSENTINEL) {
        int  ytop = runs[0];
        int  ybot = runs[1];

        RASSERT(ybot != YSENTINEL);
        runs[0] = (Run)(ytop + dy);
        runs[1] = (Run)(ybot + dy);
        runs += 2;
        while (runs[0] != XSENTINEL) {
            int  xleft  = runs[0];
            int  xright = runs[1];
            RASSERT(xright != YSENTINEL);
            runs[0] = (Run)(xleft + dx);
            runs[1] = (Run)(xright + dx);
            runs += 2;
        }
        runs += 1;
    }
}

void
skin_region_get_bounds( SkinRegion*  r, SkinRect*  bounds )
{
    if (r != NULL) {
        bounds[0] = r->bounds;
    } else {
        bounds->pos.x  = bounds->pos.y  = 0;
        bounds->size.w = bounds->size.h = 0;
    }
}

int
skin_region_is_empty( SkinRegion*  r )
{
    return region_isEmpty(r);
}

int
skin_region_is_rect( SkinRegion*  r )
{
    return region_isRect(r);
}

int
skin_region_is_complex( SkinRegion*  r )
{
    return region_isComplex(r);
}

void
skin_region_swap( SkinRegion*  r, SkinRegion*  r2 )
{
    SkinRegion  tmp;
    tmp   = r[0];
    r[0]   = r2[0];
    r2[0]  = tmp;
}


SkinOverlap
skin_region_contains( SkinRegion*  r, int  x, int  y )
{
    if (region_isEmpty(r))
        return SKIN_OUTSIDE;
    if (region_isRect(r)) {
        return skin_rect_contains(&r->bounds,x,y);
    } else {
        Run*  runs = runs_find_y( r->runs, y );
        if (runs != NULL) {
            runs += 2;
            do {
                int  xright, xleft = runs[0];

                if (x < xleft)  // also x < xleft == XSENTINEL
                    break;
                xright = runs[1];
                if (xright == XSENTINEL)
                    break;
                if (x < xright)
                    return SKIN_INSIDE;
                runs += 2;
            } while (runs[0] != XSENTINEL);
        }
        return SKIN_OUTSIDE;
    }
}


SkinOverlap
skin_region_contains_rect( SkinRegion*  r, SkinRect*  rect )
{
    SkinRegion  r2[1];
    skin_region_init_rect( r2, rect );
    return skin_region_test_intersect( r, r2 );
}


SkinOverlap
skin_region_contains_box( SkinRegion*  r, SkinBox*  b )
{
    SkinRegion  r2[1];

    skin_region_init_box( r2, b );
    return skin_region_test_intersect( r, r2 );
}



#define FLAG_REGION_1     (1 << 0)
#define FLAG_REGION_2     (1 << 1)
#define FLAG_REGION_BOTH  (1 << 2)

SkinOverlap
skin_region_test_intersect( SkinRegion*  r1,
                            SkinRegion*  r2 )
{
    Run  *runs1, *runs2;
    Run   run2_tmp[ RUNS_RECT_COUNT ];
    SkinRect  r;

    if (region_isEmpty(r1) || region_isEmpty(r2))
        return SKIN_OUTSIDE;

    if ( !skin_rect_intersect( &r, &r1->bounds, &r2->bounds) )
        return SKIN_OUTSIDE;

    if (region_isRect(r1)) {
        if (region_isRect(r2)) {
            return skin_rect_contains_rect(&r1->bounds, &r2->bounds);
        } else {
            SkinRegion*  tmp = r1;
            r1 = r2;
            r2 = tmp;
        }
    }
    /* here r1 is guaranteed to be complex, r2 is either rect of complex */
    runs1 = r1->runs;
    if (region_isRect(r2)) {
        runs2 = run2_tmp;
        runs_set_rect(runs2, &r2->bounds);
    }
    else {
        runs2 = r2->runs;
    }

    {
        int   flags = 0;

        while (runs1[0] != YSENTINEL && runs2[0] != YSENTINEL)
        {
            int  ytop1 = runs1[0];
            int  ybot1 = runs1[1];
            int  ytop2 = runs2[0];
            int  ybot2 = runs2[1];

            if (ybot1 <= ytop2)
            {
                /* band1 over band2 */
                flags |= FLAG_REGION_1;
                runs1 = runs_next_scanline( runs1 );
            }
            else if (ybot2 <= ytop1)
            {
                /* band2 over band1 */
                flags |= FLAG_REGION_2;
                runs2  = runs_next_scanline( runs2 );
            }
            else  /* band1 and band2 overlap */
            {
                Run*  span1;
                Run*  span2;
                int   ybot;

                if (ytop1 < ytop2) {
                    flags |= FLAG_REGION_1;
                    ytop1 = ytop2;
                } else if (ytop2 < ytop1) {
                    flags |= FLAG_REGION_2;
                    ytop2 = ytop1;
                }

                ybot = (ybot1 < ybot2) ? ybot1 : ybot2;

                span1 = runs1 + 2;
                span2 = runs2 + 2;

                while (span1[0] != XSENTINEL && span2[0] != XSENTINEL)
                {
                    int  xleft1  = span1[0];
                    int  xright1 = span1[1];
                    int  xleft2  = span2[0];
                    int  xright2 = span2[1];

                    RASSERT(xright1 != XSENTINEL);
                    RASSERT(xright2 != XSENTINEL);

                    if (xright1 <= xleft2) {
                        flags |= FLAG_REGION_1;
                        span1 += 2;
                    }
                    else if (xright2 <= xleft1) {
                        flags |= FLAG_REGION_2;
                        span2 += 2;
                    }
                    else {
                        int  xright;

                        if (xleft1 < xleft2) {
                            flags |= FLAG_REGION_1;
                            xleft1 = xleft2;
                        } else if (xleft2 < xleft1) {
                            flags |= FLAG_REGION_2;
                            xleft2 = xleft1;
                        }

                        xright = (xright1 < xright2) ? xright1 : xright2;

                        flags |= FLAG_REGION_BOTH;

                        if (xright == xright1)
                            span1 += 2;
                        if (xright == xright2)
                            span2 += 2;
                    }
                }

                if (span1[0] != XSENTINEL) {
                    flags |= FLAG_REGION_1;
                }

                if (span2[0] != XSENTINEL) {
                    flags |= FLAG_REGION_2;
                }

                if (ybot == ybot1)
                    runs1 = runs_next_scanline( runs1 );

                if (ybot == ybot2)
                    runs2 = runs_next_scanline( runs2 );
            }
        }

        if (runs1[0] != YSENTINEL) {
            flags |= FLAG_REGION_1;
        }

        if (runs2[0] != YSENTINEL) {
            flags |= FLAG_REGION_2;
        }

        if ( !(flags & FLAG_REGION_BOTH) ) {
            /* no intersection at all */
            return SKIN_OUTSIDE;
        }

        if ( (flags & FLAG_REGION_2) != 0 ) {
            /* intersection + overlap */
            return SKIN_OVERLAP;
        }

        return SKIN_INSIDE;
    }
}

typedef struct {
    Run*       runs1;
    Run*       runs2;
    Run*       runs_base;
    Run*       runs;
    RunStore*  store;
    Region     result[1];
    Run        runs1_rect[ RUNS_RECT_COUNT ];
    Run        runs2_rect[ RUNS_RECT_COUNT ];
} RegionOperator;


static void
region_operator_init( RegionOperator*  o,
                      Region*          r1,
                      Region*          r2 )
{
    int  run1_count, run2_count;
    int  maxruns;

    RASSERT( !region_isEmpty(r1) );
    RASSERT( !region_isEmpty(r2) );

    if (region_isRect(r1)) {
        run1_count = RUNS_RECT_COUNT;
        o->runs1   = o->runs1_rect;
        runs_set_rect( o->runs1, &r1->bounds );
    } else {
        o->runs1   = r1->runs;
        run1_count = runs_get_count(r1->runs);
    }

    if (region_isRect(r2)) {
        run2_count = RUNS_RECT_COUNT;
        o->runs2   = o->runs2_rect;
        runs_set_rect( o->runs2, &r2->bounds );
    } else {
        o->runs2   = r2->runs;
        run2_count = runs_get_count(r2->runs);
    }

    maxruns  = run1_count < run2_count ? run2_count : run1_count;
    o->store = runstore_alloc( 3*maxruns );
    o->runs_base = runstore_to_runs(o->store);
}


static void
region_operator_do( RegionOperator*  o, int  wanted )
{
    Run*  runs1 = o->runs1;
    Run*  runs2 = o->runs2;
    Run*  runs  = o->runs_base;
    int   ytop1 = runs1[0];
    int   ytop2 = runs2[0];

    if (ytop1 != YSENTINEL && ytop2 != YSENTINEL)
    {
        int  ybot1, ybot2;

        while (ytop1 != YSENTINEL && ytop2 != YSENTINEL)
        {
            int  ybot;

            ybot1 = runs1[1];
            ybot2 = runs2[1];

            RASSERT(ybot1 != YSENTINEL);
            RASSERT(ybot2 != YSENTINEL);

            if (ybot1 <= ytop2) {
                if (wanted & FLAG_REGION_1)
                    runs = runs_copy_scanline_adj( runs, runs1, ytop1, ybot1 );
                runs1   = runs_next_scanline( runs1 );
                ytop1   = runs1[0];
                continue;
            }

            if (ybot2 <= ytop1) {
                if (wanted & FLAG_REGION_2)
                    runs = runs_copy_scanline_adj( runs, runs2, ytop2, ybot2 );
                runs2 = runs_next_scanline(runs2);
                ytop2 = runs2[0];
                continue;
            }

            if (ytop1 < ytop2) {
                if (wanted & FLAG_REGION_1)
                    runs = runs_copy_scanline_adj( runs, runs1, ytop1, ytop2 );
                ytop1 = ytop2;
            }
            else if (ytop2 < ytop1) {
                if (wanted & FLAG_REGION_2)
                    runs = runs_copy_scanline_adj( runs, runs2, ytop2, ytop1 );
                ytop2 = ytop1;
            }

            ybot  = (ybot1 <= ybot2) ? ybot1 : ybot2;

            runs[0] = (Run) ytop1;
            runs[1] = (Run) ybot;

            /* do the common band */
            {
                Run*  span1 = runs1 + 2;
                Run*  span2 = runs2 + 2;
                Run*  span  = runs + 2;
                int   xleft1 = span1[0];
                int   xleft2 = span2[0];
                int   xright1, xright2;

                while (xleft1 != XSENTINEL && xleft2 != XSENTINEL)
                {
                    int  xright;

                    xright1 = span1[1];
                    xright2 = span2[1];

                    RASSERT(xright1 != XSENTINEL);
                    RASSERT(xright2 != XSENTINEL);

                    if (xright1 <= xleft2) {
                        if (wanted & FLAG_REGION_1)
                            span = runs_add_span( span, xleft1, xright1 );
                        span1 += 2;
                        xleft1 = span1[0];
                        continue;
                    }

                    if (xright2 <= xleft1) {
                        if (wanted & FLAG_REGION_2)
                            span = runs_add_span( span, xleft2, xright2 );
                        span2 += 2;
                        xleft2 = span2[0];
                        continue;
                    }

                    if (xleft1 < xleft2) {
                        if (wanted & FLAG_REGION_1)
                            span = runs_add_span( span, xleft1, xleft2 );
                        xleft1 = xleft2;
                    }

                    else if (xleft2 < xleft1) {
                        if (wanted & FLAG_REGION_2)
                            span = runs_add_span( span, xleft2, xleft1 );
                        xleft2 = xleft1;
                    }

                    xright = (xright1 <= xright2) ? xright1 : xright2;

                    if (wanted & FLAG_REGION_BOTH)
                        span = runs_add_span( span, xleft1, xright );

                    xleft1 = xleft2 = xright;

                    if (xright == xright1) {
                        span1 += 2;
                        xleft1 = span1[0];
                    }
                    if (xright == xright2) {
                        span2 += 2;
                        xleft2 = span2[0];
                    }
                }

                if (wanted & FLAG_REGION_1) {
                    while (xleft1 != XSENTINEL) {
                        RASSERT(span1[1] != XSENTINEL);
                        span[0] = (Run) xleft1;
                        span[1] = span1[1];
                        span   += 2;
                        span1  += 2;
                        xleft1  = span1[0];
                    }
                }

                if (wanted & FLAG_REGION_2) {
                    while (xleft2 != XSENTINEL) {
                        RASSERT(span2[1] != XSENTINEL);
                        span[0] = (Run) xleft2;
                        span[1] = span2[1];
                        span   += 2;
                        span2  += 2;
                        xleft2  = span2[0];
                    }
                }

                if (span > runs + 2) {
                    span[0] = XSENTINEL;
                    runs    = span + 1;
                }
            }

            ytop1 = ytop2 = ybot;

            if (ybot == ybot1) {
                runs1 = runs_next_scanline( runs1 );
                ytop1 = runs1[0];
            }
            if (ybot == ybot2) {
                runs2 = runs_next_scanline( runs2 );
                ytop2 = runs2[0];
            }
        }
    }

    if ((wanted & FLAG_REGION_1) != 0) {
        while (ytop1 != YSENTINEL) {
            runs = runs_copy_scanline_adj( runs, runs1, ytop1, runs1[1] );
            runs1 = runs_next_scanline(runs1);
            ytop1 = runs1[0];
        }
    }

    if ((wanted & FLAG_REGION_2) != 0) {
        while (ytop2 != YSENTINEL) {
            runs = runs_copy_scanline_adj( runs, runs2, ytop2, runs2[1] );
            runs2 = runs_next_scanline(runs2);
            ytop2 = runs2[0];
        }
    }

    runs[0] = YSENTINEL;
    o->runs = runs + 1;
}

/* returns 1 if the result is not empty */
static int
region_operator_done( RegionOperator*  o )
{
    Run*       src = o->runs;
    int        count;
    SkinBox    minmax;
    RunStore*  store;

    if (src <= o->runs_base + 1) {
        /* result is empty */
        skin_region_init_empty( o->result );
        return 0;
    }

    /* coalesce the temp runs in-place and compute the corresponding bounds */
    minmax.x1 = minmax.y1 = INT_MAX;
    minmax.x2 = minmax.y2 = INT_MIN;

    count = runs_coalesce( o->runs_base, o->runs_base, &minmax );
    if (count == 1) {
        /* result is empty */
        skin_region_init_empty( o->result );
    }
    else
    {
        skin_box_to_rect( &minmax, &o->result->bounds );
        if (count == RUNS_RECT_COUNT) {
            o->result->runs = RUNS_RECT;
        }
        else
        {
            /* result is complex */
            store = runstore_alloc( count );
            o->result->runs = runstore_to_runs(store);
            memcpy( o->result->runs, o->runs_base, count*sizeof(Run) );
        }
    }

    /* release temporary runstore */
    runstore_unrefp( &o->store );

    return region_isEmpty(o->result);
}



int
skin_region_intersect( SkinRegion*  r, SkinRegion*  r2 )
{
    RegionOperator  oper[1];

    if (region_isEmpty(r))
        return 0;

    if (region_isEmpty(r2))
        return 1;

    if ( skin_rect_contains_rect( &r->bounds, &r2->bounds ) == SKIN_OUTSIDE ) {
        skin_region_init_empty(r);
        return 0;
    }

    region_operator_init( oper, r, r2 );
    region_operator_do( oper, FLAG_REGION_BOTH );
    region_operator_done( oper );

    skin_region_swap( r, oper->result );
    skin_region_reset( oper->result );

    return region_isEmpty( r );
}


/* performs r = (intersect r (region+_from_rect rect)), returns true iff
   the resulting region is not empty */
int
skin_region_intersect_rect( SkinRegion*  r, SkinRect*  rect )
{
    Region  r2[1];

    skin_region_init_rect( r2, rect );
    return skin_region_intersect( r, r2 );
}

/* performs r = (union r r2) */
void
skin_region_union( SkinRegion*  r, SkinRegion*  r2 )
{
    RegionOperator  oper[1];

    if (region_isEmpty(r)) {
        skin_region_copy(r, r2);
        return;
    }

    if (region_isEmpty(r2))
        return;

    region_operator_init( oper, r, r2 );
    region_operator_do( oper, FLAG_REGION_1|FLAG_REGION_2|FLAG_REGION_BOTH );
    region_operator_done( oper );

    skin_region_swap( r, oper->result );
    skin_region_reset( oper->result );
}

void
skin_region_union_rect( SkinRegion*  r, SkinRect*  rect )
{
    Region  r2[1];

    skin_region_init_rect(r2, rect);
    return skin_region_union( r, r2 );
}

/* performs r = (difference r r2) */
void
skin_region_substract( SkinRegion*  r, SkinRegion*  r2 )
{
    RegionOperator  oper[1];

    if (region_isEmpty(r) || region_isEmpty(r2))
        return;

    if ( skin_rect_contains_rect( &r->bounds, &r2->bounds ) == SKIN_OUTSIDE ) {
        skin_region_init_empty(r);
        return;
    }

    region_operator_init( oper, r, r2 );
    region_operator_do( oper, FLAG_REGION_1 );
    region_operator_done( oper );

    skin_region_swap( r, oper->result );
    skin_region_reset( oper->result );
}

void
skin_region_substract_rect( SkinRegion*  r, SkinRect*  rect )
{
    Region  r2[1];

    skin_region_init_rect(r2, rect);
    return skin_region_substract( r, r2 );
}

/* performs r = (xor r r2) */
void
skin_region_xor( SkinRegion*  r, SkinRegion*  r2 )
{
    RegionOperator  oper[1];

    if (region_isEmpty(r)) {
        skin_region_copy(r, r2);
        return;
    }

    if (region_isEmpty(r2))
        return;

    if ( skin_rect_contains_rect( &r->bounds, &r2->bounds ) == SKIN_OUTSIDE ) {
        skin_region_init_empty(r);
        return;
    }

    region_operator_init( oper, r, r2 );
    region_operator_do( oper, FLAG_REGION_1 );
    region_operator_done( oper );

    skin_region_swap( r, oper->result );
    skin_region_reset( oper->result );
}


void
skin_region_iterator_init( SkinRegionIterator*  iter,
                           SkinRegion*          region )
{
    iter->region = region;
    iter->band   = NULL;
    iter->span   = NULL;
}

int
skin_region_iterator_next( SkinRegionIterator*  iter, SkinRect  *rect )
{
    static const Run dummy[ 2 ] = { XSENTINEL, YSENTINEL };

    Run*  span = iter->span;
    Run*  band = iter->band;

    if (span == NULL) {
        Region*  r = iter->region;
        if (region_isEmpty(r))
            return 0;
        if (region_isRect(r)) {
            rect[0] = r->bounds;
            iter->span = (Run*) dummy;
            return 1;
        }
        iter->band = band = r->runs;
        iter->span = span = r->runs + 2;
    }
    else if (band == NULL)
        return 0;

    while (span[0] == XSENTINEL) {
        band = span + 1;
        if (band[0] == YSENTINEL || band[1] == YSENTINEL)
            return 0;

        iter->band = band;
        iter->span = span = band + 2;
    }

    if (span[1] == XSENTINEL)
        return 0;

    rect->pos.y  = band[0];
    rect->pos.x  = span[0];
    rect->size.h = band[1] - band[0];
    rect->size.w = span[1] - span[0];

    iter->span = span + 2;
    return 1;
}

int
skin_region_iterator_next_box( SkinRegionIterator*  iter, SkinBox  *box )
{
    SkinRect  rect;
    int       result = skin_region_iterator_next( iter, &rect );

    if (result)
        skin_box_from_rect( box, &rect );

    return result;
}

#ifdef UNIT_TEST

#include <stdio.h>
#include <stdlib.h>
#include "skin_rect.c"

static void
panic(void)
{
    *((char*)0) = 1;
    exit(0);
}

static void
_expectCompare( Region*  r, const SkinBox*  boxes, int  count )
{
    if (count == 0) {
        if ( !skin_region_is_empty(r) ) {
            printf( " result is not empty\n" );
            panic();
        }
    }
    else if (count == 1) {
        SkinRect  rect1, rect2;
        if ( !skin_region_is_rect(r) ) {
            printf( " result is not a rectangle\n" );
            panic();
        }
        skin_region_get_bounds( r, &rect1 );
        skin_box_to_rect( (SkinBox*)boxes, &rect2 );
        if ( !skin_rect_equals( &rect1, &rect2 ) ) {
            printf( " result is (%d,%d,%d,%d), expected (%d,%d,%d,%d)\n",
                    rect1.pos.x, rect1.pos.y,
                    rect1.pos.x + rect1.size.w, rect1.pos.y + rect1.size.h,
                    rect2.pos.x, rect2.pos.y,
                    rect2.pos.x + rect2.size.w, rect2.pos.y + rect2.size.h );
            panic();
        }
    }
    else {
        SkinRegionIterator  iter;
        SkinBox             b;
        int                 n;

        skin_region_iterator_init( &iter, r );
        n = 0;
        while (n < count) {
            if ( !skin_region_iterator_next_box( &iter, &b ) ) {
                printf( "missing region box (%d, %d, %d, %d)\n",
                        boxes->x1, boxes->y1, boxes->x2, boxes->y2 );
                panic();
            }

            if (b.x1 != boxes->x1 || b.x2 != boxes->x2||
                b.y1 != boxes->y1 || b.y2 != boxes->y2)
            {
                printf( "invalid region box (%d,%d,%d,%d) expecting (%d,%d,%d,%d)\n",
                        b.x1, b.y1, b.x2, b.y2,
                        boxes->x1, boxes->y1, boxes->x2, boxes->y2 );
                panic();
            }
            boxes += 1;
            n += 1;
        }

        if ( skin_region_iterator_next_box( &iter, &b ) ) {
            printf( "excess region box (%d,%d,%d,%d)\n",
                    b.x1, b.y1, b.x2, b.y2 );
            panic();
        }
    }
}


static void
expectEmptyRegion( Region*  r )
{
    printf( "expectEmptyRegion: " );
    if (!skin_region_is_empty(r)) {
        printf( "region not empty !!\n" );
        panic();
    }
    printf( "ok\n" );
}

static void
expectTestIntersect( Region*  r1, Region*  r2, SkinOverlap  overlap )
{
    SkinOverlap  result;
    printf( "expectTestIntersect(%d): ", overlap );
    result = skin_region_test_intersect(r1, r2);
    if (result != overlap) {
        printf( "bad result %d, expected %d\n", result, overlap );
        panic();
    }
    printf( "ok\n" );
}

static void
expectRectRegion( Region*  r, int  x1, int  y1, int  x2, int  y2 )
{
    SkinRect  rect;
    SkinBox   b;

    printf( "expectRectRegion(%d,%d,%d,%d): ",x1,y1,x2,y2 );
    if (!skin_region_is_rect(r)) {
        printf( "region not rect !!\n" );
        panic();
    }

    skin_region_get_bounds( r, &rect );
    skin_box_from_rect( &b, &rect );

    if (b.x1 != x1 || b.x2 != x2 || b.y1 != y1 || b.y2 != y2) {
        printf( "rect region bounds are (%d,%d,%d,%d), expecting (%d,%d,%d,%d)\n",
               b.x1, b.y1, b.x2, b.y2, x1, y1, x2, y2 );
        panic();
    }
    printf( "ok\n" );
}

static void
expectComplexRegion( Region* r, const SkinBox*  boxes, int  count )
{
    SkinRegionIterator  iter;
    SkinBox             b;
    int                 n;

    printf( "expectComplexRegion(): " );
    if (!skin_region_is_complex(r)) {
        printf( "region is not complex !!\n" );
        panic();
    }
    _expectCompare( r, boxes, count );
    printf( "ok\n" );
}

static void
expectIntersect( Region*  r1, Region*  r2, const SkinBox*  boxes, int  count )
{
    SkinRegion  r[1];

    printf( "expectIntersect(%d): ", count );
    skin_region_init_copy( r, r1 );
    skin_region_intersect( r, r2 );
    _expectCompare( r, boxes, count );
    printf( "ok\n" );
}

static void
expectUnion( Region*  r1, Region*  r2, const SkinBox*  boxes, int  count )
{
    SkinRegion  r[1];

    printf( "expectUnion(%d): ", count );
    skin_region_init_copy( r, r1 );
    skin_region_union( r, r2 );
    _expectCompare( r, boxes, count );
    printf( "ok\n" );
}


static void
expectSubstract( Region*  r1, Region*  r2, const SkinBox*  boxes, int  count )
{
    SkinRegion  r[1];

    printf( "expectSubstract(%d): ", count );
    skin_region_init_copy( r, r1 );
    skin_region_substract( r, r2 );
    _expectCompare( r, boxes, count );
    printf( "ok\n" );
}


int main( void )
{
    SkinRegion  r[1], r2[1];

    skin_region_init_empty( r );
    expectEmptyRegion( r );

    skin_region_init( r, 10, 20, 110, 120 );
    expectRectRegion( r, 10, 20, 110, 120 );

    skin_region_translate( r, 50, 80 );
    expectRectRegion( r, 60, 100, 160, 200 );

    skin_region_init( r, 10, 10, 40, 40 );
    skin_region_init( r2, 20, 20, 50, 50 );
    expectTestIntersect( r, r2, SKIN_OVERLAP );

    skin_region_translate(r2, +20, + 20 );
    expectTestIntersect( r, r2, SKIN_OUTSIDE );

    skin_region_translate(r2, -30, -30 );
    expectTestIntersect( r, r2, SKIN_INSIDE );

    {
        static const SkinBox  result1[1] = {
            { 20, 20, 40, 40 }
        };
        static const SkinBox  result2[3] = {
            { 10, 10, 40, 20 },
            { 10, 20, 50, 40 },
            { 20, 40, 50, 50 },
        };
        static const SkinBox  result3[2] = {
            { 10, 10, 40, 20 },
            { 10, 20, 20, 40 },
        };

        skin_region_init( r, 10, 10, 40, 40 );
        skin_region_init( r2, 20, 20, 50, 50 );
        expectIntersect( r, r2, result1, 1 );
        expectUnion( r, r2, result2, 3 );
        expectSubstract( r, r2, result3, 2 );
    }

    return 0;
}

#endif /* UNIT_TEST */
