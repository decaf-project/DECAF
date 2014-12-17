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
#include "goldfish_device.h"

enum {
    SW_NAME_LEN     = 0x00,
    SW_NAME_PTR     = 0x04,
    SW_FLAGS        = 0x08,
    SW_STATE        = 0x0c,
    SW_INT_STATUS   = 0x10,
    SW_INT_ENABLE   = 0x14,

    SW_FLAGS_OUTPUT = 1U << 0
};


struct switch_state {
    struct goldfish_device dev;
    char *name;
    uint32_t state;
    uint32_t state_changed : 1;
    uint32_t int_enable : 1;
    uint32_t (*writefn)(void *opaque, uint32_t state);
    void *writeopaque;
};

#define  GOLDFISH_SWITCH_SAVE_VERSION  1

static void  goldfish_switch_save(QEMUFile*  f, void*  opaque)
{
    struct switch_state*  s = opaque;

    qemu_put_be32(f, s->state);
    qemu_put_byte(f, s->state_changed);
    qemu_put_byte(f, s->int_enable);
}

static int  goldfish_switch_load(QEMUFile*  f, void*  opaque, int  version_id)
{
    struct switch_state*  s = opaque;

    if (version_id != GOLDFISH_SWITCH_SAVE_VERSION)
        return -1;

    s->state         = qemu_get_be32(f);
    s->state_changed = qemu_get_byte(f);
    s->int_enable    = qemu_get_byte(f);

    return 0;
}

static uint32_t goldfish_switch_read(void *opaque, target_phys_addr_t offset)
{
    struct switch_state *s = (struct switch_state *)opaque;

    //printf("goldfish_switch_read %x %x\n", offset, size);

    switch (offset) {
        case SW_NAME_LEN:
            return strlen(s->name);
        case SW_FLAGS:
            return s->writefn ? SW_FLAGS_OUTPUT : 0;
        case SW_STATE:
            return s->state;
        case SW_INT_STATUS:
            if(s->state_changed && s->int_enable) {
                s->state_changed = 0;
                goldfish_device_set_irq(&s->dev, 0, 0);
                return 1;
            }
            return 0;
    default:
        cpu_abort (cpu_single_env, "goldfish_switch_read: Bad offset %x\n", offset);
        return 0;
    }
}

static void goldfish_switch_write(void *opaque, target_phys_addr_t offset, uint32_t value)
{
    struct switch_state *s = (struct switch_state *)opaque;

    //printf("goldfish_switch_read %x %x %x\n", offset, value, size);

    switch(offset) {
        case SW_NAME_PTR:
            cpu_memory_rw_debug(cpu_single_env, value, (void*)s->name, strlen(s->name), 1);
            break;

        case SW_STATE:
            if(s->writefn) {
                uint32_t new_state;
                new_state = s->writefn(s->writeopaque, value);
                if(new_state != s->state) {
                    goldfish_switch_set_state(s, new_state);
                }
            }
            else
                cpu_abort (cpu_single_env, "goldfish_switch_write: write to SW_STATE on input\n");
            break;

        case SW_INT_ENABLE:
            value &= 1;
            if(s->state_changed && s->int_enable != value)
                goldfish_device_set_irq(&s->dev, 0, value);
            s->int_enable = value;
            break;

        default:
            cpu_abort (cpu_single_env, "goldfish_switch_write: Bad offset %x\n", offset);
    }
}

static CPUReadMemoryFunc *goldfish_switch_readfn[] = {
    goldfish_switch_read,
    goldfish_switch_read,
    goldfish_switch_read
};

static CPUWriteMemoryFunc *goldfish_switch_writefn[] = {
    goldfish_switch_write,
    goldfish_switch_write,
    goldfish_switch_write
};

void goldfish_switch_set_state(void *opaque, uint32_t state)
{
    struct switch_state *s = opaque;
    s->state_changed = 1;
    s->state = state;
    if(s->int_enable)
        goldfish_device_set_irq(&s->dev, 0, 1);
}

void *goldfish_switch_add(char *name, uint32_t (*writefn)(void *opaque, uint32_t state), void *writeopaque, int id)
{
    int ret;
    struct switch_state *s;

    s = qemu_mallocz(sizeof(*s));
    s->dev.name = "goldfish-switch";
    s->dev.id = id;
    s->dev.size = 0x1000;
    s->dev.irq_count = 1;
    s->name = name;
    s->writefn = writefn;
    s->writeopaque = writeopaque;


    ret = goldfish_device_add(&s->dev, goldfish_switch_readfn, goldfish_switch_writefn, s);
    if(ret) {
        qemu_free(s);
        return NULL;
    }

    register_savevm( "goldfish_switch", 0, GOLDFISH_SWITCH_SAVE_VERSION,
                     goldfish_switch_save, goldfish_switch_load, s);

    return s;
}

