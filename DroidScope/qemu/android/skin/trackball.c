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
#include "android/skin/trackball.h"
#include "android/skin/image.h"
#include "android/utils/system.h"
#include "user-events.h"
#include <math.h>

/***********************************************************************/
/***********************************************************************/
/*****                                                             *****/
/*****       T R A C K   B A L L                                   *****/
/*****                                                             *****/
/***********************************************************************/
/***********************************************************************/

// a 3-d vector
typedef  double   VectorRec[3];
typedef  double*  Vector;

/* define FIX16_IS_FLOAT to use floats for computations */
#define  FIX16_IS_FLOAT

#ifdef FIX16_IS_FLOAT
typedef float   Fix16;
#define  FIX16_ONE           1.0
#define  FIX16_FROM_FLOAT(x)  (x)
#define  FIX16_TO_FLOAT(x)    (x)

#else
typedef int     Fix16;

#define  FIX16_SHIFT  16
#define  FIX16_ONE            (1 << FIX16_SHIFT)
#define  FIX16_FROM_FLOAT(x)  (Fix16)((x) * FIX16_ONE)
#define  FIX16_TO_FLOAT(x)    ((x)/(1.0*FIX16_ONE))

#endif

typedef Fix16   Fix16VectorRec[3];
typedef Fix16*  Fix16Vector;

static Fix16
fixedvector_len( Fix16Vector  v )
{
    double  x = FIX16_TO_FLOAT(v[0]);
    double  y = FIX16_TO_FLOAT(v[1]);
    double  z = FIX16_TO_FLOAT(v[2]);
    double  len = sqrt( x*x + y*y + z*z );

    return FIX16_FROM_FLOAT(len);
}

static void
fixedvector_from_vector( Fix16Vector  f, Vector  v )
{
    f[0] = FIX16_FROM_FLOAT(v[0]);
    f[1] = FIX16_FROM_FLOAT(v[1]);
    f[2] = FIX16_FROM_FLOAT(v[2]);
}


#ifdef FIX16_IS_FLOAT
static double
fixedvector_dot( Fix16Vector  u, Fix16Vector  v )
{
    return u[0]*v[0] + u[1]*v[1] + u[2]*v[2];
}
#else
static Fix16
fixedvector_dot( Fix16Vector  u, Fix16Vector  v )
{
    long long  t;

    t = (long long)u[0] * v[0] + (long long)u[1] * v[1] + (long long)u[2] * v[2];
    return (Fix16)(t >> FIX16_SHIFT);
}
#endif

static int
norm( int  dx, int  dy )
{
    return (int) sqrt( dx*1.0*dx + dy*1.0*dy );
}

/*** ROTATOR: used to rotate the reference axis when mouse motion happens
 ***/

typedef struct
{
    VectorRec   d;
    VectorRec   n;
    double      angle;

} RotatorRec, *Rotator;


#define  ANGLE_FACTOR  (M_PI/200)

static void
rotator_reset( Rotator  rot, int  dx, int  dy )
{
    double  len = sqrt( dx*dx + dy*dy );
    double  zx, zy;

    if (len < 1e-3 ) {
        zx = 1.;
        zy = 0;
    } else {
        zx = dx / len;
        zy = dy / len;
    }
    rot->d[0] = zx;
    rot->d[1] = zy;
    rot->d[2] = 0.;

    rot->n[0] = -rot->d[1];
    rot->n[1] =  rot->d[0];
    rot->n[2] = 0;

    rot->angle = len * ANGLE_FACTOR;
}

static void
rotator_apply( Rotator  rot, double*  vec )
{
    double   d, n, z, d2, z2, cs, sn;

    /* project on D, N, Z */
    d = vec[0]*rot->d[0] + vec[1]*rot->d[1];
    n = vec[0]*rot->n[0] + vec[1]*rot->n[1];
    z = vec[2];

    /* rotate on D, Z */
    cs = cos( rot->angle );
    sn = sin( rot->angle );

    d2 =  cs*d + sn*z;
    z2 = -sn*d + cs*z;

    /* project on X, Y, Z */
    vec[0] = d2*rot->d[0] + n*rot->n[0];
    vec[1] = d2*rot->d[1] + n*rot->n[1];
    vec[2] = z2;
}

/*** TRACKBALL OBJECT
 ***/
typedef struct { int  x, y, offset, alpha; Fix16VectorRec  f; } SphereCoordRec, *SphereCoord;

typedef struct SkinTrackBall
{
    int             diameter;
    unsigned*       pixels;
    SDL_Surface*    surface;
    VectorRec       axes[3];  /* current ball axes */

#define  DOT_GRID        3                        /* number of horizontal and vertical cells per side grid */
#define  DOT_CELLS       2                        /* number of random dots per cell */
#define  DOT_MAX         (6*DOT_GRID*DOT_GRID*DOT_CELLS)  /* total number of dots */
#define  DOT_RANDOM_X    1007                     /* horizontal random range in each cell */
#define  DOT_RANDOM_Y    1007                     /* vertical random range in each cell */

#define  DOT_THRESHOLD  FIX16_FROM_FLOAT(0.17)

    Fix16VectorRec   dots[ DOT_MAX ];

    SphereCoordRec*  sphere_map;
    int              sphere_count;

    unsigned         ball_color;
    unsigned         dot_color;
    unsigned         ring_color;

    Uint32           ticks_last;  /* ticks since last move */
    int              acc_x;
    int              acc_y;
    int              acc_threshold;
    double           acc_scale;

    /* rotation applied to events send to the system */
    SkinRotation     rotation;

} TrackBallRec, *TrackBall;


/* The following constants are used to better mimic a real trackball.
 *
 * ACC_THRESHOLD is used to filter small ball movements out.
 * If the length of the relative mouse motion is smaller than this
 * constant, then no corresponding ball event will be sent to the
 * system.
 *
 * ACC_SCALE is used to scale the relative mouse motion vector into
 * the corresponding ball motion vector.
 */
#define  ACC_THRESHOLD  20
#define  ACC_SCALE      0.2

static void
trackball_init( TrackBall  ball, int  diameter, int  ring,
                unsigned   ball_color, unsigned  dot_color,
                unsigned   ring_color )
{
    int  diameter2 = diameter + ring*2;

    memset( ball, 0, sizeof(*ball) );

    ball->acc_threshold = ACC_THRESHOLD;
    ball->acc_scale     = ACC_SCALE;

    /* init SDL surface */
    ball->diameter   = diameter2;
    ball->ball_color = ball_color;
    ball->dot_color  = dot_color;
    ball->ring_color = ring_color;

    ball->rotation   = SKIN_ROTATION_0;

    ball->pixels   = (unsigned*)calloc( diameter2*diameter2, sizeof(unsigned) );
    ball->surface  = sdl_surface_from_argb32( ball->pixels, diameter2, diameter2 );

    /* init axes */
    ball->axes[0][0] = 1.; ball->axes[0][1] = 0.; ball->axes[0][2] = 0.;
    ball->axes[1][0] = 0.; ball->axes[1][1] = 1.; ball->axes[1][2] = 0.;
    ball->axes[2][0] = 0.; ball->axes[2][1] = 0.; ball->axes[2][2] = 1.;

    /* init dots */
    {
        int  side, nn = 0;

        for (side = 0; side < 6; side++) {
            VectorRec  origin, axis1, axis2;
            int        xx, yy;

            switch (side) {
            case 0:
                origin[0] = -1; origin[1] = -1; origin[2] = +1;
                axis1 [0] =  1; axis1 [1] =  0; axis1 [2] =  0;
                axis2 [0] =  0; axis2 [1] =  1; axis2 [2] =  0;
                break;
            case 1:
                origin[0] = -1; origin[1] = -1; origin[2] = -1;
                axis1 [0] =  1; axis1 [1] =  0; axis1 [2] =  0;
                axis2 [0] =  0; axis2 [1] =  1; axis2 [2] =  0;
                break;
            case 2:
                origin[0] = +1; origin[1] = -1; origin[2] = -1;
                axis1 [0] =  0; axis1 [1] =  0; axis1 [2] =  1;
                axis2 [0] =  0; axis2 [1] =  1; axis2 [2] =  0;
                break;
            case 3:
                origin[0] = -1; origin[1] = -1; origin[2] = -1;
                axis1 [0] =  0; axis1 [1] =  0; axis1 [2] =  1;
                axis2 [0] =  0; axis2 [1] =  1; axis2 [2] =  0;
                break;
            case 4:
                origin[0] = -1; origin[1] = -1; origin[2] = -1;
                axis1 [0] =  1; axis1 [1] =  0; axis1 [2] =  0;
                axis2 [0] =  0; axis2 [1] =  0; axis2 [2] =  1;
                break;
            default:
                origin[0] = -1; origin[1] = +1; origin[2] = -1;
                axis1 [0] =  1; axis1 [1] =  0; axis1 [2] =  0;
                axis2 [0] =  0; axis2 [1] =  0; axis2 [2] =  1;
            }

            for (xx = 0; xx < DOT_GRID; xx++) {
                double  tx = xx*(2./DOT_GRID);
                for (yy = 0; yy < DOT_GRID; yy++) {
                    double  ty = yy*(2./DOT_GRID);
                    double  x0  = origin[0] + axis1[0]*tx + axis2[0]*ty;
                    double  y0  = origin[1] + axis1[1]*tx + axis2[1]*ty;
                    double  z0  = origin[2] + axis1[2]*tx + axis2[2]*ty;
                    int     cc;
                    for (cc = 0; cc < DOT_CELLS; cc++) {
                        double  h = (rand() % DOT_RANDOM_X)/((double)DOT_RANDOM_X*DOT_GRID/2);
                        double  v = (rand() % DOT_RANDOM_Y)/((double)DOT_RANDOM_Y*DOT_GRID/2);
                        double  x = x0 + axis1[0]*h + axis2[0]*v;
                        double  y = y0 + axis1[1]*h + axis2[1]*v;
                        double  z = z0 + axis1[2]*h + axis2[2]*v;
                        double  invlen = 1/sqrt( x*x + y*y + z*z );

                        ball->dots[nn][0] = FIX16_FROM_FLOAT(x*invlen);
                        ball->dots[nn][1] = FIX16_FROM_FLOAT(y*invlen);
                        ball->dots[nn][2] = FIX16_FROM_FLOAT(z*invlen);
                        nn++;
                    }
                }
            }
        }
    }

    /* init sphere */
    {
        int     diameter2 = diameter + 2*ring;
        double  radius    = diameter*0.5;
        double  radius2   = diameter2*0.5;
        int     xx, yy;
        int     empty = 0, total = 0;

        ball->sphere_map = calloc( diameter2*diameter2, sizeof(SphereCoordRec) );

        for (yy = 0; yy < diameter2; yy++) {
            for (xx = 0; xx < diameter2; xx++) {
                double       x0    = xx - radius2;
                double       y0    = yy - radius2;
                double       r0    = sqrt( x0*x0 + y0*y0 );
                SphereCoord  coord = &ball->sphere_map[total];

                if (r0 <= radius) {  /* ball pixel */
                    double  rx = x0/radius;
                    double  ry = y0/radius;
                    double  rz = sqrt( 1.0 - rx*rx - ry*ry );

                    coord->x      = xx;
                    coord->y      = yy;
                    coord->offset = xx + yy*diameter2;
                    coord->alpha  = 256;
                    coord->f[0]   = FIX16_FROM_FLOAT(rx);
                    coord->f[1]   = FIX16_FROM_FLOAT(ry);
                    coord->f[2]   = FIX16_FROM_FLOAT(rz);
                    if (r0 >= radius-1.) {
                        coord->alpha = 256*(radius - r0);
                    }
                    /* illumination model */
                    {
#define  LIGHT_X         -2.0
#define  LIGHT_Y         -2.5
#define  LIGHT_Z          5.0

                        double  lx = LIGHT_X - rx;
                        double  ly = LIGHT_Y - ry;
                        double  lz = LIGHT_Z - rz;
                        double  lir = 1/sqrt(lx*lx + ly*ly + lz*lz);
                        double  cosphi = lir*(lx*rx + ly*ry + lz*rz);
                        double  scale  = 1.1*cosphi + 0.3;

                        if (scale < 0)
                            scale = 0;

                        coord->alpha = coord->alpha * scale;
                    }
                    total++;
                } else if (r0 <= radius2) { /* ring pixel */
                    coord->x      = xx;
                    coord->y      = yy;
                    coord->offset = xx + yy*diameter2;
                    coord->alpha  = 0;
                    if (r0 >= radius2-1.) {
                        coord->alpha = -256*(r0 - (radius2-1.));
                    }
                    total++;

                } else   /* outside pixel */
                    empty++;
            }
        }
        ball->sphere_count = total;
    }
}

static int
trackball_contains( TrackBall  ball, int  x, int  y )
{
    return ( (unsigned)(x) < (unsigned)ball->diameter &&
             (unsigned)(y) < (unsigned)ball->diameter );
}

static void
trackball_done( TrackBall  ball )
{
    free( ball->sphere_map );
    ball->sphere_map   = NULL;
    ball->sphere_count = 0;

    if (ball->surface) {
        SDL_FreeSurface( ball->surface );
        ball->surface = NULL;
    }

    if (ball->pixels) {
        free( ball->pixels );
        ball->pixels = NULL;
    }
}

/*** TRACKBALL SPHERE PIXELS
 ***/
static unsigned
color_blend( unsigned  from, unsigned  to,  int  alpha )
{
    unsigned  from_ag    = (from >> 8) & 0x00ff00ff;
    unsigned  to_ag      = (to >> 8) & 0x00ff00ff;
    unsigned  from_rb    = from & 0x00ff00ff;
    unsigned  to_rb      = to & 0x00ff00ff;
    unsigned  result_ag = (from_ag + (alpha*(to_ag - from_ag) >> 8)) & 0xff00ff;
    unsigned  result_rb = (from_rb + (alpha*(to_rb - from_rb) >> 8)) & 0xff00ff;

    return (result_ag << 8) | result_rb;
}

static int
trackball_move( TrackBall  ball,  int  dx, int  dy )
{
    RotatorRec  rot[1];
    Uint32      now = SDL_GetTicks();

    ball->acc_x += dx;
    ball->acc_y += dy;

    if ( norm( ball->acc_x, ball->acc_y ) > ball->acc_threshold )
    {
        int  ddx = ball->acc_x * ball->acc_scale;
        int  ddy = ball->acc_y * ball->acc_scale;
        int  ddt;

        ball->acc_x = 0;
        ball->acc_y = 0;

        switch (ball->rotation) {
        case SKIN_ROTATION_0:
            break;

        case SKIN_ROTATION_90:
            ddt = ddx;
            ddx = ddy;
            ddy = -ddt;
            break;

        case SKIN_ROTATION_180:
            ddx = -ddx;
            ddy = -ddy;
            break;

        case SKIN_ROTATION_270:
            ddt = ddx;
            ddx = -ddy;
            ddy = ddt;
            break;
        }

        user_event_mouse(ddx, ddy, 1, 0);
    }

    rotator_reset( rot, dx, dy );
    rotator_apply( rot, ball->axes[0] );
    rotator_apply( rot, ball->axes[1] );
    rotator_apply( rot, ball->axes[2] );

    if ( ball->ticks_last == 0 )
        ball->ticks_last = now;
    else if ( now > ball->ticks_last + (1000/60) ) {
        ball->ticks_last = now;
        return 1;
    }
    return 0;
}

#define  BACK_COLOR   0x00000000
#define  LIGHT_COLOR  0xffffffff

static void
trackball_refresh( TrackBall  ball )
{
    int             diameter = ball->diameter;
    unsigned*       pixels   = ball->pixels;
    Fix16VectorRec  faxes[3];
    Fix16           dot_threshold = DOT_THRESHOLD * diameter;
    int             nn;

    SDL_LockSurface( ball->surface );

    fixedvector_from_vector( (Fix16Vector)&faxes[0], (Vector)&ball->axes[0] );
    fixedvector_from_vector( (Fix16Vector)&faxes[1], (Vector)&ball->axes[1] );
    fixedvector_from_vector( (Fix16Vector)&faxes[2], (Vector)&ball->axes[2] );

    for (nn = 0; nn < ball->sphere_count; nn++) {
        SphereCoord  coord = &ball->sphere_map[nn];
        unsigned     color = BACK_COLOR;

        if (coord->alpha > 0) {
            /* are we near one of the points ? */
            Fix16  ax = fixedvector_dot( (Fix16Vector)&coord->f, (Fix16Vector)&faxes[0] );
            Fix16  ay = fixedvector_dot( (Fix16Vector)&coord->f, (Fix16Vector)&faxes[1] );
            Fix16  az = fixedvector_dot( (Fix16Vector)&coord->f, (Fix16Vector)&faxes[2] );

            Fix16  best_dist = FIX16_ONE;
            int    pp;

            color = ball->ball_color;

            for (pp = 0; pp < DOT_MAX; pp++) {
                Fix16VectorRec  d;
                Fix16           dist;

                d[0] = ball->dots[pp][0] - ax;
                d[1] = ball->dots[pp][1] - ay;
                d[2] = ball->dots[pp][2] - az;

                if (d[0] > dot_threshold || d[0] < -dot_threshold ||
                    d[1] > dot_threshold || d[1] < -dot_threshold ||
                    d[2] > dot_threshold || d[2] < -dot_threshold )
                    continue;

                dist = fixedvector_len( (Fix16Vector)&d );

                if (dist < best_dist)
                    best_dist = dist;
            }
            if (best_dist < DOT_THRESHOLD) {
                int  a = 256*(DOT_THRESHOLD - best_dist) / DOT_THRESHOLD;
                color = color_blend( color, ball->dot_color, a );
            }

            if (coord->alpha < 256) {
                int  a = coord->alpha;
                color = color_blend( ball->ring_color, color, a );
            }
            else if (coord->alpha > 256) {
                int  a = (coord->alpha - 256);
                color = color_blend( color, LIGHT_COLOR, a );
            }
        }
        else /* coord->alpha <= 0 */
        {
            color = ball->ring_color;

            if (coord->alpha < 0) {
                int  a = -coord->alpha;
                color = color_blend( color, BACK_COLOR, a );
            }
        }

        pixels[coord->x + diameter*coord->y] = color;
    }
    SDL_UnlockSurface( ball->surface );
}

void
trackball_draw( TrackBall  ball, int  x, int  y, SDL_Surface*  dst )
{
    SDL_Rect  d;

    d.x = x;
    d.y = y;
    d.w = ball->diameter;
    d.h = ball->diameter;

    SDL_BlitSurface( ball->surface, NULL, dst, &d );
    SDL_UpdateRects( dst, 1, &d );
}


SkinTrackBall*
skin_trackball_create  ( SkinTrackBallParameters*  params )
{
    TrackBall  ball;

    ANEW0(ball);
    trackball_init( ball,
                    params->diameter,
                    params->ring,
                    params->ball_color,
                    params->dot_color,
                    params->ring_color );
    return  ball;
}

int
skin_trackball_contains( SkinTrackBall*  ball, int  x, int  y )
{
    return  trackball_contains(ball, x, y);
}

int
skin_trackball_move( SkinTrackBall*  ball, int  dx, int  dy )
{
    return  trackball_move(ball, dx, dy);
}

void
skin_trackball_refresh ( SkinTrackBall*  ball )
{
    trackball_refresh(ball);
}

void
skin_trackball_draw( SkinTrackBall*  ball, int  x, int  y, SDL_Surface*  dst )
{
    trackball_draw(ball, x, y, dst);
}

void
skin_trackball_destroy ( SkinTrackBall*  ball )
{
    if (ball) {
        trackball_done(ball);
        AFREE(ball);
    }
}

void
skin_trackball_rect( SkinTrackBall*  ball, SDL_Rect*  rect )
{
    rect->x = 0;
    rect->y = 0;
    rect->w = ball->diameter;
    rect->h = ball->diameter;
}


void
skin_trackball_set_rotation( SkinTrackBall*  ball, SkinRotation  rotation )
{
    ball->rotation = rotation & 3;
}
