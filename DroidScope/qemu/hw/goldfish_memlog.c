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
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "qemu_file.h"
#include "goldfish_device.h"
#include "audio/audio.h"

extern void  dprint(const char*  fmt, ...);

int fd = -1;

static uint32_t memlog_read(void *opaque, target_phys_addr_t offset)
{
    (void)opaque;
    (void)offset;
    return 0;
}

unsigned info[8];

static void memlog_write(void *opaque, target_phys_addr_t offset, uint32_t val)
{
    char buf[128];
    struct goldfish_device *dev = opaque;
    int ret;

    (void)dev;

    if (offset < 8*4)
        info[offset / 4] = val;

    if (offset == 0) {
            /* write PID and VADDR to logfile */
        snprintf(buf, sizeof buf, "%08x %08x\n", info[0], info[1]);
        do {
            ret = write(fd, buf, strlen(buf));
        } while (ret < 0 && errno == EINTR);
    }
}


static CPUReadMemoryFunc *memlog_readfn[] = {
   memlog_read,
   memlog_read,
   memlog_read
};

static CPUWriteMemoryFunc *memlog_writefn[] = {
   memlog_write,
   memlog_write,
   memlog_write
};

struct goldfish_device memlog_dev;

void goldfish_memlog_init(uint32_t base)
{
    struct goldfish_device *dev = &memlog_dev;

    dev->name = "goldfish_memlog";
    dev->id = 0;
    dev->base = base;
    dev->size = 0x1000;
    dev->irq_count = 0;

    do {
        fd = open("mem.log", /* O_CREAT | */ O_TRUNC | O_WRONLY, 0644);
    } while (fd < 0 && errno == EINTR);

    goldfish_device_add(dev, memlog_readfn, memlog_writefn, dev);
}

