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
#ifndef GOLDFISH_DEVICE_H
#define GOLDFISH_DEVICE_H

struct goldfish_device {
    struct goldfish_device *next;
    struct goldfish_device *prev;
    uint32_t reported_state;
    void *cookie;
    const char *name;
    uint32_t id;
    uint32_t base; // filled in by goldfish_device_add if 0
    uint32_t size;
    uint32_t irq; // filled in by goldfish_device_add if 0
    uint32_t irq_count;
};


void goldfish_device_set_irq(struct goldfish_device *dev, int irq, int level);
int goldfish_device_add(struct goldfish_device *dev,
                       CPUReadMemoryFunc **mem_read,
                       CPUWriteMemoryFunc **mem_write,
                       void *opaque);

int goldfish_add_device_no_io(struct goldfish_device *dev);

void goldfish_device_init(qemu_irq *pic, uint32_t base, uint32_t size, uint32_t irq, uint32_t irq_count);
int goldfish_device_bus_init(uint32_t base, uint32_t irq);

// device init functions:
qemu_irq *goldfish_interrupt_init(uint32_t base, qemu_irq parent_irq, qemu_irq parent_fiq);
void goldfish_timer_and_rtc_init(uint32_t timerbase, int timerirq);
int goldfish_tty_add(CharDriverState *cs, int id, uint32_t base, int irq);
void goldfish_fb_init(int id);
void goldfish_audio_init(uint32_t base, int id, const char* input_source);
void goldfish_battery_init();
void goldfish_battery_set_prop(int ac, int property, int value);
void goldfish_battery_display(void (* callback)(void *data, const char* string), void *data);
void goldfish_mmc_init(uint32_t base, int id, BlockDriverState* bs);
void *goldfish_switch_add(char *name, uint32_t (*writefn)(void *opaque, uint32_t state), void *writeopaque, int id);
void goldfish_switch_set_state(void *opaque, uint32_t state);

// these do not add a device
void trace_dev_init();
void events_dev_init(uint32_t base, qemu_irq irq);
void nand_dev_init(uint32_t base);

#ifdef TARGET_I386
/* Maximum IRQ number available for a device on x86. */
#define GFD_MAX_IRQ      16
/* IRQ reserved for keyboard. */
#define GFD_KBD_IRQ      1
/* IRQ reserved for mouse. */
#define GFD_MOUSE_IRQ    12
/* IRQ reserved for error (raising an exception in TB code). */
#define GFD_ERR_IRQ      13
#else
/* Maximum IRQ number available for a device on ARM. */
#define GFD_MAX_IRQ     32
#endif

#endif  /* GOLDFISH_DEVICE_H */
