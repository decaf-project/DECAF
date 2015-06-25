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
#include "goldfish_vmem.h"
#include "android/utils/debug.h"

#define PDEV_BUS_OP_DONE        (0x00)
#define PDEV_BUS_OP_REMOVE_DEV  (0x04)
#define PDEV_BUS_OP_ADD_DEV     (0x08)

#define PDEV_BUS_OP_INIT        (0x00)

#define PDEV_BUS_OP             (0x00)
#define PDEV_BUS_GET_NAME       (0x04)
#define PDEV_BUS_NAME_LEN       (0x08)
#define PDEV_BUS_ID             (0x0c)
#define PDEV_BUS_IO_BASE        (0x10)
#define PDEV_BUS_IO_SIZE        (0x14)
#define PDEV_BUS_IRQ            (0x18)
#define PDEV_BUS_IRQ_COUNT      (0x1c)

struct bus_state {
    struct goldfish_device dev;
    struct goldfish_device *current;
};

qemu_irq *goldfish_pic;
static struct goldfish_device *first_device;
static struct goldfish_device *last_device;
uint32_t goldfish_free_base;
uint32_t goldfish_free_irq;

void goldfish_device_set_irq(struct goldfish_device *dev, int irq, int level)
{
    if(irq >= dev->irq_count)
        cpu_abort (cpu_single_env, "goldfish_device_set_irq: Bad irq %d >= %d\n", irq, dev->irq_count);
    else
        qemu_set_irq(goldfish_pic[dev->irq + irq], level);
}

int goldfish_add_device_no_io(struct goldfish_device *dev)
{
    if(dev->base == 0) {
        dev->base = goldfish_free_base;
        goldfish_free_base += dev->size;
    }
    if(dev->irq == 0 && dev->irq_count > 0) {
        dev->irq = goldfish_free_irq;
        goldfish_free_irq += dev->irq_count;
#ifdef TARGET_I386
        /* Make sure that we pass by the reserved IRQs. */
        while (goldfish_free_irq == GFD_KBD_IRQ ||
               goldfish_free_irq == GFD_MOUSE_IRQ ||
               goldfish_free_irq == GFD_ERR_IRQ) {
            goldfish_free_irq++;
        }
#endif
        if (goldfish_free_irq >= GFD_MAX_IRQ) {
            derror("Goldfish device has exceeded available IRQ number.");
            exit(1);
        }
    }
    //printf("goldfish_add_device: %s, base %x %x, irq %d %d\n",
    //       dev->name, dev->base, dev->size, dev->irq, dev->irq_count);
    dev->next = NULL;
    if(last_device) {
        last_device->next = dev;
    }
    else {
        first_device = dev;
    }
    last_device = dev;
    return 0;
}

int goldfish_device_add(struct goldfish_device *dev,
                       CPUReadMemoryFunc **mem_read,
                       CPUWriteMemoryFunc **mem_write,
                       void *opaque)
{
    int iomemtype;
    goldfish_add_device_no_io(dev);
    iomemtype = cpu_register_io_memory(mem_read, mem_write, opaque);
    cpu_register_physical_memory(dev->base, dev->size, iomemtype);
    return 0;
}

static uint32_t goldfish_bus_read(void *opaque, target_phys_addr_t offset)
{
    struct bus_state *s = (struct bus_state *)opaque;

    switch (offset) {
        case PDEV_BUS_OP:
            if(s->current) {
                s->current->reported_state = 1;
                s->current = s->current->next;
            }
            else {
                s->current = first_device;
            }
            while(s->current && s->current->reported_state == 1)
                s->current = s->current->next;
            if(s->current)
                return PDEV_BUS_OP_ADD_DEV;
            else {
                goldfish_device_set_irq(&s->dev, 0, 0);
                return PDEV_BUS_OP_DONE;
            }

        case PDEV_BUS_NAME_LEN:
            return s->current ? strlen(s->current->name) : 0;
        case PDEV_BUS_ID:
            return s->current ? s->current->id : 0;
        case PDEV_BUS_IO_BASE:
            return s->current ? s->current->base : 0;
        case PDEV_BUS_IO_SIZE:
            return s->current ? s->current->size : 0;
        case PDEV_BUS_IRQ:
            return s->current ? s->current->irq : 0;
        case PDEV_BUS_IRQ_COUNT:
            return s->current ? s->current->irq_count : 0;
    default:
        cpu_abort (cpu_single_env, "goldfish_bus_read: Bad offset %x\n", offset);
        return 0;
    }
}

static void goldfish_bus_op_init(struct bus_state *s)
{
    struct goldfish_device *dev = first_device;
    while(dev) {
        dev->reported_state = 0;
        dev = dev->next;
    }
    s->current = NULL;
    goldfish_device_set_irq(&s->dev, 0, first_device != NULL);
}

static void goldfish_bus_write(void *opaque, target_phys_addr_t offset, uint32_t value)
{
    struct bus_state *s = (struct bus_state *)opaque;

    switch(offset) {
        case PDEV_BUS_OP:
            switch(value) {
                case PDEV_BUS_OP_INIT:
                    goldfish_bus_op_init(s);
                    break;
                default:
                    cpu_abort (cpu_single_env, "goldfish_bus_write: Bad PDEV_BUS_OP value %x\n", value);
            };
            break;
        case PDEV_BUS_GET_NAME:
            if(s->current) {
                safe_memory_rw_debug(cpu_single_env, value, (void*)s->current->name, strlen(s->current->name), 1);
            }
            break;
        default:
            cpu_abort (cpu_single_env, "goldfish_bus_write: Bad offset %x\n", offset);
    }
}

static CPUReadMemoryFunc *goldfish_bus_readfn[] = {
    goldfish_bus_read,
    goldfish_bus_read,
    goldfish_bus_read
};

static CPUWriteMemoryFunc *goldfish_bus_writefn[] = {
    goldfish_bus_write,
    goldfish_bus_write,
    goldfish_bus_write
};


static struct bus_state bus_state = {
    .dev = {
        .name = "goldfish_device_bus",
        .id = -1,
        .base = 0x10001000,
        .size = 0x1000,
        .irq = 1,
        .irq_count = 1,
    }
};

void goldfish_device_init(qemu_irq *pic, uint32_t base, uint32_t size, uint32_t irq, uint32_t irq_count)
{
    goldfish_pic = pic;
    goldfish_free_base = base;
    goldfish_free_irq = irq;
}

int goldfish_device_bus_init(uint32_t base, uint32_t irq)
{
    bus_state.dev.base = base;
    bus_state.dev.irq = irq;

    return goldfish_device_add(&bus_state.dev, goldfish_bus_readfn, goldfish_bus_writefn, &bus_state);
}

