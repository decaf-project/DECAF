/*
Copyright (C) <2012> <Syracuse System Security (Sycure) Lab>

DECAF is based on QEMU, a whole-system emulator. You can redistribute
and modify it under the terms of the GNU GPL, version 3 or later,
but it is made available WITHOUT ANY WARRANTY. See the top-level
README file for more details.

For more information about DECAF and other softwares, see our
web site at:
http://sycurelab.ecs.syr.edu/

If you have any questions about DECAF,please post it on
http://code.google.com/p/decaf-platform/
*/
// AWH #include "hw/hw.h"
// AWH #include "hw/pc.h"
#include "DECAF_vm_compress.h"

#define DECAF_CBLOCK_MAGIC 0xfabe

int DECAF_compress_open(DECAF_CompressState_t *s, void *f)
{
    int ret;
    memset(s, 0, sizeof(*s));
    s->f = f;
    ret = deflateInit2(&s->zstream, 1,
                       Z_DEFLATED, 15, 
                       9, Z_DEFAULT_STRATEGY);
    if (ret != Z_OK)
        return -1;
    s->zstream.avail_out = IOBUF_SIZE;
    s->zstream.next_out = s->buf;
    return 0;
}

static void DECAF_put_cblock(DECAF_CompressState_t *s, const uint8_t *buf, int len)
{
    qemu_put_be16((QEMUFile *)s->f, DECAF_CBLOCK_MAGIC);
    qemu_put_be16((QEMUFile *)s->f, len);
    qemu_put_buffer((QEMUFile *)s->f, buf, len);
}

int DECAF_compress_buf(DECAF_CompressState_t *s, const uint8_t *buf, int len)
{
    int ret;

    s->zstream.avail_in = len;
    s->zstream.next_in = (uint8_t *)buf;
    while (s->zstream.avail_in > 0) {
        ret = deflate(&s->zstream, Z_NO_FLUSH);
        if (ret != Z_OK)
            return -1;
        if (s->zstream.avail_out == 0) {
            DECAF_put_cblock(s, s->buf, IOBUF_SIZE);
            s->zstream.avail_out = IOBUF_SIZE;
            s->zstream.next_out = s->buf;
        }
    }
    return 0;
}

void DECAF_compress_close(DECAF_CompressState_t *s)
{
    int len, ret;

    /* compress last bytes */
    for(;;) {
        ret = deflate(&s->zstream, Z_FINISH);
        if (ret == Z_OK || ret == Z_STREAM_END) {
            len = IOBUF_SIZE - s->zstream.avail_out;
            if (len > 0) {
                DECAF_put_cblock(s, s->buf, len);
            }
            s->zstream.avail_out = IOBUF_SIZE;
            s->zstream.next_out = s->buf;
            if (ret == Z_STREAM_END)
                break;
        } else {
            goto fail;
        }
    }
fail:
    deflateEnd(&s->zstream);
}

int DECAF_decompress_open(DECAF_CompressState_t *s, void *f)
{
    int ret;
    memset(s, 0, sizeof(*s));
    s->f = f;
    ret = inflateInit(&s->zstream);
    if (ret != Z_OK)
        return -1;
    return 0;
}

int DECAF_decompress_buf(DECAF_CompressState_t *s, uint8_t *buf, int len)
{
    int ret, clen;

    s->zstream.avail_out = len;
    s->zstream.next_out = buf;
    while (s->zstream.avail_out > 0) {
        if (s->zstream.avail_in == 0) {
            if (qemu_get_be16((QEMUFile *)s->f) != DECAF_CBLOCK_MAGIC)
                return -1;
            clen = qemu_get_be16((QEMUFile *)s->f);
            if (clen > IOBUF_SIZE)
                return -1;
            qemu_get_buffer((QEMUFile *)s->f, s->buf, clen);
            s->zstream.avail_in = clen;
            s->zstream.next_in = s->buf;
        }
        ret = inflate(&s->zstream, Z_PARTIAL_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            return -1;
        }
    }
    return 0;
}

void DECAF_decompress_close(DECAF_CompressState_t *s)
{
    inflateEnd(&s->zstream);
}

void DECAF_vm_compress_init(void)
{
}
