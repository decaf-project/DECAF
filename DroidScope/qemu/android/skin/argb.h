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
/* this file contains template code and may be included multiple times */

#ifndef ARGB_T_DEFINED
#define ARGB_T_DEFINED

#if USE_MMX
#include <mmintrin.h>

typedef __m64   mmx_t;
typedef  mmx_t  argb_t;

static inline mmx_t
mmx_load8888( unsigned  value, mmx_t  zero )
{
    return _mm_unpacklo_pi8( _mm_cvtsi32_si64 (value), zero);
}

static inline unsigned
mmx_save8888( mmx_t   argb, mmx_t  zero )
{
    return (unsigned) _mm_cvtsi64_si32( _mm_packs_pu16( argb, zero ) );
}

static inline mmx_t
mmx_expand16( int  value )
{
    mmx_t  t1 = _mm_cvtsi32_si64( value );
    return _mm_packs_pi32( t1, t1 );
}

static inline mmx_t
mmx_mulshift( mmx_t   argb, int  multiplier, int  rshift, mmx_t  zero )
{
    mmx_t   ar   = _mm_unpackhi_pi16(argb, zero );
    mmx_t   gb   = _mm_unpacklo_pi16(argb, zero );
    mmx_t   mult = mmx_expand16(multiplier);

    ar = _mm_srli_pi32( _mm_madd_pi16( ar, mult ), rshift );
    gb = _mm_srli_pi32( _mm_madd_pi16( gb, mult ), rshift );

    return _mm_packs_pi32( gb, ar );
}

static inline mmx_t
mmx_interp255( mmx_t  m1, mmx_t  m2, mmx_t  zero, int  alpha )
{
    mmx_t  mult, mult2, t1, t2, r1, r2;

    // m1 = [ a1 | r1 | g1 | b1 ]
    // m2 = [ a2 | r2 | g2 | b2 ]
    alpha = (alpha << 16) | (alpha ^ 255);
    mult  = _mm_cvtsi32_si64( alpha );                   // mult  = [  0  |  0  |  a  | 1-a ]
    mult2 = _mm_slli_si64( mult, 32 );                   // mult2 = [  a  | 1-a |  0  |  0  ]
    mult  = _mm_or_si64( mult, mult2 );                  // mults = [  a  | 1-a |  a  | 1-a ]

    t1 = _mm_unpackhi_pi16( m1, m2 );    // t1 = [ a2 | a1 | r2 | r1 ]
    r1 = _mm_madd_pi16( t1, mult );      // r1 = [   ra    |    rr   ]

    t2 = _mm_unpacklo_pi16( m1, m2 );    // t1 = [ g2 | g1 | b2 | b1 ]
    r2 = _mm_madd_pi16( t2, mult );      // r2 = [   rg    |    rb   ]

    r1 = _mm_srli_pi32( r1, 8 );
    r2 = _mm_srli_pi32( r2, 8 );

    return  _mm_packs_pi32( r2, r1 );
}

#define   ARGB_DECL_ZERO()      mmx_t    _zero = _mm_setzero_si64()
#define   ARGB_DECL(x)          mmx_t    x
#define   ARGB_DECL2(x1,x2)     mmx_t    x1, x2
#define   ARGB_ZERO(x)          x = _zero
#define   ARGB_UNPACK(x,v)      x =  mmx_load8888((v), _zero)
#define   ARGB_PACK(x)          mmx_save8888(x, _zero)
#define   ARGB_COPY(x,y)        x = y
#define   ARGB_SUM(x1,x2,x3)    x1 = _mm_add_pi32(x2, x3)
#define   ARGB_REDUCE(x,red)   \
    ({ \
        int  _red = (red) >> 8;  \
        if (_red < 256) \
            x = mmx_mulshift( x, _red, 8, _zero ); \
    })

#define  ARGB_INTERP255(x1,x2,x3,alpha)  \
    x1 = mmx_interp255( x2, x3, _zero, (alpha))

#define    ARGB_ADDW_11(x1,x2,x3)  \
    ARGB_SUM(x1,x2,x3)

#define    ARGB_ADDW_31(x1,x2,x3)  \
    ({ \
        mmx_t   _t1 = _mm_add_pi16(x2, x3);  \
        mmx_t   _t2 = _mm_slli_pi16(x2, 1);  \
        x1 = _mm_add_pi16(_t1, _t2);  \
    })

#define    ARGB_ADDW_13(x1,x2,x3)  \
    ({ \
        mmx_t   _t1 = _mm_add_pi16(x2, x3);  \
        mmx_t   _t2 = _mm_slli_pi16(x3, 1);  \
        x1 = _mm_add_pi16(_t1, _t2);  \
    })

#define    ARGB_SHR(x1,x2,s)   \
    x1 = _mm_srli_pi16(x2, s)


#define    ARGB_MULSHIFT(x1,x2,v,s)   \
    x1 = mmx_mulshift(x2, v, s, _zero)

#define   ARGB_BEGIN _mm_empty()
#define   ARGB_DONE  _mm_empty()

#define   ARGB_RESCALE_SHIFT      10
#define   ARGB_DECL_SCALE(s2,s)   int   s2 = (int)((s)*(s)*(1 << ARGB_RESCALE_SHIFT))
#define   ARGB_RESCALE(x,s2)      x = mmx_mulshift( x, s2, ARGB_RESCALE_SHIFT, _zero )

#else /* !USE_MMX */

typedef uint32_t    argb_t;

#define  ARGB_DECL_ZERO()   /* nothing */
#define  ARGB_DECL(x)       argb_t    x##_ag, x##_rb
#define  ARGB_DECL2(x1,x2)  argb_t    x1##_ag, x1##_rb, x2##_ag, x2##_rb
#define  ARGB_ZERO(x)       (x##_ag = x##_rb = 0)
#define  ARGB_COPY(x,y)     (x##_ag = y##_ag, x##_rb = y##_rb)

#define  ARGB_UNPACK(x,v)  \
    ({ \
        argb_t  _v = (argb_t)(v); \
        x##_ag = (_v >> 8) & 0xff00ff; \
        x##_rb = (_v)      & 0xff00ff; \
    })

#define  ARGB_PACK(x)      (uint32_t)(((x##_ag) << 8) | x##_rb)

#define   ARGB_SUM(x1,x2,x3)  \
    ({ \
        x1##_ag = x2##_ag + x3##_ag; \
        x1##_rb = x2##_rb + x3##_rb; \
    })

#define   ARGB_REDUCE(x,red)   \
    ({ \
        int  _red = (red) >> 8; \
        if (_red < 256) { \
            x##_ag = ((x##_ag*_red) >> 8) & 0xff00ff; \
            x##_rb = ((x##_rb*_red) >> 8) & 0xff00ff; \
        } \
    })

#define    ARGB_INTERP255(x1,x2,x3,alpha)  \
    ({ \
        int  _alpha = (alpha); \
        int  _ialpha; \
        _alpha += _alpha >> 8; \
        _ialpha = 256 - _alpha; \
        x1##_ag = ((x2##_ag*_ialpha + x3##_ag*_alpha) >> 8) & 0xff00ff;  \
        x1##_rb = ((x2##_rb*_ialpha + x3##_rb*_alpha) >> 8) & 0xff00ff;  \
    })

#define    ARGB_ADDW_11(x1,x2,x3)  \
    ({ \
        x1##_ag = (x2##_ag + x3##_ag);  \
        x1##_rb = (x2##_rb + x3##_rb);  \
    })

#define    ARGB_ADDW_31(x1,x2,x3)  \
    ({ \
        x1##_ag = (3*x2##_ag + x3##_ag);  \
        x1##_rb = (3*x2##_rb + x3##_rb);  \
    })

#define    ARGB_ADDW_13(x1,x2,x3)  \
    ({ \
        x1##_ag = (x2##_ag + 3*x3##_ag);  \
        x1##_rb = (x2##_rb + 3*x3##_rb);  \
    })

#define    ARGB_MULSHIFT(x1,x2,v,s)   \
    ({ \
        unsigned  _vv = (v);  \
        x1##_ag = ((x2##_ag * _vv) >> (s)) & 0xff00ff;  \
        x1##_rb = ((x2##_rb * _vv) >> (s)) & 0xff00ff;  \
    })

#define   ARGB_SHR(x1,x2,s)  \
    ({  \
        int  _s = (s);  \
        x1##_ag = (x2##_ag >> _s) & 0xff00ff; \
        x1##_rb = (x2##_rb >> _s) & 0xff00ff; \
    })

#define   ARGB_BEGIN ((void)0)
#define   ARGB_DONE  ((void)0)

#define   ARGB_RESCALE_SHIFT      8
#define   ARGB_DECL_SCALE(s2,s)   int   s2 = (int)((s)*(s)*(1 << ARGB_RESCALE_SHIFT))
#define   ARGB_RESCALE(x,scale2)  ARGB_MULSHIFT(x,x,scale2,ARGB_RESCALE_SHIFT)

#endif /* !USE_MMX */

#define   ARGB_ADD(x1,x2)     ARGB_SUM(x1,x1,x2)
#define   ARGB_READ(x,p)      ARGB_UNPACK(x,*(uint32_t*)(p))
#define   ARGB_WRITE(x,p)     *(uint32_t*)(p) = ARGB_PACK(x)

#endif /* !ARGB_T_DEFINED */



#ifdef ARGB_SCALE_GENERIC
static void
ARGB_SCALE_GENERIC( ScaleOp*   op )
{
    int        dst_pitch = op->dst_pitch;
    int        src_pitch = op->src_pitch;
    uint8_t*   dst_line  = op->dst_line;
    uint8_t*   src_line  = op->src_line;
    ARGB_DECL_SCALE(scale2, op->scale);
    int        h;
    int        sx = op->sx;
    int        sy = op->sy;
    int        ix = op->ix;
    int        iy = op->iy;

    ARGB_BEGIN;

    src_line += (sx >> 16)*4 + (sy >> 16)*src_pitch;
    sx       &= 0xffff;
    sy       &= 0xffff;

    for ( h = op->rd.h; h > 0; h-- ) {
        uint8_t*  dst = dst_line;
        uint8_t*  src = src_line;
        uint8_t*  dst_end = dst + 4*op->rd.w;
        int       sx1 = sx;
        int       sy1 = sy;

        for ( ; dst < dst_end; ) {
            int  sx2 = sx1 + ix;
            int  sy2 = sy1 + iy;

            ARGB_DECL_ZERO();
            ARGB_DECL(spix);
            ARGB_DECL(pix);
            ARGB_ZERO(pix);

            /* the current destination pixel maps to the (sx1,sy1)-(sx2,sy2)
            * source square, we're going to compute the sum of its pixels'
            * colors...  simple box filtering
            */
            {
                int  gsy, gsx;
                for ( gsy = 0; gsy < sy2; gsy += 65536 ) {
                    for ( gsx = 0; gsx < sx2; gsx += 65536 ) {
                        uint8_t*  s    = src + (gsx >> 16)*4 + (gsy >> 16)*src_pitch;
                        int       xmin = gsx, xmax = gsx + 65536, ymin = gsy, ymax = gsy + 65536;
                        unsigned  ww, hh;
                        unsigned  red;

                        if (xmin < sx1) xmin = sx1;
                        if (xmax > sx2) xmax = sx2;
                        if (ymin < sy1) ymin = sy1;
                        if (ymax > sy2) ymax = sy2;

                        ww = (unsigned)(xmax-xmin);
                        red = ww;

                        hh = (unsigned)(ymax-ymin);
                        red = (hh < 65536) ? (red*hh >> 16U) : red;

                        ARGB_READ(spix,s);
                        ARGB_REDUCE(spix,red);
                        ARGB_ADD(pix,spix);
                    }
                }
            }

            ARGB_RESCALE(pix,scale2);
            ARGB_WRITE(pix,dst);

            sx1  = sx2;
            src += (sx1 >> 16)*4;
            sx1 &= 0xffff;
            dst += 4;
        }

        sy       += iy;
        src_line += (sy >> 16)*src_pitch;
        sy       &= 0xffff;

        dst_line += dst_pitch;
    }
    ARGB_DONE;
}
#endif
#undef  ARGB_SCALE_GENERIC


#ifdef ARGB_SCALE_05_TO_10
static inline int cross( int  x, int  y ) {
    if (x == 65536 && y == 65536)
        return 65536;

    return (int)((unsigned)x * (unsigned)y >> 16U);
}

static void
scale_05_to_10( ScaleOp*   op )
{
    int        dst_pitch = op->dst_pitch;
    int        src_pitch = op->src_pitch;
    uint8_t*   dst_line  = op->dst_line;
    uint8_t*   src_line  = op->src_line;
    ARGB_DECL_SCALE(scale2, op->scale);
    int        h;
    int        sx = op->sx;
    int        sy = op->sy;
    int        ix = op->ix;
    int        iy = op->iy;

    ARGB_BEGIN;

    src_line += (sx >> 16)*4 + (sy >> 16)*src_pitch;
    sx       &= 0xffff;
    sy       &= 0xffff;

    for ( h = op->rd.h; h > 0; h-- ) {
        uint8_t*  dst = dst_line;
        uint8_t*  src = src_line;
        uint8_t*  dst_end = dst + 4*op->rd.w;
        int       sx1 = sx;
        int       sy1 = sy;

        for ( ; dst < dst_end; ) {
            int  sx2 = sx1 + ix;
            int  sy2 = sy1 + iy;

            ARGB_DECL_ZERO();
            ARGB_DECL2(spix, pix);

            int      off = src_pitch;
            int      fx1 = sx1 & 0xffff;
            int      fx2 = sx2 & 0xffff;
            int      fy1 = sy1 & 0xffff;
            int      fy2 = sy2 & 0xffff;

            int      center_x = ((sx1 >> 16) + 1) < ((sx2-1) >> 16);
            int      center_y = ((sy1 >> 16) + 1) < ((sy2-1) >> 16);

            ARGB_ZERO(pix);

            if (fx2 == 0) {
                fx2  = 65536;
            }
            if (fy2 == 0) {
                fy2  = 65536;
            }
            fx1 = 65536 - fx1;
            fy1 = 65536 - fy1;

            /** TOP BAND
             **/

            /* top-left pixel */
            ARGB_READ(spix,src);
            ARGB_REDUCE(spix,cross(fx1,fy1));
            ARGB_ADD(pix,spix);

            /* top-center pixel, if any */
            ARGB_READ(spix,src + 4);
            if (center_x) {
                ARGB_REDUCE(spix,fy1);
                ARGB_ADD(pix,spix);
                ARGB_READ(spix,src + 8);
            }

            /* top-right pixel */
            ARGB_REDUCE(spix,cross(fx2,fy1));
            ARGB_ADD(pix,spix);

            /** MIDDLE BAND, IF ANY
             **/
            if (center_y) {
                /* left-middle pixel */
                ARGB_READ(spix,src + off);
                ARGB_REDUCE(spix,fx1);
                ARGB_ADD(pix,spix);

                /* center pixel, if any */
                ARGB_READ(spix,src + off + 4);
                if (center_x) {
                    ARGB_ADD(pix,spix);
                    ARGB_READ(spix,src + off + 8);
                }

                /* right-middle pixel */
                ARGB_REDUCE(spix,fx2);
                ARGB_ADD(pix,spix);

                off += src_pitch;
            }

            /** BOTTOM BAND
             **/
            /* left-bottom pixel */
            ARGB_READ(spix,src + off);
            ARGB_REDUCE(spix,cross(fx1,fy2));
            ARGB_ADD(pix,spix);

            /* center-bottom, if any */
            ARGB_READ(spix,src + off + 4);
            if (center_x) {
                ARGB_REDUCE(spix,fy2);
                ARGB_ADD(pix,spix);
                ARGB_READ(spix,src + off + 8);
            }

            /* right-bottom pixel */
            ARGB_REDUCE(spix,cross(fx2,fy2));
            ARGB_ADD(pix,spix);

            /** WRITE IT
             **/
            ARGB_RESCALE(pix,scale2);
            ARGB_WRITE(pix,dst);

            sx1  = sx2;
            src += (sx1 >> 16)*4;
            sx1 &= 0xffff;
            dst += 4;
        }

        sy       += iy;
        src_line += (sy >> 16)*src_pitch;
        sy       &= 0xffff;

        dst_line += dst_pitch;
    }
    ARGB_DONE;
}
#endif
#undef ARGB_SCALE_05_TO_10


#ifdef ARGB_SCALE_UP_BILINEAR
static void
scale_up_bilinear( ScaleOp*  op )
{
    int        dst_pitch = op->dst_pitch;
    int        src_pitch = op->src_pitch;
    uint8_t*   dst_line  = op->dst_line;
    uint8_t*   src_line  = op->src_line;
    int        sx = op->sx;
    int        sy = op->sy;
    int        ix = op->ix;
    int        iy = op->iy;
    int        xlimit, ylimit;
    int        h, sx0;

    ARGB_BEGIN;

    /* the center pixel is at (sx+ix/2, sy+iy/2), we then want to get */
    /* the four nearest source pixels, which are at (0.5,0.5) offsets */

    sx = sx + ix/2 - 32768;
    sy = sy + iy/2 - 32768;

    xlimit = (op->src_w-1);
    ylimit = (op->src_h-1);

    sx0 = sx;

    for ( h = op->rd.h; h > 0; h-- ) {
        uint8_t*  dst = dst_line;
        uint8_t*  dst_end = dst + 4*op->rd.w;

        sx = sx0;
        for ( ; dst < dst_end; ) {
            int        ex1, ex2, ey1, ey2, alpha;
            uint8_t*   s;

            ARGB_DECL_ZERO();
            ARGB_DECL2(spix1,spix2);
            ARGB_DECL2(pix3,pix4);
            ARGB_DECL(pix);

            /* find the four neighbours */
            ex1 = (sx >> 16);
            ey1 = (sy >> 16);
            ex2 = (sx+65535) >> 16;
            ey2 = (sy+65535) >> 16;

            if (ex1 < 0) ex1 = 0; else if (ex1 > xlimit) ex1 = xlimit;
            if (ey1 < 0) ey1 = 0; else if (ey1 > ylimit) ey1 = ylimit;
            if (ex2 < 0) ex2 = 0; else if (ex2 > xlimit) ex2 = xlimit;
            if (ey2 < 0) ey2 = 0; else if (ey2 > ylimit) ey2 = ylimit;

            ex2 = (ex2-ex1)*4;
            ey2 = (ey2-ey1)*src_pitch;

            /* interpolate */
            s   = src_line + ex1*4 + ey1*src_pitch;
            ARGB_READ(spix1, s);
            ARGB_READ(spix2, s+ex2);

            alpha  = (sx >> 8) & 0xff;
            ARGB_INTERP255(pix3,spix1,spix2,alpha);

            s  += ey2;
            ARGB_READ(spix1, s);
            ARGB_READ(spix2, s+ex2);

            ARGB_INTERP255(pix4,spix1,spix2,alpha);

            alpha = (sy >> 8) & 0xff;
            ARGB_INTERP255(pix,pix3,pix4,alpha);

            ARGB_WRITE(pix,dst);

            sx  += ix;
            dst += 4;
        }

        sy       += iy;
        dst_line += dst_pitch;
    }
    ARGB_DONE;
}
#endif
#undef ARGB_SCALE_UP_BILINEAR

#ifdef ARGB_SCALE_UP_QUICK_4x4
static void
ARGB_SCALE_UP_QUICK_4x4( ScaleOp*  op )
{
    int        dst_pitch = op->dst_pitch;
    int        src_pitch = op->src_pitch;
    uint8_t*   dst_line  = op->dst_line;
    uint8_t*   src_line  = op->src_line;
    int        sx = op->sx;
    int        sy = op->sy;
    int        ix = op->ix;
    int        iy = op->iy;
    int        xlimit, ylimit;
    int        h, sx0;

    ARGB_BEGIN;

    /* the center pixel is at (sx+ix/2, sy+iy/2), we then want to get */
    /* the four nearest source pixels, which are at (0.5,0.5) offsets */

    sx = sx + ix/2 - 32768;
    sy = sy + iy/2 - 32768;

    xlimit = (op->src_w-1);
    ylimit = (op->src_h-1);

    sx0 = sx;

    for ( h = op->rd.h; h > 0; h-- ) {
        uint8_t*  dst = dst_line;
        uint8_t*  dst_end = dst + 4*op->rd.w;

        sx = sx0;
        for ( ; dst < dst_end; ) {
            int        ex1, ex2, ey1, ey2;
            uint8_t*   p;
            ARGB_DECL_ZERO();
            ARGB_DECL(pix);
            ARGB_DECL2(spix1, spix2);
            ARGB_DECL2(pix3, pix4);

            /* find the four neighbours */
            ex1 = (sx >> 16);
            ey1 = (sy >> 16);
            ex2 = (sx+65535) >> 16;
            ey2 = (sy+65535) >> 16;

            if (ex1 < 0) ex1 = 0; else if (ex1 > xlimit) ex1 = xlimit;
            if (ey1 < 0) ey1 = 0; else if (ey1 > ylimit) ey1 = ylimit;
            if (ex2 < 0) ex2 = 0; else if (ex2 > xlimit) ex2 = xlimit;
            if (ey2 < 0) ey2 = 0; else if (ey2 > ylimit) ey2 = ylimit;

            /* interpolate */
            p   = (src_line + ex1*4 + ey1*src_pitch);

            ex2 = (ex2-ex1)*4;
            ey2 = (ey2-ey1)*src_pitch;

            switch (((sx >> 14) & 3) | ((sy >> 12) & 12)) {
                case 0:
                    *(uint32_t*)dst = *(uint32_t*)p;
                    break;

                /* top-line is easy */
                case 1:
                    ARGB_READ(spix1, p);
                    ARGB_READ(spix2, p+ex2);
                    ARGB_ADDW_31(pix,spix1,spix2);
                    ARGB_SHR(pix,pix,2);
                    ARGB_WRITE(pix, dst);
                    break;

                case 2:
                    ARGB_READ(spix1, p);
                    ARGB_READ(spix2, p+ex2);
                    ARGB_ADDW_11(pix, spix1, spix2);
                    ARGB_SHR(pix,pix,1);
                    ARGB_WRITE(pix, dst);
                    break;

                case 3:
                    ARGB_READ(spix1, p);
                    ARGB_READ(spix2, p+ex2);
                    ARGB_ADDW_13(pix,spix1,spix2);
                    ARGB_SHR(pix,pix,2);
                    ARGB_WRITE(pix, dst);
                    break;

                /* second line is harder */
                case 4:
                    ARGB_READ(spix1, p);
                    ARGB_READ(spix2, p+ey2);
                    ARGB_ADDW_31(pix,spix1,spix2);
                    ARGB_SHR(pix,pix,2);
                    ARGB_WRITE(pix, dst);
                    break;

                case 5:
                    ARGB_READ(spix1, p);
                    ARGB_READ(spix2, p+ex2);
                    ARGB_ADDW_31(pix3,spix1,spix2);
                    p += ey2;
                    ARGB_READ(spix1, p);
                    ARGB_READ(spix2, p+ex2);
                    ARGB_ADDW_31(pix4,spix1,spix2);

                    ARGB_ADDW_31(pix,pix3,pix4);
                    ARGB_SHR(pix,pix,4);
                    ARGB_WRITE(pix,dst);
                    break;

                case 6:
                    ARGB_READ(spix1, p);
                    ARGB_READ(spix2, p+ex2);
                    ARGB_ADDW_11(pix3,spix1,spix2);
                    p += ey2;
                    ARGB_READ(spix1, p);
                    ARGB_READ(spix2, p+ex2);
                    ARGB_ADDW_11(pix4,spix1,spix2);

                    ARGB_ADDW_31(pix,pix3,pix4);
                    ARGB_SHR(pix,pix,3);
                    ARGB_WRITE(pix,dst);
                    break;

                case 7:
                    ARGB_READ(spix1, p);
                    ARGB_READ(spix2, p+ex2);
                    ARGB_ADDW_13(pix3,spix1,spix2);
                    p += ey2;
                    ARGB_READ(spix1, p);
                    ARGB_READ(spix2, p+ex2);
                    ARGB_ADDW_13(pix4,spix1,spix2);

                    ARGB_ADDW_31(pix,pix3,pix4);
                    ARGB_SHR(pix,pix,4);
                    ARGB_WRITE(pix,dst);
                    break;

                 /* third line */
                case 8:
                    ARGB_READ(spix1, p);
                    ARGB_READ(spix2, p+ey2);
                    ARGB_ADDW_11(pix,spix1,spix2);
                    ARGB_SHR(pix,pix,1);
                    ARGB_WRITE(pix, dst);
                    break;

                case 9:
                    ARGB_READ(spix1, p);
                    ARGB_READ(spix2, p+ex2);
                    ARGB_ADDW_31(pix3,spix1,spix2);
                    p += ey2;
                    ARGB_READ(spix1, p);
                    ARGB_READ(spix2, p+ex2);
                    ARGB_ADDW_31(pix4,spix1,spix2);

                    ARGB_ADDW_11(pix,pix3,pix4);
                    ARGB_SHR(pix,pix,3);
                    ARGB_WRITE(pix,dst);
                    break;

                case 10:
                    ARGB_READ(spix1, p);
                    ARGB_READ(spix2, p+ex2);
                    ARGB_ADDW_11(pix3,spix1,spix2);
                    p += ey2;
                    ARGB_READ(spix1, p);
                    ARGB_READ(spix2, p+ex2);
                    ARGB_ADDW_11(pix4,spix1,spix2);

                    ARGB_ADDW_11(pix,pix3,pix4);
                    ARGB_SHR(pix,pix,2);
                    ARGB_WRITE(pix,dst);
                    break;

                case 11:
                    ARGB_READ(spix1, p);
                    ARGB_READ(spix2, p+ex2);
                    ARGB_ADDW_13(pix3,spix1,spix2);
                    p += ey2;
                    ARGB_READ(spix1, p);
                    ARGB_READ(spix2, p+ex2);
                    ARGB_ADDW_13(pix4,spix1,spix2);

                    ARGB_ADDW_11(pix,pix3,pix4);
                    ARGB_SHR(pix,pix,3);
                    ARGB_WRITE(pix,dst);
                    break;

                 /* last line */
                case 12:
                    ARGB_READ(spix1, p);
                    ARGB_READ(spix2, p+ey2);
                    ARGB_ADDW_13(pix,spix1,spix2);
                    ARGB_SHR(pix,pix,2);
                    ARGB_WRITE(pix, dst);
                    break;

                case 13:
                    ARGB_READ(spix1, p);
                    ARGB_READ(spix2, p+ex2);
                    ARGB_ADDW_31(pix3,spix1,spix2);
                    p += ey2;
                    ARGB_READ(spix1, p);
                    ARGB_READ(spix2, p+ex2);
                    ARGB_ADDW_31(pix4,spix1,spix2);

                    ARGB_ADDW_13(pix,pix3,pix4);
                    ARGB_SHR(pix,pix,4);
                    ARGB_WRITE(pix,dst);
                    break;

                case 14:
                    ARGB_READ(spix1, p);
                    ARGB_READ(spix2, p+ex2);
                    ARGB_ADDW_11(pix3,spix1,spix2);
                    p += ey2;
                    ARGB_READ(spix1, p);
                    ARGB_READ(spix2, p+ex2);
                    ARGB_ADDW_11(pix4,spix1,spix2);

                    ARGB_ADDW_13(pix,pix3,pix4);
                    ARGB_SHR(pix,pix,3);
                    ARGB_WRITE(pix,dst);
                    break;

                default:
                    ARGB_READ(spix1, p);
                    ARGB_READ(spix2, p+ex2);
                    ARGB_ADDW_13(pix3,spix1,spix2);
                    p += ey2;
                    ARGB_READ(spix1, p);
                    ARGB_READ(spix2, p+ex2);
                    ARGB_ADDW_13(pix4,spix1,spix2);

                    ARGB_ADDW_13(pix,pix3,pix4);
                    ARGB_SHR(pix,pix,4);
                    ARGB_WRITE(pix,dst);
            }
            sx  += ix;
            dst += 4;
        }

        sy       += iy;
        dst_line += dst_pitch;
    }
    ARGB_DONE;
}
#endif
#undef  ARGB_SCALE_UP_QUICK_4x4


#ifdef ARGB_SCALE_NEAREST
/* this version scales up with nearest neighbours - looks crap */
static void
ARGB_SCALE_NEAREST( ScaleOp*  op )
{
    int        dst_pitch = op->dst_pitch;
    int        src_pitch = op->src_pitch;
    uint8_t*   dst_line  = op->dst_line;
    uint8_t*   src_line  = op->src_line;
    int        sx = op->sx;
    int        sy = op->sy;
    int        ix = op->ix;
    int        iy = op->iy;
    int        xlimit, ylimit;
    int        h, sx0;

    ARGB_BEGIN;

    /* the center pixel is at (sx+ix/2, sy+iy/2), we then want to get */
    /* the four nearest source pixels, which are at (0.5,0.5) offsets */

    sx = sx + ix/2 - 32768;
    sy = sy + iy/2 - 32768;

    xlimit = (op->src_w-1);
    ylimit = (op->src_h-1);

    sx0 = sx;

    for ( h = op->rd.h; h > 0; h-- ) {
        uint8_t*  dst = dst_line;
        uint8_t*  dst_end = dst + 4*op->rd.w;

        sx = sx0;
        for ( ; dst < dst_end; ) {
            int        ex1, ex2, ey1, ey2;
            unsigned*  p;

            /* find the top-left neighbour */
            ex1 = (sx >> 16);
            ey1 = (sy >> 16);
            ex2 = ex1+1;
            ey2 = ey1+1;

            if (ex1 < 0) ex1 = 0; else if (ex1 > xlimit) ex1 = xlimit;
            if (ey1 < 0) ey1 = 0; else if (ey1 > ylimit) ey1 = ylimit;
            if (ex2 < 0) ex2 = 0; else if (ex2 > xlimit) ex2 = xlimit;
            if (ey2 < 0) ey2 = 0; else if (ey2 > ylimit) ey2 = ylimit;

            p   = (unsigned*)(src_line + ex1*4 + ey1*src_pitch);
            if ((sx & 0xffff) >= 32768)
                p += (ex2-ex1);
            if ((sy & 0xffff) >= 32768)
                p = (unsigned*)((char*)p + (ey2-ey1)*src_pitch);

            *(unsigned*)dst = p[0];

            sx  += ix;
            dst += 4;
        }

        sy       += iy;
        dst_line += dst_pitch;
    }
}
#endif
#undef  ARGB_SCALE_NEAREST
