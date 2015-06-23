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
#include "arm_pic.h"
#include "goldfish_device.h"
#include "irq.h"

enum {
    INTERRUPT_STATUS        = 0x00, // number of pending interrupts
    INTERRUPT_NUMBER        = 0x04,
    INTERRUPT_DISABLE_ALL   = 0x08,
    INTERRUPT_DISABLE       = 0x0c,
    INTERRUPT_ENABLE        = 0x10
};

struct goldfish_int_state {
    struct goldfish_device dev;
    uint32_t level;
    uint32_t pending_count;
    uint32_t irq_enabled;
    uint32_t fiq_enabled;
    qemu_irq parent_irq;
    qemu_irq parent_fiq;
};

#define  GOLDFISH_INT_SAVE_VERSION  1

#define  QFIELD_STRUCT  struct goldfish_int_state
QFIELD_BEGIN(goldfish_int_fields)
    QFIELD_INT32(level),
    QFIELD_INT32(pending_count),
    QFIELD_INT32(irq_enabled),
    QFIELD_INT32(fiq_enabled),
QFIELD_END

static void goldfish_int_save(QEMUFile*  f, void*  opaque)
{
    struct goldfish_int_state*  s = opaque;

    qemu_put_struct(f, goldfish_int_fields, s);
}

static int  goldfish_int_load(QEMUFile*  f, void*  opaque, int  version_id)
{
    struct goldfish_int_state*  s = opaque;

    if (version_id != GOLDFISH_INT_SAVE_VERSION)
        return -1;

    return qemu_get_struct(f, goldfish_int_fields, s);
}

static void goldfish_int_update(struct goldfish_int_state *s)
{
    uint32_t flags;

    flags = (s->level & s->irq_enabled);
    qemu_set_irq(s->parent_irq, flags != 0);

    flags = (s->level & s->fiq_enabled);
    qemu_set_irq(s->parent_fiq, flags != 0);
}

static void goldfish_int_set_irq(void *opaque, int irq, int level)
{
    struct goldfish_int_state *s = (struct goldfish_int_state *)opaque;
    uint32_t mask = (1U << irq);

    if(level) {
        if(!(s->level & mask)) {
            if(s->irq_enabled & mask)
                s->pending_count++;
            s->level |= mask;
        }
    }
    else {
        if(s->level & mask) {
            if(s->irq_enabled & mask)
                s->pending_count--;
            s->level &= ~mask;
        }
    }
    goldfish_int_update(s);
}

static uint32_t goldfish_int_read(void *opaque, target_phys_addr_t offset)
{
    struct goldfish_int_state *s = (struct goldfish_int_state *)opaque;

    switch (offset) {
    case INTERRUPT_STATUS: /* IRQ_STATUS */
        return s->pending_count;
    case INTERRUPT_NUMBER: {
        int i;
        uint32_t pending = s->level & s->irq_enabled;
        for(i = 0; i < 32; i++) {
            if(pending & (1U << i))
                return i;
        }
        return 0;
    }
    default:
        cpu_abort (cpu_single_env, "goldfish_int_read: Bad offset %x\n", offset);
        return 0;
    }
}

static void goldfish_int_write(void *opaque, target_phys_addr_t offset, uint32_t value)
{
    struct goldfish_int_state *s = (struct goldfish_int_state *)opaque;
    uint32_t mask = (1U << value);

    switch (offset) {
        case INTERRUPT_DISABLE_ALL:
            s->pending_count = 0;
            s->level = 0;
            break;

        case INTERRUPT_DISABLE:
            if(s->irq_enabled & mask) {
                if(s->level & mask)
                    s->pending_count--;
                s->irq_enabled &= ~mask;
            }
            break;
        case INTERRUPT_ENABLE:
            if(!(s->irq_enabled & mask)) {
                s->irq_enabled |= mask;
                if(s->level & mask)
                    s->pending_count++;
            }
            break;

    default:
        cpu_abort (cpu_single_env, "goldfish_int_write: Bad offset %x\n", offset);
        return;
    }
    goldfish_int_update(s);
}

static CPUReadMemoryFunc *goldfish_int_readfn[] = {
    goldfish_int_read,
    goldfish_int_read,
    goldfish_int_read
};

static CPUWriteMemoryFunc *goldfish_int_writefn[] = {
    goldfish_int_write,
    goldfish_int_write,
    goldfish_int_write
};

qemu_irq*  goldfish_interrupt_init(uint32_t base, qemu_irq parent_irq, qemu_irq parent_fiq)
{
    int ret;
    struct goldfish_int_state *s;
    qemu_irq*  qi;

    s = qemu_mallocz(sizeof(*s));
    qi = qemu_allocate_irqs(goldfish_int_set_irq, s, GFD_MAX_IRQ);
    s->dev.name = "goldfish_interrupt_controller";
    s->dev.id = -1;
    s->dev.base = base;
    s->dev.size = 0x1000;
    s->parent_irq = parent_irq;
    s->parent_fiq = parent_fiq;

    ret = goldfish_device_add(&s->dev, goldfish_int_readfn, goldfish_int_writefn, s);
    if(ret) {
        qemu_free(s);
        return NULL;
    }

    register_savevm( "goldfish_int", 0, GOLDFISH_INT_SAVE_VERSION,
                     goldfish_int_save, goldfish_int_load, s);

    return qi;
}

