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
#include "qemu_file.h"
#include "android/android.h"
#include "android/utils/debug.h"
#include "android/utils/duff.h"
#include "goldfish_device.h"
#include "console.h"

/* These values *must* match the platform definitions found under
 * hardware/libhardware/include/hardware/hardware.h
 */
enum {
    HAL_PIXEL_FORMAT_RGBA_8888          = 1,
    HAL_PIXEL_FORMAT_RGBX_8888          = 2,
    HAL_PIXEL_FORMAT_RGB_888            = 3,
    HAL_PIXEL_FORMAT_RGB_565            = 4,
    HAL_PIXEL_FORMAT_BGRA_8888          = 5,
    HAL_PIXEL_FORMAT_RGBA_5551          = 6,
    HAL_PIXEL_FORMAT_RGBA_4444          = 7,
};

enum {
    FB_GET_WIDTH        = 0x00,
    FB_GET_HEIGHT       = 0x04,
    FB_INT_STATUS       = 0x08,
    FB_INT_ENABLE       = 0x0c,
    FB_SET_BASE         = 0x10,
    FB_SET_ROTATION     = 0x14,
    FB_SET_BLANK        = 0x18,
    FB_GET_PHYS_WIDTH   = 0x1c,
    FB_GET_PHYS_HEIGHT  = 0x20,
    FB_GET_FORMAT       = 0x24,

    FB_INT_VSYNC             = 1U << 0,
    FB_INT_BASE_UPDATE_DONE  = 1U << 1
};

struct goldfish_fb_state {
    struct goldfish_device dev;
    DisplayState*  ds;
    int      pixel_format;
    int      bytes_per_pixel;
    uint32_t fb_base;
    uint32_t base_valid : 1;
    uint32_t need_update : 1;
    uint32_t need_int : 1;
    uint32_t set_rotation : 2;
    uint32_t blank : 1;
    uint32_t int_status;
    uint32_t int_enable;
    int      rotation;   /* 0, 1, 2 or 3 */
    int      dpi;
};

#define  GOLDFISH_FB_SAVE_VERSION  2

static void goldfish_fb_save(QEMUFile*  f, void*  opaque)
{
    struct goldfish_fb_state*  s = opaque;

    DisplayState*  ds = s->ds;

    qemu_put_be32(f, ds->surface->width);
    qemu_put_be32(f, ds->surface->height);
    qemu_put_be32(f, ds->surface->linesize);
    qemu_put_byte(f, 0);

    qemu_put_be32(f, s->fb_base);
    qemu_put_byte(f, s->base_valid);
    qemu_put_byte(f, s->need_update);
    qemu_put_byte(f, s->need_int);
    qemu_put_byte(f, s->set_rotation);
    qemu_put_byte(f, s->blank);
    qemu_put_be32(f, s->int_status);
    qemu_put_be32(f, s->int_enable);
    qemu_put_be32(f, s->rotation);
    qemu_put_be32(f, s->dpi);
}

static int  goldfish_fb_load(QEMUFile*  f, void*  opaque, int  version_id)
{
    struct goldfish_fb_state*  s   = opaque;
    int                        ret = -1;
    int                        ds_w, ds_h, ds_pitch, ds_rot;

    if (version_id != GOLDFISH_FB_SAVE_VERSION)
        goto Exit;

    ds_w     = qemu_get_be32(f);
    ds_h     = qemu_get_be32(f);
    ds_pitch = qemu_get_be32(f);
    ds_rot   = qemu_get_byte(f);

    DisplayState*  ds = s->ds;

    if (ds->surface->width != ds_w ||
        ds->surface->height != ds_h ||
        ds->surface->linesize != ds_pitch ||
        ds_rot != 0)
    {
        /* XXX: We should be able to force a resize/rotation from here ? */
        fprintf(stderr, "%s: framebuffer dimensions mismatch\n", __FUNCTION__);
        goto Exit;
    }

    s->fb_base      = qemu_get_be32(f);
    s->base_valid   = qemu_get_byte(f);
    s->need_update  = qemu_get_byte(f);
    s->need_int     = qemu_get_byte(f);
    s->set_rotation = qemu_get_byte(f);
    s->blank        = qemu_get_byte(f);
    s->int_status   = qemu_get_be32(f);
    s->int_enable   = qemu_get_be32(f);
    s->rotation     = qemu_get_be32(f);
    s->dpi          = qemu_get_be32(f);

    /* force a refresh */
    s->need_update = 1;

    ret = 0;
Exit:
    return ret;
}

/* Type used to record a mapping from display surface pixel format to
 * HAL pixel format */
typedef struct {
    int    pixel_format; /* HAL pixel format */
    uint8_t bits;
    uint8_t bytes;
    uint32_t rmask, gmask, bmask, amask;
} FbConfig;


/* Return the pixel format of the current framebuffer, based on
 * the current display surface's pixel format.
 *
 * Note that you should not call this function from the device initialization
 * function, because the display surface will change format before the kernel
 * start.
 */
static int goldfish_fb_get_pixel_format(struct goldfish_fb_state *s)
{
    if (s->pixel_format >= 0) {
        return s->pixel_format;
    }
    static const FbConfig fb_configs[] = {
        { HAL_PIXEL_FORMAT_RGB_565, 16, 2, 0xf800, 0x7e0, 0x1f, 0x0 },
        { HAL_PIXEL_FORMAT_RGBX_8888, 32, 4, 0xff0000, 0xff00, 0xff, 0x0 },
        { HAL_PIXEL_FORMAT_RGBA_8888, 32, 4, 0xff0000, 0xff00, 0xff, 0xff000000 },
        { -1, }
    };

    /* Determine HAL pixel format value based on s->ds */
    struct PixelFormat* pf = &s->ds->surface->pf;
    if (VERBOSE_CHECK(init)) {
        printf("%s:%d: display surface,pixel format:\n", __FUNCTION__, __LINE__);
        printf("  bits/pixel:  %d\n", pf->bits_per_pixel);
        printf("  bytes/pixel: %d\n", pf->bytes_per_pixel);
        printf("  depth:       %d\n", pf->depth);
        printf("  red:         bits=%d mask=0x%x shift=%d max=0x%x\n",
            pf->rbits, pf->rmask, pf->rshift, pf->rmax);
        printf("  green:       bits=%d mask=0x%x shift=%d max=0x%x\n",
            pf->gbits, pf->gmask, pf->gshift, pf->gmax);
        printf("  blue:        bits=%d mask=0x%x shift=%d max=0x%x\n",
            pf->bbits, pf->bmask, pf->bshift, pf->bmax);
        printf("  alpha:       bits=%d mask=0x%x shift=%d max=0x%x\n",
            pf->abits, pf->amask, pf->ashift, pf->amax);
    }

    s->bytes_per_pixel = pf->bytes_per_pixel;
    int nn;
    for (nn = 0; fb_configs[nn].pixel_format >= 0; nn++) {
        const FbConfig* fbc = &fb_configs[nn];
        if (pf->bits_per_pixel == fbc->bits &&
            pf->bytes_per_pixel == fbc->bytes &&
            pf->rmask == fbc->rmask &&
            pf->gmask == fbc->gmask &&
            pf->bmask == fbc->bmask &&
            pf->amask == fbc->amask) {
            /* We found it */
            s->pixel_format = fbc->pixel_format;
            return s->pixel_format;
        }
    }
    fprintf(stderr, "%s:%d: Unsupported display pixel format (depth=%d, bytespp=%d, bitspp=%d)\n",
                __FUNCTION__, __LINE__,
                pf->depth,
                pf->bytes_per_pixel,
                pf->bits_per_pixel);
    exit(1);
    return -1;
}

static int goldfish_fb_get_bytes_per_pixel(struct goldfish_fb_state *s)
{
    if (s->pixel_format < 0) {
        (void) goldfish_fb_get_pixel_format(s);
    }
    return s->bytes_per_pixel;
}

static int
pixels_to_mm(int  pixels, int dpi)
{
    /* dpi = dots / inch
    ** inch = dots / dpi
    ** mm / 25.4 = dots / dpi
    ** mm = (dots * 25.4)/dpi
    */
    return (int)(0.5 + 25.4 * pixels  / dpi);
}


#define  STATS  0

#if STATS
static int   stats_counter;
static long  stats_total;
static int   stats_full_updates;
static long  stats_total_full_updates;
#endif

/* This structure is used to hold the inputs for
 * compute_fb_update_rect_linear below.
 * This corresponds to the source framebuffer and destination
 * surface pixel buffers.
 */
typedef struct {
    int            width;
    int            height;
    int            bytes_per_pixel;
    const uint8_t* src_pixels;
    int            src_pitch;
    uint8_t*       dst_pixels;
    int            dst_pitch;
} FbUpdateState;

/* This structure is used to hold the outputs for
 * compute_fb_update_rect_linear below.
 * This corresponds to the smalled bounding rectangle of the
 * latest framebuffer update.
 */
typedef struct {
    int xmin, ymin, xmax, ymax;
} FbUpdateRect;

/* Determine the smallest bounding rectangle of pixels which changed
 * between the source (framebuffer) and destination (surface) pixel
 * buffers.
 *
 * Return 0 if there was no change, otherwise, populate '*rect'
 * and return 1.
 *
 * If 'dirty_base' is not 0, it is a physical address that will be
 * used to speed-up the check using the VGA dirty bits. In practice
 * this is only used if your kernel driver does not implement.
 *
 * This function assumes that the framebuffers are in linear memory.
 * This may change later when we want to support larger framebuffers
 * that exceed the max DMA aperture size though.
 */
static int
compute_fb_update_rect_linear(FbUpdateState*  fbs,
                              uint32_t        dirty_base,
                              FbUpdateRect*   rect)
{
    int  yy;
    int  width = fbs->width;
    const uint8_t* src_line = fbs->src_pixels;
    uint8_t*       dst_line = fbs->dst_pixels;
    uint32_t       dirty_addr = dirty_base;
    rect->xmin = rect->ymin = INT_MAX;
    rect->xmax = rect->ymax = INT_MIN;
    for (yy = 0; yy < fbs->height; yy++) {
        int xx1, xx2;
        /* If dirty_addr is != 0, then use it as a physical address to
         * use the VGA dirty bits table to speed up the detection of
         * changed pixels.
         */
        if (dirty_addr != 0) {
            int  dirty = 0;
            int  len   = fbs->src_pitch;

            while (len > 0) {
                int  len2 = TARGET_PAGE_SIZE - (dirty_addr & (TARGET_PAGE_SIZE-1));

                if (len2 > len)
                    len2 = len;

                dirty |= cpu_physical_memory_get_dirty(dirty_addr, VGA_DIRTY_FLAG);
                dirty_addr  += len2;
                len         -= len2;
            }

            if (!dirty) { /* this line was not modified, skip to next one */
                goto NEXT_LINE;
            }
        }

        /* Then compute actual bounds of the changed pixels, while
         * copying them from 'src' to 'dst'. This depends on the pixel depth.
         */
        switch (fbs->bytes_per_pixel) {
        case 2:
        {
            const uint16_t* src = (const uint16_t*) src_line;
            uint16_t*       dst = (uint16_t*) dst_line;

            xx1 = 0;
            DUFF4(width, {
                uint16_t spix = src[xx1];
#if defined(HOST_WORDS_BIGENDIAN) != defined(TARGET_WORDS_BIGENDIAN)
                spix = (uint16_t)((spix << 8) | (spix >> 8));
#endif
                if (spix != dst[xx1])
                    break;
                xx1++;
            });
            if (xx1 == width) {
                break;
            }
            xx2 = width-1;
            DUFF4(xx2-xx1, {
                if (src[xx2] != dst[xx2])
                    break;
                xx2--;
            });
#if defined(HOST_WORDS_BIGENDIAN) != defined(TARGET_WORDS_BIGENDIAN)
            /* Convert the guest pixels into host ones */
            int xx = xx1;
            DUFF4(xx2-xx1+1,{
                unsigned   spix = src[xx];
                dst[xx] = (uint16_t)((spix << 8) | (spix >> 8));
                xx++;
            });
#else
            memcpy( dst+xx1, src+xx1, (xx2-xx1+1)*2 );
#endif
            break;
        }

        case 3:
        {
            xx1 = 0;
            DUFF4(width, {
                int xx = xx1*3;
                if (src_line[xx+0] != dst_line[xx+0] ||
                    src_line[xx+1] != dst_line[xx+1] ||
                    src_line[xx+2] != dst_line[xx+2]) {
                    break;
                }
                xx1 ++;
            });
            if (xx1 == width) {
                break;
            }
            xx2 = width-1;
            DUFF4(xx2-xx1,{
                int xx = xx2*3;
                if (src_line[xx+0] != dst_line[xx+0] ||
                    src_line[xx+1] != dst_line[xx+1] ||
                    src_line[xx+2] != dst_line[xx+2]) {
                    break;
                }
                xx2--;
            });
            memcpy( dst_line+xx1*3, src_line+xx1*3, (xx2-xx1+1)*3 );
            break;
        }

        case 4:
        {
            const uint32_t* src = (const uint32_t*) src_line;
            uint32_t*       dst = (uint32_t*) dst_line;

            xx1 = 0;
            DUFF4(width, {
                uint32_t spix = src[xx1];
#if defined(HOST_WORDS_BIGENDIAN) != defined(TARGET_WORDS_BIGENDIAN)
                spix = (spix << 16) | (spix >> 16);
                spix = ((spix << 8) & 0xff00ff00) | ((spix >> 8) & 0x00ff00ff);
#endif
                if (spix != dst[xx1]) {
                    break;
                }
                xx1++;
            });
            if (xx1 == width) {
                break;
            }
            xx2 = width-1;
            DUFF4(xx2-xx1,{
                if (src[xx2] != dst[xx2]) {
                    break;
                }
                xx2--;
            });
#if defined(HOST_WORDS_BIGENDIAN) != defined(TARGET_WORDS_BIGENDIAN)
            /* Convert the guest pixels into host ones */
            int xx = xx1;
            DUFF4(xx2-xx1+1,{
                uint32_t   spix = src[xx];
                spix = (spix << 16) | (spix >> 16);
                spix = ((spix << 8) & 0xff00ff00) | ((spix >> 8) & 0x00ff00ff);
                dst[xx] = spix;
                xx++;
            })
#else
            memcpy( dst+xx1, src+xx1, (xx2-xx1+1)*4 );
#endif
            break;
        }
        default:
            return 0;
        }
        /* Update bounds if pixels on this line were modified */
        if (xx1 < width) {
            if (xx1 < rect->xmin) rect->xmin = xx1;
            if (xx2 > rect->xmax) rect->xmax = xx2;
            if (yy < rect->ymin) rect->ymin = yy;
            if (yy > rect->ymax) rect->ymax = yy;
        }
    NEXT_LINE:
        src_line += fbs->src_pitch;
        dst_line += fbs->dst_pitch;
    }

    if (rect->ymin > rect->ymax) { /* nothing changed */
        return 0;
    }

    /* Always clear the dirty VGA bits */
    cpu_physical_memory_reset_dirty(dirty_base + rect->ymin * fbs->src_pitch,
                                    dirty_base + (rect->ymax+1)* fbs->src_pitch,
                                    VGA_DIRTY_FLAG);
    return 1;
}


static void goldfish_fb_update_display(void *opaque)
{
    struct goldfish_fb_state *s = (struct goldfish_fb_state *)opaque;
    uint32_t base;
    uint8_t*  dst_line;
    uint8_t*  src_line;
    int full_update = 0;
    int  width, height, pitch;

    base = s->fb_base;
    if(base == 0)
        return;

    if((s->int_enable & FB_INT_VSYNC) && !(s->int_status & FB_INT_VSYNC)) {
        s->int_status |= FB_INT_VSYNC;
        goldfish_device_set_irq(&s->dev, 0, 1);
    }

    if(s->need_update) {
        full_update = 1;
        if(s->need_int) {
            s->int_status |= FB_INT_BASE_UPDATE_DONE;
            if(s->int_enable & FB_INT_BASE_UPDATE_DONE)
                goldfish_device_set_irq(&s->dev, 0, 1);
        }
        s->need_int = 0;
        s->need_update = 0;
    }

    src_line  = qemu_get_ram_ptr( base );

    dst_line  = s->ds->surface->data;
    pitch     = s->ds->surface->linesize;
    width     = s->ds->surface->width;
    height    = s->ds->surface->height;

    FbUpdateState  fbs;
    FbUpdateRect   rect;

    fbs.width      = width;
    fbs.height     = height;
    fbs.dst_pixels = dst_line;
    fbs.dst_pitch  = pitch;
    fbs.bytes_per_pixel = goldfish_fb_get_bytes_per_pixel(s);

    fbs.src_pixels = src_line;
    fbs.src_pitch  = width*s->ds->surface->pf.bytes_per_pixel;


#if STATS
    if (full_update)
        stats_full_updates += 1;
    if (++stats_counter == 120) {
        stats_total               += stats_counter;
        stats_total_full_updates  += stats_full_updates;

        printf( "full update stats:  peak %.2f %%  total %.2f %%\n",
                stats_full_updates*100.0/stats_counter,
                stats_total_full_updates*100.0/stats_total );

        stats_counter      = 0;
        stats_full_updates = 0;
    }
#endif /* STATS */

    if (s->blank)
    {
        memset( dst_line, 0, height*pitch );
        rect.xmin = 0;
        rect.ymin = 0;
        rect.xmax = width-1;
        rect.ymax = height-1;
    }
    else
    {
        if (full_update) { /* don't use dirty-bits optimization */
            base = 0;
        }
        if (compute_fb_update_rect_linear(&fbs, base, &rect) == 0) {
            return;
        }
    }

    rect.xmax += 1;
    rect.ymax += 1;
#if 0
    printf("goldfish_fb_update_display (y:%d,h:%d,x=%d,w=%d)\n",
           rect.ymin, rect.ymax-rect.ymin, rect.xmin, rect.xmax-rect.xmin);
#endif

    dpy_update(s->ds, rect.xmin, rect.ymin, rect.xmax-rect.xmin, rect.ymax-rect.ymin);
}

static void goldfish_fb_invalidate_display(void * opaque)
{
    // is this called?
    struct goldfish_fb_state *s = (struct goldfish_fb_state *)opaque;
    s->need_update = 1;
}

static uint32_t goldfish_fb_read(void *opaque, target_phys_addr_t offset)
{
    uint32_t ret;
    struct goldfish_fb_state *s = opaque;

    switch(offset) {
        case FB_GET_WIDTH:
            ret = ds_get_width(s->ds);
            //printf("FB_GET_WIDTH => %d\n", ret);
            return ret;

        case FB_GET_HEIGHT:
            ret = ds_get_height(s->ds);
            //printf( "FB_GET_HEIGHT = %d\n", ret);
            return ret;

        case FB_INT_STATUS:
            ret = s->int_status & s->int_enable;
            if(ret) {
                s->int_status &= ~ret;
                goldfish_device_set_irq(&s->dev, 0, 0);
            }
            return ret;

        case FB_GET_PHYS_WIDTH:
            ret = pixels_to_mm( ds_get_width(s->ds), s->dpi );
            //printf( "FB_GET_PHYS_WIDTH => %d\n", ret );
            return ret;

        case FB_GET_PHYS_HEIGHT:
            ret = pixels_to_mm( ds_get_height(s->ds), s->dpi );
            //printf( "FB_GET_PHYS_HEIGHT => %d\n", ret );
            return ret;

        case FB_GET_FORMAT:
            return goldfish_fb_get_pixel_format(s);

        default:
            cpu_abort (cpu_single_env, "goldfish_fb_read: Bad offset %x\n", offset);
            return 0;
    }
}

static void goldfish_fb_write(void *opaque, target_phys_addr_t offset,
                        uint32_t val)
{
    struct goldfish_fb_state *s = opaque;

    switch(offset) {
        case FB_INT_ENABLE:
            s->int_enable = val;
            goldfish_device_set_irq(&s->dev, 0, (s->int_status & s->int_enable));
            break;
        case FB_SET_BASE: {
            int need_resize = !s->base_valid;
            s->fb_base = val;
            s->int_status &= ~FB_INT_BASE_UPDATE_DONE;
            s->need_update = 1;
            s->need_int = 1;
            s->base_valid = 1;
            if(s->set_rotation != s->rotation) {
                //printf("FB_SET_BASE: rotation : %d => %d\n", s->rotation, s->set_rotation);
                s->rotation = s->set_rotation;
                need_resize = 1;
            }
            goldfish_device_set_irq(&s->dev, 0, (s->int_status & s->int_enable));
            if (need_resize) {
                //printf("FB_SET_BASE: need resize (rotation=%d)\n", s->rotation );
                dpy_resize(s->ds);
            }
            } break;
        case FB_SET_ROTATION:
            //printf( "FB_SET_ROTATION %d\n", val);
            s->set_rotation = val;
            break;
        case FB_SET_BLANK:
            s->blank = val;
            s->need_update = 1;
            break;
        default:
            cpu_abort (cpu_single_env, "goldfish_fb_write: Bad offset %x\n", offset);
    }
}

static CPUReadMemoryFunc *goldfish_fb_readfn[] = {
   goldfish_fb_read,
   goldfish_fb_read,
   goldfish_fb_read
};

static CPUWriteMemoryFunc *goldfish_fb_writefn[] = {
   goldfish_fb_write,
   goldfish_fb_write,
   goldfish_fb_write
};

void goldfish_fb_init(int id)
{
    struct goldfish_fb_state *s;

    s = (struct goldfish_fb_state *)qemu_mallocz(sizeof(*s));
    s->dev.name = "goldfish_fb";
    s->dev.id = id;
    s->dev.size = 0x1000;
    s->dev.irq_count = 1;

    s->ds = graphic_console_init(goldfish_fb_update_display,
                                 goldfish_fb_invalidate_display,
                                 NULL,
                                 NULL,
                                 s);

    s->dpi = 165;  /* XXX: Find better way to get actual value ! */

    /* IMPORTANT: DO NOT COMPUTE s->pixel_format and s->bytes_per_pixel
     * here because the display surface is going to change later.
     */
    s->bytes_per_pixel = 0;
    s->pixel_format    = -1;

    goldfish_device_add(&s->dev, goldfish_fb_readfn, goldfish_fb_writefn, s);

    register_savevm( "goldfish_fb", 0, GOLDFISH_FB_SAVE_VERSION,
                     goldfish_fb_save, goldfish_fb_load, s);
}
