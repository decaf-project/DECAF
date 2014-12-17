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
#include "qemu-common.h"
#include "qemu-timer.h"
#include "cpu.h"
#include "arm_pic.h"
#include "goldfish_device.h"
#include "hw/hw.h"

enum {
    TIMER_TIME_LOW          = 0x00, // get low bits of current time and update TIMER_TIME_HIGH
    TIMER_TIME_HIGH         = 0x04, // get high bits of time at last TIMER_TIME_LOW read
    TIMER_ALARM_LOW         = 0x08, // set low bits of alarm and activate it
    TIMER_ALARM_HIGH        = 0x0c, // set high bits of next alarm
    TIMER_CLEAR_INTERRUPT   = 0x10,
    TIMER_CLEAR_ALARM       = 0x14
};

struct timer_state {
    struct goldfish_device dev;
    uint32_t alarm_low_ns;
    int32_t alarm_high_ns;
    int64_t now_ns;
    int     armed;
    QEMUTimer *timer;
};

#define  GOLDFISH_TIMER_SAVE_VERSION  1

static void  goldfish_timer_save(QEMUFile*  f, void*  opaque)
{
    struct timer_state*  s   = opaque;

    qemu_put_be64(f, s->now_ns);  /* in case the kernel is in the middle of a timer read */
    qemu_put_byte(f, s->armed);
    if (s->armed) {
        int64_t  now_ns   = qemu_get_clock_ns(vm_clock);
        int64_t  alarm_ns = (s->alarm_low_ns | (int64_t)s->alarm_high_ns << 32);
        qemu_put_be64(f, alarm_ns - now_ns);
    }
}

static int  goldfish_timer_load(QEMUFile*  f, void*  opaque, int  version_id)
{
    struct timer_state*  s   = opaque;

    if (version_id != GOLDFISH_TIMER_SAVE_VERSION)
        return -1;

    s->now_ns = qemu_get_be64(f);
    s->armed  = qemu_get_byte(f);
    if (s->armed) {
        int64_t  now_tks   = qemu_get_clock(vm_clock);
        int64_t  diff_tks  = qemu_get_be64(f);
        int64_t  alarm_tks = now_tks + diff_tks;

        if (alarm_tks <= now_tks) {
            goldfish_device_set_irq(&s->dev, 0, 1);
            s->armed = 0;
        } else {
            qemu_mod_timer(s->timer, alarm_tks);
        }
    }
    return 0;
}

static uint32_t goldfish_timer_read(void *opaque, target_phys_addr_t offset)
{
    struct timer_state *s = (struct timer_state *)opaque;
    switch(offset) {
        case TIMER_TIME_LOW:
            s->now_ns = qemu_get_clock_ns(vm_clock);
            return s->now_ns;
        case TIMER_TIME_HIGH:
            return s->now_ns >> 32;
        default:
            cpu_abort (cpu_single_env, "goldfish_timer_read: Bad offset %x\n", offset);
            return 0;
    }
}

static void goldfish_timer_write(void *opaque, target_phys_addr_t offset, uint32_t value_ns)
{
    struct timer_state *s = (struct timer_state *)opaque;
    int64_t alarm_ns, now_ns;
    switch(offset) {
        case TIMER_ALARM_LOW:
            s->alarm_low_ns = value_ns;
            alarm_ns = (s->alarm_low_ns | (int64_t)s->alarm_high_ns << 32);
            now_ns   = qemu_get_clock_ns(vm_clock);
            if (alarm_ns <= now_ns) {
                goldfish_device_set_irq(&s->dev, 0, 1);
            } else {
                qemu_mod_timer(s->timer, alarm_ns);
                s->armed = 1;
            }
            break;
        case TIMER_ALARM_HIGH:
            s->alarm_high_ns = value_ns;
            break;
        case TIMER_CLEAR_ALARM:
            qemu_del_timer(s->timer);
            s->armed = 0;
            /* fall through */
        case TIMER_CLEAR_INTERRUPT:
            goldfish_device_set_irq(&s->dev, 0, 0);
            break;
        default:
            cpu_abort (cpu_single_env, "goldfish_timer_write: Bad offset %x\n", offset);
    }
}

static void goldfish_timer_tick(void *opaque)
{
    struct timer_state *s = (struct timer_state *)opaque;

    s->armed = 0;
    goldfish_device_set_irq(&s->dev, 0, 1);
}

struct rtc_state {
    struct goldfish_device dev;
    uint32_t alarm_low;
    int32_t alarm_high;
    int64_t now;
};

/* we save the RTC for the case where the kernel is in the middle of a rtc_read
 * (i.e. it has read the low 32-bit of s->now, but not the high 32-bits yet */
#define  GOLDFISH_RTC_SAVE_VERSION  1

static void  goldfish_rtc_save(QEMUFile*  f, void*  opaque)
{
    struct rtc_state*  s = opaque;

    qemu_put_be64(f, s->now);
}

static int  goldfish_rtc_load(QEMUFile*  f, void*  opaque, int  version_id)
{
    struct  rtc_state*  s = opaque;

    if (version_id != GOLDFISH_RTC_SAVE_VERSION)
        return -1;

    /* this is an old value that is not correct. but that's ok anyway */
    s->now = qemu_get_be64(f);
    return 0;
}

static uint32_t goldfish_rtc_read(void *opaque, target_phys_addr_t offset)
{
    struct rtc_state *s = (struct rtc_state *)opaque;
    switch(offset) {
        case 0x0:
            s->now = (int64_t)time(NULL) * 1000000000;
            return s->now;
        case 0x4:
            return s->now >> 32;
        default:
            cpu_abort (cpu_single_env, "goldfish_rtc_read: Bad offset %x\n", offset);
            return 0;
    }
}

static void goldfish_rtc_write(void *opaque, target_phys_addr_t offset, uint32_t value)
{
    struct rtc_state *s = (struct rtc_state *)opaque;
    int64_t alarm;
    switch(offset) {
        case 0x8:
            s->alarm_low = value;
            alarm = s->alarm_low | (int64_t)s->alarm_high << 32;
            //printf("next alarm at %lld, tps %lld\n", alarm, ticks_per_sec);
            //qemu_mod_timer(s->timer, alarm);
            break;
        case 0xc:
            s->alarm_high = value;
            //printf("alarm_high %d\n", s->alarm_high);
            break;
        case 0x10:
            goldfish_device_set_irq(&s->dev, 0, 0);
            break;
        default:
            cpu_abort (cpu_single_env, "goldfish_rtc_write: Bad offset %x\n", offset);
    }
}

static struct timer_state timer_state = {
    .dev = {
        .name = "goldfish_timer",
        .id = -1,
        .size = 0x1000,
        .irq_count = 1,
    }
};

static struct timer_state rtc_state = {
    .dev = {
        .name = "goldfish_rtc",
        .id = -1,
        .size = 0x1000,
        .irq_count = 1,
    }
};

static CPUReadMemoryFunc *goldfish_timer_readfn[] = {
    goldfish_timer_read,
    goldfish_timer_read,
    goldfish_timer_read
};

static CPUWriteMemoryFunc *goldfish_timer_writefn[] = {
    goldfish_timer_write,
    goldfish_timer_write,
    goldfish_timer_write
};

static CPUReadMemoryFunc *goldfish_rtc_readfn[] = {
    goldfish_rtc_read,
    goldfish_rtc_read,
    goldfish_rtc_read
};

static CPUWriteMemoryFunc *goldfish_rtc_writefn[] = {
    goldfish_rtc_write,
    goldfish_rtc_write,
    goldfish_rtc_write
};

void goldfish_timer_and_rtc_init(uint32_t timerbase, int timerirq)
{
    timer_state.dev.base = timerbase;
    timer_state.dev.irq = timerirq;
    timer_state.timer = qemu_new_timer_ns(vm_clock, goldfish_timer_tick, &timer_state);
    goldfish_device_add(&timer_state.dev, goldfish_timer_readfn, goldfish_timer_writefn, &timer_state);
    register_savevm( "goldfish_timer", 0, GOLDFISH_TIMER_SAVE_VERSION,
                     goldfish_timer_save, goldfish_timer_load, &timer_state);

    goldfish_device_add(&rtc_state.dev, goldfish_rtc_readfn, goldfish_rtc_writefn, &rtc_state);
    register_savevm( "goldfish_rtc", 0, GOLDFISH_RTC_SAVE_VERSION,
                     goldfish_rtc_save, goldfish_rtc_load, &rtc_state);
}

