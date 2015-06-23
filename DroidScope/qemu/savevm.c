/*
 * QEMU System Emulator
 *
 * Copyright (c) 2003-2008 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>
#include <zlib.h>

/* Needed early for CONFIG_BSD etc. */
#include "config-host.h"

#ifndef _WIN32
#include <sys/times.h>
#include <sys/wait.h>
#include <termios.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <netdb.h>
#include <sys/select.h>
#ifdef CONFIG_BSD
#include <sys/stat.h>
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__DragonFly__)
#include <libutil.h>
#else
#include <util.h>
#endif
#ifdef __linux__
#include <pty.h>
#include <malloc.h>
#include <linux/rtc.h>
#endif
#endif
#endif

#ifdef _WIN32
#include <windows.h>
#include <malloc.h>
#include <sys/timeb.h>
#include <mmsystem.h>
#define getopt_long_only getopt_long
#define memalign(align, size) malloc(size)
#endif

#include "qemu-common.h"
#include "hw/hw.h"
#include "net.h"
#include "monitor.h"
#include "sysemu.h"
#include "qemu-timer.h"
#include "qemu-char.h"
#include "blockdev.h"
#include "block.h"
#include "audio/audio.h"
#include "migration.h"
#include "qemu_socket.h"
#include "qemu-queue.h"
#include "qemu_file.h"
#include "android/snapshot.h"


#define SELF_ANNOUNCE_ROUNDS 5

#ifndef ETH_P_RARP
#define ETH_P_RARP 0x8035
#endif
#define ARP_HTYPE_ETH 0x0001
#define ARP_PTYPE_IP 0x0800
#define ARP_OP_REQUEST_REV 0x3

static int announce_self_create(uint8_t *buf,
				uint8_t *mac_addr)
{
    /* Ethernet header. */
    memset(buf, 0xff, 6);         /* destination MAC addr */
    memcpy(buf + 6, mac_addr, 6); /* source MAC addr */
    *(uint16_t *)(buf + 12) = htons(ETH_P_RARP); /* ethertype */

    /* RARP header. */
    *(uint16_t *)(buf + 14) = htons(ARP_HTYPE_ETH); /* hardware addr space */
    *(uint16_t *)(buf + 16) = htons(ARP_PTYPE_IP); /* protocol addr space */
    *(buf + 18) = 6; /* hardware addr length (ethernet) */
    *(buf + 19) = 4; /* protocol addr length (IPv4) */
    *(uint16_t *)(buf + 20) = htons(ARP_OP_REQUEST_REV); /* opcode */
    memcpy(buf + 22, mac_addr, 6); /* source hw addr */
    memset(buf + 28, 0x00, 4);     /* source protocol addr */
    memcpy(buf + 32, mac_addr, 6); /* target hw addr */
    memset(buf + 38, 0x00, 4);     /* target protocol addr */

    /* Padding to get up to 60 bytes (ethernet min packet size, minus FCS). */
    memset(buf + 42, 0x00, 18);

    return 60; /* len (FCS will be added by hardware) */
}

static void qemu_announce_self_once(void *opaque)
{
    int i, len;
    VLANState *vlan;
    VLANClientState *vc;
    uint8_t buf[256];
    static int count = SELF_ANNOUNCE_ROUNDS;
    QEMUTimer *timer = *(QEMUTimer **)opaque;

    for (i = 0; i < MAX_NICS; i++) {
        if (!nd_table[i].used)
            continue;
        len = announce_self_create(buf, nd_table[i].macaddr);
        vlan = nd_table[i].vlan;
	for(vc = vlan->first_client; vc != NULL; vc = vc->next) {
            vc->receive(vc, buf, len);
        }
    }
    if (--count) {
        /* delay 50ms, 150ms, 250ms, ... */
        qemu_mod_timer(timer, qemu_get_clock_ms(rt_clock) +
                       50 + (SELF_ANNOUNCE_ROUNDS - count - 1) * 100);
    } else {
	    qemu_del_timer(timer);
	    qemu_free_timer(timer);
    }
}

void qemu_announce_self(void)
{
	static QEMUTimer *timer;
	timer = qemu_new_timer_ms(rt_clock, qemu_announce_self_once, &timer);
	qemu_announce_self_once(&timer);
}

/***********************************************************/
/* savevm/loadvm support */

#define IO_BUF_SIZE 32768

struct QEMUFile {
    QEMUFilePutBufferFunc *put_buffer;
    QEMUFileGetBufferFunc *get_buffer;
    QEMUFileCloseFunc *close;
    QEMUFileRateLimit *rate_limit;
    QEMUFileSetRateLimit *set_rate_limit;
    QEMUFileGetRateLimit *get_rate_limit;
    void *opaque;
    int is_write;

    int64_t buf_offset; /* start of buffer when writing, end of buffer
                           when reading */
    int buf_index;
    int buf_size; /* 0 when writing */
    uint8_t buf[IO_BUF_SIZE];

    int has_error;
};

typedef struct QEMUFileStdio
{
    FILE *stdio_file;
    QEMUFile *file;
} QEMUFileStdio;

typedef struct QEMUFileSocket
{
    int fd;
    QEMUFile *file;
} QEMUFileSocket;

static int socket_get_buffer(void *opaque, uint8_t *buf, int64_t pos, int size)
{
    QEMUFileSocket *s = opaque;
    ssize_t len;

    do {
        len = recv(s->fd, (void *)buf, size, 0);
    } while (len == -1 && socket_error() == EINTR);

    if (len == -1)
        len = -socket_error();

    return len;
}

static int file_socket_close(void *opaque)
{
    QEMUFileSocket *s = opaque;
    qemu_free(s);
    return 0;
}

static int stdio_put_buffer(void *opaque, const uint8_t *buf, int64_t pos, int size)
{
    QEMUFileStdio *s = opaque;
    return fwrite(buf, 1, size, s->stdio_file);
}

static int stdio_get_buffer(void *opaque, uint8_t *buf, int64_t pos, int size)
{
    QEMUFileStdio *s = opaque;
    FILE *fp = s->stdio_file;
    int bytes;

    do {
        clearerr(fp);
        bytes = fread(buf, 1, size, fp);
    } while ((bytes == 0) && ferror(fp) && (errno == EINTR));
    return bytes;
}

static int stdio_pclose(void *opaque)
{
    QEMUFileStdio *s = opaque;
    int ret;
    ret = pclose(s->stdio_file);
    qemu_free(s);
    return ret;
}

static int stdio_fclose(void *opaque)
{
    QEMUFileStdio *s = opaque;
    fclose(s->stdio_file);
    qemu_free(s);
    return 0;
}

QEMUFile *qemu_popen(FILE *stdio_file, const char *mode)
{
    QEMUFileStdio *s;

    if (stdio_file == NULL || mode == NULL || (mode[0] != 'r' && mode[0] != 'w') || mode[1] != 0) {
        fprintf(stderr, "qemu_popen: Argument validity check failed\n");
        return NULL;
    }

    s = qemu_mallocz(sizeof(QEMUFileStdio));

    s->stdio_file = stdio_file;

    if(mode[0] == 'r') {
        s->file = qemu_fopen_ops(s, NULL, stdio_get_buffer, stdio_pclose,
				 NULL, NULL, NULL);
    } else {
        s->file = qemu_fopen_ops(s, stdio_put_buffer, NULL, stdio_pclose,
				 NULL, NULL, NULL);
    }
    return s->file;
}

QEMUFile *qemu_popen_cmd(const char *command, const char *mode)
{
    FILE *popen_file;

    popen_file = popen(command, mode);
    if(popen_file == NULL) {
        return NULL;
    }

    return qemu_popen(popen_file, mode);
}

int qemu_stdio_fd(QEMUFile *f)
{
    QEMUFileStdio *p;
    int fd;

    p = (QEMUFileStdio *)f->opaque;
    fd = fileno(p->stdio_file);

    return fd;
}

QEMUFile *qemu_fdopen(int fd, const char *mode)
{
    QEMUFileStdio *s;

    if (mode == NULL ||
	(mode[0] != 'r' && mode[0] != 'w') ||
	mode[1] != 'b' || mode[2] != 0) {
        fprintf(stderr, "qemu_fdopen: Argument validity check failed\n");
        return NULL;
    }

    s = qemu_mallocz(sizeof(QEMUFileStdio));
    s->stdio_file = fdopen(fd, mode);
    if (!s->stdio_file)
        goto fail;

    if(mode[0] == 'r') {
        s->file = qemu_fopen_ops(s, NULL, stdio_get_buffer, stdio_fclose,
				 NULL, NULL, NULL);
    } else {
        s->file = qemu_fopen_ops(s, stdio_put_buffer, NULL, stdio_fclose,
				 NULL, NULL, NULL);
    }
    return s->file;

fail:
    qemu_free(s);
    return NULL;
}

QEMUFile *qemu_fopen_socket(int fd)
{
    QEMUFileSocket *s = qemu_mallocz(sizeof(QEMUFileSocket));

    s->fd = fd;
    s->file = qemu_fopen_ops(s, NULL, socket_get_buffer, file_socket_close,
			     NULL, NULL, NULL);
    return s->file;
}

static int file_put_buffer(void *opaque, const uint8_t *buf,
                            int64_t pos, int size)
{
    QEMUFileStdio *s = opaque;
    fseek(s->stdio_file, pos, SEEK_SET);
    return fwrite(buf, 1, size, s->stdio_file);
}

static int file_get_buffer(void *opaque, uint8_t *buf, int64_t pos, int size)
{
    QEMUFileStdio *s = opaque;
    fseek(s->stdio_file, pos, SEEK_SET);
    return fread(buf, 1, size, s->stdio_file);
}

QEMUFile *qemu_fopen(const char *filename, const char *mode)
{
    QEMUFileStdio *s;

    if (mode == NULL ||
	(mode[0] != 'r' && mode[0] != 'w') ||
	mode[1] != 'b' || mode[2] != 0) {
        fprintf(stderr, "qemu_fopen: Argument validity check failed\n");
        return NULL;
    }

    s = qemu_mallocz(sizeof(QEMUFileStdio));

    s->stdio_file = fopen(filename, mode);
    if (!s->stdio_file)
        goto fail;

    if(mode[0] == 'w') {
        s->file = qemu_fopen_ops(s, file_put_buffer, NULL, stdio_fclose,
				 NULL, NULL, NULL);
    } else {
        s->file = qemu_fopen_ops(s, NULL, file_get_buffer, stdio_fclose,
			       NULL, NULL, NULL);
    }
    return s->file;
fail:
    qemu_free(s);
    return NULL;
}

static int block_put_buffer(void *opaque, const uint8_t *buf,
                           int64_t pos, int size)
{
    bdrv_save_vmstate(opaque, buf, pos, size);
    return size;
}

static int block_get_buffer(void *opaque, uint8_t *buf, int64_t pos, int size)
{
    return bdrv_load_vmstate(opaque, buf, pos, size);
}

static int bdrv_fclose(void *opaque)
{
    return 0;
}

static QEMUFile *qemu_fopen_bdrv(BlockDriverState *bs, int is_writable)
{
    if (is_writable)
        return qemu_fopen_ops(bs, block_put_buffer, NULL, bdrv_fclose,
			      NULL, NULL, NULL);
    return qemu_fopen_ops(bs, NULL, block_get_buffer, bdrv_fclose, NULL, NULL, NULL);
}

QEMUFile *qemu_fopen_ops(void *opaque, QEMUFilePutBufferFunc *put_buffer,
                         QEMUFileGetBufferFunc *get_buffer,
                         QEMUFileCloseFunc *close,
                         QEMUFileRateLimit *rate_limit,
                         QEMUFileSetRateLimit *set_rate_limit,
                         QEMUFileGetRateLimit *get_rate_limit)
{
    QEMUFile *f;

    f = qemu_mallocz(sizeof(QEMUFile));

    f->opaque = opaque;
    f->put_buffer = put_buffer;
    f->get_buffer = get_buffer;
    f->close = close;
    f->rate_limit = rate_limit;
    f->set_rate_limit = set_rate_limit;
    f->get_rate_limit = get_rate_limit;
    f->is_write = 0;

    return f;
}

int qemu_file_has_error(QEMUFile *f)
{
    return f->has_error;
}

void qemu_file_set_error(QEMUFile *f)
{
    f->has_error = 1;
}

void qemu_fflush(QEMUFile *f)
{
    if (!f->put_buffer)
        return;

    if (f->is_write && f->buf_index > 0) {
        int len;

        len = f->put_buffer(f->opaque, f->buf, f->buf_offset, f->buf_index);
        if (len > 0)
            f->buf_offset += f->buf_index;
        else
            f->has_error = 1;
        f->buf_index = 0;
    }
}

static void qemu_fill_buffer(QEMUFile *f)
{
    int len;

    if (!f->get_buffer)
        return;

    if (f->is_write)
        abort();

    len = f->get_buffer(f->opaque, f->buf, f->buf_offset, IO_BUF_SIZE);
    if (len > 0) {
        f->buf_index = 0;
        f->buf_size = len;
        f->buf_offset += len;
    } else if (len != -EAGAIN)
        f->has_error = 1;
}

int qemu_fclose(QEMUFile *f)
{
    int ret = 0;
    qemu_fflush(f);
    if (f->close)
        ret = f->close(f->opaque);
    qemu_free(f);
    return ret;
}

void qemu_file_put_notify(QEMUFile *f)
{
    f->put_buffer(f->opaque, NULL, 0, 0);
}

void qemu_put_buffer(QEMUFile *f, const uint8_t *buf, int size)
{
    int l;

    if (!f->has_error && f->is_write == 0 && f->buf_index > 0) {
        fprintf(stderr,
                "Attempted to write to buffer while read buffer is not empty\n");
        abort();
    }

    while (!f->has_error && size > 0) {
        l = IO_BUF_SIZE - f->buf_index;
        if (l > size)
            l = size;
        memcpy(f->buf + f->buf_index, buf, l);
        f->is_write = 1;
        f->buf_index += l;
        buf += l;
        size -= l;
        if (f->buf_index >= IO_BUF_SIZE)
            qemu_fflush(f);
    }
}

void qemu_put_byte(QEMUFile *f, int v)
{
    if (!f->has_error && f->is_write == 0 && f->buf_index > 0) {
        fprintf(stderr,
                "Attempted to write to buffer while read buffer is not empty\n");
        abort();
    }

    f->buf[f->buf_index++] = v;
    f->is_write = 1;
    if (f->buf_index >= IO_BUF_SIZE)
        qemu_fflush(f);
}

int qemu_get_buffer(QEMUFile *f, uint8_t *buf, int size1)
{
    int size, l;

    if (f->is_write)
        abort();

    size = size1;
    while (size > 0) {
        l = f->buf_size - f->buf_index;
        if (l == 0) {
            qemu_fill_buffer(f);
            l = f->buf_size - f->buf_index;
            if (l == 0)
                break;
        }
        if (l > size)
            l = size;
        memcpy(buf, f->buf + f->buf_index, l);
        f->buf_index += l;
        buf += l;
        size -= l;
    }
    return size1 - size;
}

int qemu_get_byte(QEMUFile *f)
{
    if (f->is_write)
        abort();

    if (f->buf_index >= f->buf_size) {
        qemu_fill_buffer(f);
        if (f->buf_index >= f->buf_size)
            return 0;
    }
    return f->buf[f->buf_index++];
}

#ifdef CONFIG_ANDROID
void qemu_put_string(QEMUFile *f, const char* str)
{
    /* We will encode NULL and the empty string in the same way */
    int  slen;
    if (str == NULL) {
        str = "";
    }
    slen = strlen(str);
    qemu_put_be32(f, slen);
    qemu_put_buffer(f, (const uint8_t*)str, slen);
}

char* qemu_get_string(QEMUFile *f)
{
    int slen = qemu_get_be32(f);
    char* str;
    if (slen == 0)
        return NULL;

    str = qemu_malloc(slen+1);
    if (qemu_get_buffer(f, (uint8_t*)str, slen) != slen) {
        qemu_free(str);
        return NULL;
    }
    str[slen] = '\0';
    return str;
}
#endif


int64_t qemu_ftell(QEMUFile *f)
{
    return f->buf_offset - f->buf_size + f->buf_index;
}

int64_t qemu_fseek(QEMUFile *f, int64_t pos, int whence)
{
    if (whence == SEEK_SET) {
        /* nothing to do */
    } else if (whence == SEEK_CUR) {
        pos += qemu_ftell(f);
    } else {
        /* SEEK_END not supported */
        return -1;
    }
    if (f->put_buffer) {
        qemu_fflush(f);
        f->buf_offset = pos;
    } else {
        f->buf_offset = pos;
        f->buf_index = 0;
        f->buf_size = 0;
    }
    return pos;
}

int qemu_file_rate_limit(QEMUFile *f)
{
    if (f->rate_limit)
        return f->rate_limit(f->opaque);

    return 0;
}

int64_t qemu_file_get_rate_limit(QEMUFile *f)
{
    if (f->get_rate_limit)
        return f->get_rate_limit(f->opaque);

    return 0;
}

int64_t qemu_file_set_rate_limit(QEMUFile *f, int64_t new_rate)
{
    /* any failed or completed migration keeps its state to allow probing of
     * migration data, but has no associated file anymore */
    if (f && f->set_rate_limit)
        return f->set_rate_limit(f->opaque, new_rate);

    return 0;
}

void qemu_put_be16(QEMUFile *f, unsigned int v)
{
    qemu_put_byte(f, v >> 8);
    qemu_put_byte(f, v);
}

void qemu_put_be32(QEMUFile *f, unsigned int v)
{
    qemu_put_byte(f, v >> 24);
    qemu_put_byte(f, v >> 16);
    qemu_put_byte(f, v >> 8);
    qemu_put_byte(f, v);
}

void qemu_put_be64(QEMUFile *f, uint64_t v)
{
    qemu_put_be32(f, v >> 32);
    qemu_put_be32(f, v);
}

unsigned int qemu_get_be16(QEMUFile *f)
{
    unsigned int v;
    v = qemu_get_byte(f) << 8;
    v |= qemu_get_byte(f);
    return v;
}

unsigned int qemu_get_be32(QEMUFile *f)
{
    unsigned int v;
    v = qemu_get_byte(f) << 24;
    v |= qemu_get_byte(f) << 16;
    v |= qemu_get_byte(f) << 8;
    v |= qemu_get_byte(f);
    return v;
}

uint64_t qemu_get_be64(QEMUFile *f)
{
    uint64_t v;
    v = (uint64_t)qemu_get_be32(f) << 32;
    v |= qemu_get_be32(f);
    return v;
}

void  qemu_put_struct(QEMUFile*  f, const QField*  fields, const void*  s)
{
    const QField*  qf = fields;

    /* Iterate over struct fields */
    while (qf->type != Q_FIELD_END) {
        uint8_t*  p = (uint8_t*)s + qf->offset;

        switch (qf->type) {
            case Q_FIELD_BYTE:
                qemu_put_byte(f, p[0]);
                break;
            case Q_FIELD_INT16:
                qemu_put_be16(f, ((uint16_t*)p)[0]);
                break;
            case Q_FIELD_INT32:
                qemu_put_be32(f, ((uint32_t*)p)[0]);
                break;
            case Q_FIELD_INT64:
                qemu_put_be64(f, ((uint64_t*)p)[0]);
                break;
            case Q_FIELD_BUFFER:
                if (qf[1].type != Q_FIELD_BUFFER_SIZE ||
                    qf[2].type != Q_FIELD_BUFFER_SIZE)
                {
                    fprintf(stderr, "%s: invalid QFIELD_BUFFER item passed as argument. aborting\n",
                            __FUNCTION__ );
                    exit(1);
                }
                else
                {
                    uint32_t  size = ((uint32_t)qf[1].offset << 16) | (uint32_t)qf[2].offset;

                    qemu_put_buffer(f, p, size);
                    qf += 2;
                }
                break;
            default:
                fprintf(stderr, "%s: invalid fields list passed as argument. aborting\n", __FUNCTION__);
                exit(1);
        }
        qf++;
    }
}

int   qemu_get_struct(QEMUFile*  f, const QField*  fields, void*  s)
{
    const QField*  qf = fields;

    /* Iterate over struct fields */
    while (qf->type != Q_FIELD_END) {
        uint8_t*  p = (uint8_t*)s + qf->offset;

        switch (qf->type) {
            case Q_FIELD_BYTE:
                p[0] = qemu_get_byte(f);
                break;
            case Q_FIELD_INT16:
                ((uint16_t*)p)[0] = qemu_get_be16(f);
                break;
            case Q_FIELD_INT32:
                ((uint32_t*)p)[0] = qemu_get_be32(f);
                break;
            case Q_FIELD_INT64:
                ((uint64_t*)p)[0] = qemu_get_be64(f);
                break;
            case Q_FIELD_BUFFER:
                if (qf[1].type != Q_FIELD_BUFFER_SIZE ||
                        qf[2].type != Q_FIELD_BUFFER_SIZE)
                {
                    fprintf(stderr, "%s: invalid QFIELD_BUFFER item passed as argument.\n",
                            __FUNCTION__ );
                    return -1;
                }
                else
                {
                    uint32_t  size = ((uint32_t)qf[1].offset << 16) | (uint32_t)qf[2].offset;
                    int       ret  = qemu_get_buffer(f, p, size);

                    if (ret != size) {
                        fprintf(stderr, "%s: not enough bytes to load structure\n", __FUNCTION__);
                        return -1;
                    }
                    qf += 2;
                }
                break;
            default:
                fprintf(stderr, "%s: invalid fields list passed as argument. aborting\n", __FUNCTION__);
                exit(1);
        }
        qf++;
    }
    return 0;
}

/* write a float to file */
void qemu_put_float(QEMUFile *f, float v)
{
    uint8_t *bytes = (uint8_t*) &v;
    qemu_put_buffer(f, bytes, sizeof(float));
}

/* read a float from file */
float qemu_get_float(QEMUFile *f)
{
    uint8_t bytes[sizeof(float)];
    qemu_get_buffer(f, bytes, sizeof(float));

    return *((float*) bytes);
}

typedef struct SaveStateEntry {
    char idstr[256];
    int instance_id;
    int version_id;
    int section_id;
    SaveLiveStateHandler *save_live_state;
    SaveStateHandler *save_state;
    LoadStateHandler *load_state;
    void *opaque;
    struct SaveStateEntry *next;
} SaveStateEntry;

static SaveStateEntry *first_se;

/* TODO: Individual devices generally have very little idea about the rest
   of the system, so instance_id should be removed/replaced.
   Meanwhile pass -1 as instance_id if you do not already have a clearly
   distinguishing id for all instances of your device class. */
int register_savevm_live(const char *idstr,
                         int instance_id,
                         int version_id,
                         SaveLiveStateHandler *save_live_state,
                         SaveStateHandler *save_state,
                         LoadStateHandler *load_state,
                         void *opaque)
{
    SaveStateEntry *se, **pse;
    static int global_section_id;

    se = qemu_malloc(sizeof(SaveStateEntry));
    pstrcpy(se->idstr, sizeof(se->idstr), idstr);
    se->instance_id = (instance_id == -1) ? 0 : instance_id;
    se->version_id = version_id;
    se->section_id = global_section_id++;
    se->save_live_state = save_live_state;
    se->save_state = save_state;
    se->load_state = load_state;
    se->opaque = opaque;
    se->next = NULL;

    /* add at the end of list */
    pse = &first_se;
    while (*pse != NULL) {
        if (instance_id == -1
                && strcmp(se->idstr, (*pse)->idstr) == 0
                && se->instance_id <= (*pse)->instance_id)
            se->instance_id = (*pse)->instance_id + 1;
        pse = &(*pse)->next;
    }
    *pse = se;
    return 0;
}

int register_savevm(const char *idstr,
                    int instance_id,
                    int version_id,
                    SaveStateHandler *save_state,
                    LoadStateHandler *load_state,
                    void *opaque)
{
    return register_savevm_live(idstr, instance_id, version_id,
                                NULL, save_state, load_state, opaque);
}

void unregister_savevm(const char *idstr, void *opaque)
{
    SaveStateEntry **pse;

    pse = &first_se;
    while (*pse != NULL) {
        if (strcmp((*pse)->idstr, idstr) == 0 && (*pse)->opaque == opaque) {
            SaveStateEntry *next = (*pse)->next;
            qemu_free(*pse);
            *pse = next;
            continue;
        }
        pse = &(*pse)->next;
    }
}

#define QEMU_VM_FILE_MAGIC           0x5145564d
#define QEMU_VM_FILE_VERSION_COMPAT  0x00000002
#define QEMU_VM_FILE_VERSION         0x00000004

#define QEMU_VM_EOF                  0x00
#define QEMU_VM_SECTION_START        0x01
#define QEMU_VM_SECTION_PART         0x02
#define QEMU_VM_SECTION_END          0x03
#define QEMU_VM_SECTION_FULL         0x04

int qemu_savevm_state_begin(QEMUFile *f)
{
    SaveStateEntry *se;

    qemu_put_be32(f, QEMU_VM_FILE_MAGIC);
    qemu_put_be32(f, QEMU_VM_FILE_VERSION);

    for (se = first_se; se != NULL; se = se->next) {
        int len;

        if (se->save_live_state == NULL)
            continue;

        /* Section type */
        qemu_put_byte(f, QEMU_VM_SECTION_START);
        qemu_put_be32(f, se->section_id);

        /* ID string */
        len = strlen(se->idstr);
        qemu_put_byte(f, len);
        qemu_put_buffer(f, (uint8_t *)se->idstr, len);

        qemu_put_be32(f, se->instance_id);
        qemu_put_be32(f, se->version_id);

        se->save_live_state(f, QEMU_VM_SECTION_START, se->opaque);
    }

    if (qemu_file_has_error(f))
        return -EIO;

    return 0;
}

int qemu_savevm_state_iterate(QEMUFile *f)
{
    SaveStateEntry *se;
    int ret = 1;

    for (se = first_se; se != NULL; se = se->next) {
        if (se->save_live_state == NULL)
            continue;

        /* Section type */
        qemu_put_byte(f, QEMU_VM_SECTION_PART);
        qemu_put_be32(f, se->section_id);

        ret &= !!se->save_live_state(f, QEMU_VM_SECTION_PART, se->opaque);
    }

    if (ret)
        return 1;

    if (qemu_file_has_error(f))
        return -EIO;

    return 0;
}

int qemu_savevm_state_complete(QEMUFile *f)
{
    SaveStateEntry *se;

    for (se = first_se; se != NULL; se = se->next) {
        if (se->save_live_state == NULL)
            continue;

        /* Section type */
        qemu_put_byte(f, QEMU_VM_SECTION_END);
        qemu_put_be32(f, se->section_id);

        se->save_live_state(f, QEMU_VM_SECTION_END, se->opaque);
    }

    for(se = first_se; se != NULL; se = se->next) {
        int len;

        if (se->save_state == NULL)
            continue;

        /* Section type */
        qemu_put_byte(f, QEMU_VM_SECTION_FULL);
        qemu_put_be32(f, se->section_id);

        /* ID string */
        len = strlen(se->idstr);
        qemu_put_byte(f, len);
        qemu_put_buffer(f, (uint8_t *)se->idstr, len);

        qemu_put_be32(f, se->instance_id);
        qemu_put_be32(f, se->version_id);

        se->save_state(f, se->opaque);
    }

    qemu_put_byte(f, QEMU_VM_EOF);

    if (qemu_file_has_error(f))
        return -EIO;

    return 0;
}

int qemu_savevm_state(QEMUFile *f)
{
    int saved_vm_running;
    int ret;

    saved_vm_running = vm_running;
    vm_stop(0);

    bdrv_flush_all();

    ret = qemu_savevm_state_begin(f);
    if (ret < 0)
        goto out;

    do {
        ret = qemu_savevm_state_iterate(f);
        if (ret < 0)
            goto out;
    } while (ret == 0);

    ret = qemu_savevm_state_complete(f);

out:
    if (qemu_file_has_error(f))
        ret = -EIO;

    if (!ret && saved_vm_running)
        vm_start();

    return ret;
}

static SaveStateEntry *find_se(const char *idstr, int instance_id)
{
    SaveStateEntry *se;

    for(se = first_se; se != NULL; se = se->next) {
        if (!strcmp(se->idstr, idstr) &&
            instance_id == se->instance_id)
            return se;
    }
    return NULL;
}

typedef struct LoadStateEntry {
    SaveStateEntry *se;
    int section_id;
    int version_id;
    struct LoadStateEntry *next;
} LoadStateEntry;

static int qemu_loadvm_state_v2(QEMUFile *f)
{
    SaveStateEntry *se;
    int len, ret, instance_id, record_len, version_id;
    int64_t total_len, end_pos, cur_pos;
    char idstr[256];

    total_len = qemu_get_be64(f);
    end_pos = total_len + qemu_ftell(f);
    for(;;) {
        if (qemu_ftell(f) >= end_pos)
            break;
        len = qemu_get_byte(f);
        qemu_get_buffer(f, (uint8_t *)idstr, len);
        idstr[len] = '\0';
        instance_id = qemu_get_be32(f);
        version_id = qemu_get_be32(f);
        record_len = qemu_get_be32(f);
        cur_pos = qemu_ftell(f);
        se = find_se(idstr, instance_id);
        if (!se) {
            fprintf(stderr, "qemu: warning: instance 0x%x of device '%s' not present in current VM\n",
                    instance_id, idstr);
        } else {
            ret = se->load_state(f, se->opaque, version_id);
            if (ret < 0) {
                fprintf(stderr, "qemu: warning: error while loading state for instance 0x%x of device '%s'\n",
                        instance_id, idstr);
                return ret;
            }
        }
        /* always seek to exact end of record */
        qemu_fseek(f, cur_pos + record_len, SEEK_SET);
    }

    if (qemu_file_has_error(f))
        return -EIO;

    return 0;
}

int qemu_loadvm_state(QEMUFile *f)
{
    LoadStateEntry *first_le = NULL;
    uint8_t section_type;
    unsigned int v;
    int ret;

    v = qemu_get_be32(f);
    if (v != QEMU_VM_FILE_MAGIC)
        return -EINVAL;

    v = qemu_get_be32(f);
    if (v == QEMU_VM_FILE_VERSION_COMPAT)
        return qemu_loadvm_state_v2(f);
    if (v < QEMU_VM_FILE_VERSION) {
        fprintf(stderr, "Snapshot format %d is too old for this version of the emulator, please create a new one.\n", v);
        return -ENOTSUP;
    } else if (v > QEMU_VM_FILE_VERSION) {
        fprintf(stderr, "Snapshot format %d is more recent than the emulator, please update your Android SDK Tools.\n", v);
        return -ENOTSUP;
    }

    while ((section_type = qemu_get_byte(f)) != QEMU_VM_EOF) {
        uint32_t instance_id, version_id, section_id;
        LoadStateEntry *le;
        SaveStateEntry *se;
        char idstr[257];
        int len;

        switch (section_type) {
        case QEMU_VM_SECTION_START:
        case QEMU_VM_SECTION_FULL:
            /* Read section start */
            section_id = qemu_get_be32(f);
            len = qemu_get_byte(f);
            qemu_get_buffer(f, (uint8_t *)idstr, len);
            idstr[len] = 0;
            instance_id = qemu_get_be32(f);
            version_id = qemu_get_be32(f);

            /* Find savevm section */
            se = find_se(idstr, instance_id);
            if (se == NULL) {
                fprintf(stderr, "Unknown savevm section or instance '%s' %d\n", idstr, instance_id);
                ret = -EINVAL;
                goto out;
            }

            /* Validate version */
            if (version_id > se->version_id) {
                fprintf(stderr, "savevm: unsupported version %d for '%s' v%d\n",
                        version_id, idstr, se->version_id);
                ret = -EINVAL;
                goto out;
            }

            /* Add entry */
            le = qemu_mallocz(sizeof(*le));

            le->se = se;
            le->section_id = section_id;
            le->version_id = version_id;
            le->next = first_le;
            first_le = le;

            if (le->se->load_state(f, le->se->opaque, le->version_id)) {
                fprintf(stderr, "savevm: unable to load section %s\n", idstr);
                ret = -EINVAL;
                goto out;
            }
            break;
        case QEMU_VM_SECTION_PART:
        case QEMU_VM_SECTION_END:
            section_id = qemu_get_be32(f);

            for (le = first_le; le && le->section_id != section_id; le = le->next);
            if (le == NULL) {
                fprintf(stderr, "Unknown savevm section %d\n", section_id);
                ret = -EINVAL;
                goto out;
            }

            le->se->load_state(f, le->se->opaque, le->version_id);
            break;
        default:
            fprintf(stderr, "Unknown savevm section type %d\n", section_type);
            ret = -EINVAL;
            goto out;
        }
    }

    ret = 0;

out:
    while (first_le) {
        LoadStateEntry *le = first_le;
        first_le = first_le->next;
        qemu_free(le);
    }

    if (qemu_file_has_error(f))
        ret = -EIO;

    return ret;
}
#if 0
static BlockDriverState *get_bs_snapshots(void)
{
    BlockDriverState *bs;
    int i;

    if (bs_snapshots)
        return bs_snapshots;
    for(i = 0; i <= nb_drives; i++) {
        bs = drives_table[i].bdrv;
        if (bdrv_can_snapshot(bs))
            goto ok;
    }
    return NULL;
 ok:
    bs_snapshots = bs;
    return bs;
}
#endif
static int bdrv_snapshot_find(BlockDriverState *bs, QEMUSnapshotInfo *sn_info,
                              const char *name)
{
    QEMUSnapshotInfo *sn_tab, *sn;
    int nb_sns, i, ret;

    ret = -ENOENT;
    nb_sns = bdrv_snapshot_list(bs, &sn_tab);
    if (nb_sns < 0)
        return ret;
    for(i = 0; i < nb_sns; i++) {
        sn = &sn_tab[i];
        if (!strcmp(sn->id_str, name) || !strcmp(sn->name, name)) {
            *sn_info = *sn;
            ret = 0;
            break;
        }
    }
    qemu_free(sn_tab);
    return ret;
}

void do_savevm(Monitor *err, const char *name)
{
    BlockDriverState *bs, *bs1;
    QEMUSnapshotInfo sn1, *sn = &sn1, old_sn1, *old_sn = &old_sn1;
    int must_delete, ret;
    BlockDriverInfo bdi1, *bdi = &bdi1;
    QEMUFile *f;
    int saved_vm_running;
    uint32_t vm_state_size;
#ifdef _WIN32
    struct _timeb tb;
#else
    struct timeval tv;
#endif

    bs = bdrv_snapshots();
    if (!bs) {
        monitor_printf(err, "No block device can accept snapshots\n");
        return;
    }

    /* ??? Should this occur after vm_stop?  */
    qemu_aio_flush();

    saved_vm_running = vm_running;
    vm_stop(0);

    must_delete = 0;
    if (name) {
        ret = bdrv_snapshot_find(bs, old_sn, name);
        if (ret >= 0) {
            must_delete = 1;
        }
    }
    memset(sn, 0, sizeof(*sn));
    if (must_delete) {
        pstrcpy(sn->name, sizeof(sn->name), old_sn->name);
        pstrcpy(sn->id_str, sizeof(sn->id_str), old_sn->id_str);
    } else {
        if (name)
            pstrcpy(sn->name, sizeof(sn->name), name);
    }

    /* fill auxiliary fields */
#ifdef _WIN32
    _ftime(&tb);
    sn->date_sec = tb.time;
    sn->date_nsec = tb.millitm * 1000000;
#else
    gettimeofday(&tv, NULL);
    sn->date_sec = tv.tv_sec;
    sn->date_nsec = tv.tv_usec * 1000;
#endif
    sn->vm_clock_nsec = qemu_get_clock_ns(vm_clock);

    if (bdrv_get_info(bs, bdi) < 0 || bdi->vm_state_offset <= 0) {
        monitor_printf(err, "Device %s does not support VM state snapshots\n",
                              bdrv_get_device_name(bs));
        goto the_end;
    }

    /* save the VM state */
    f = qemu_fopen_bdrv(bs, 1);
    if (!f) {
        monitor_printf(err, "Could not open VM state file\n");
        goto the_end;
    }
    ret = qemu_savevm_state(f);
    vm_state_size = qemu_ftell(f);
    qemu_fclose(f);
    if (ret < 0) {
        monitor_printf(err, "Error %d while writing VM\n", ret);
        goto the_end;
    }

    /* create the snapshots */

    bs1 = NULL;
    while ((bs1 = bdrv_next(bs1))) {
        if (bdrv_can_snapshot(bs1)) {
            if (must_delete) {
                ret = bdrv_snapshot_delete(bs1, old_sn->id_str);
                if (ret < 0) {
                    monitor_printf(err,
                                          "Error while deleting snapshot on '%s'\n",
                                          bdrv_get_device_name(bs1));
                }
            }
            /* Write VM state size only to the image that contains the state */
            sn->vm_state_size = (bs == bs1 ? vm_state_size : 0);
            ret = bdrv_snapshot_create(bs1, sn);
            if (ret < 0) {
                monitor_printf(err, "Error while creating snapshot on '%s'\n",
                                      bdrv_get_device_name(bs1));
            }
        }
    }

 the_end:
    if (saved_vm_running)
        vm_start();
}

void do_loadvm(Monitor *err, const char *name)
{
    BlockDriverState *bs, *bs1;
    BlockDriverInfo bdi1, *bdi = &bdi1;
    QEMUSnapshotInfo sn;
    QEMUFile *f;
    int ret;
    int saved_vm_running;

    bs = bdrv_snapshots();
    if (!bs) {
        monitor_printf(err, "No block device supports snapshots\n");
        return;
    }

    /* Flush all IO requests so they don't interfere with the new state.  */
    qemu_aio_flush();

    saved_vm_running = vm_running;
    vm_stop(0);

    bs1 = bs;
    do {
        if (bdrv_can_snapshot(bs1)) {
            ret = bdrv_snapshot_goto(bs1, name);
            if (ret < 0) {
                if (bs != bs1)
                    monitor_printf(err, "Warning: ");
                switch(ret) {
                case -ENOTSUP:
                    monitor_printf(err,
                                   "Snapshots not supported on device '%s'\n",
                                   bdrv_get_device_name(bs1));
                    break;
                case -ENOENT:
                    monitor_printf(err, "Could not find snapshot '%s' on "
                                   "device '%s'\n",
                                   name, bdrv_get_device_name(bs1));
                    break;
                default:
                    monitor_printf(err, "Error %d while activating snapshot on"
                                   " '%s'\n", ret, bdrv_get_device_name(bs1));
                    break;
                }
                /* fatal on snapshot block device */
                if (bs == bs1)
                    goto the_end;
            }
        }
    } while ((bs1 = bdrv_next(bs)));

    if (bdrv_get_info(bs, bdi) < 0 || bdi->vm_state_offset <= 0) {
        monitor_printf(err, "Device %s does not support VM state snapshots\n",
                       bdrv_get_device_name(bs));
        return;
    }

    /* Don't even try to load empty VM states */
    ret = bdrv_snapshot_find(bs, &sn, name);
    if ((ret >= 0) && (sn.vm_state_size == 0))
        goto the_end;

    /* restore the VM state */
    f = qemu_fopen_bdrv(bs, 0);
    if (!f) {
        monitor_printf(err, "Could not open VM state file\n");
        goto the_end;
    }
    ret = qemu_loadvm_state(f);
    qemu_fclose(f);
    if (ret < 0) {
        monitor_printf(err, "Error %d while loading VM state\n", ret);
    }
 the_end:
    if (saved_vm_running)
        vm_start();
}

void do_delvm(Monitor *err, const char *name)
{
    BlockDriverState *bs, *bs1;
    int ret;

    bs = bdrv_snapshots();
    if (!bs) {
        monitor_printf(err, "No block device supports snapshots\n");
        return;
    }

    bs1 = NULL;
    while ((bs1 = bdrv_next(bs1))) {
        if (bdrv_can_snapshot(bs1)) {
            ret = bdrv_snapshot_delete(bs1, name);
            if (ret < 0) {
                if (ret == -ENOTSUP)
                    monitor_printf(err,
                                          "Snapshots not supported on device '%s'\n",
                                          bdrv_get_device_name(bs1));
                else
                    monitor_printf(err, "Error %d while deleting snapshot on "
                                          "'%s'\n", ret, bdrv_get_device_name(bs1));
            }
        }
    }
}

void do_info_snapshots(Monitor* out, Monitor* err)
{
    BlockDriverState *bs, *bs1;
    QEMUSnapshotInfo *sn_tab, *sn;
    int nb_sns, i;
    char buf[256];

    bs = bdrv_snapshots();
    if (!bs) {
        monitor_printf(err, "No available block device supports snapshots\n");
        return;
    }
    monitor_printf(out, "Snapshot devices:");
    bs1 = NULL;
    while ((bs1 = bdrv_next(bs1))) {
        if (bdrv_can_snapshot(bs1)) {
            if (bs == bs1)
                monitor_printf(out, " %s", bdrv_get_device_name(bs1));
        }
    }
    monitor_printf(out, "\n");

    nb_sns = bdrv_snapshot_list(bs, &sn_tab);
    if (nb_sns < 0) {
        monitor_printf(err, "bdrv_snapshot_list: error %d\n", nb_sns);
        return;
    }
    monitor_printf(out, "Snapshot list (from %s):\n",
                   bdrv_get_device_name(bs));
    monitor_printf(out, "%s\n", bdrv_snapshot_dump(buf, sizeof(buf), NULL));
    for(i = 0; i < nb_sns; i++) {
        sn = &sn_tab[i];
        monitor_printf(out, "%s\n", bdrv_snapshot_dump(buf, sizeof(buf), sn));
    }
    qemu_free(sn_tab);
}
