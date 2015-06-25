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
#include "android/hw-events.h"
#include "android/charmap.h"
#include "android/globals.h"  /* for android_hw */
#include "android/multitouch-screen.h"
#include "irq.h"
#include "user-events.h"
#include "console.h"

#define MAX_EVENTS 256*4

enum {
    REG_READ        = 0x00,
    REG_SET_PAGE    = 0x00,
    REG_LEN         = 0x04,
    REG_DATA        = 0x08,

    PAGE_NAME       = 0x00000,
    PAGE_EVBITS     = 0x10000,
    PAGE_ABSDATA    = 0x20000 | EV_ABS,
};

/* These corresponds to the state of the driver.
 * Unfortunately, we have to buffer events coming
 * from the UI, since the kernel driver is not
 * capable of receiving them until XXXXXX
 */
enum {
    STATE_INIT = 0,  /* The device is initialized */
    STATE_BUFFERED,  /* Events have been buffered, but no IRQ raised yet */
    STATE_LIVE       /* Events can be sent directly to the kernel */
};

/* NOTE: The ev_bits arrays are used to indicate to the kernel
 *       which events can be sent by the emulated hardware.
 */

typedef struct
{
    uint32_t base;
    qemu_irq  irq;
    int pending;
    int page;

    unsigned events[MAX_EVENTS];
    unsigned first;
    unsigned last;
    unsigned state;

    const char *name;

    struct {
        size_t   len;
        uint8_t *bits;
    } ev_bits[EV_MAX + 1];

    int32_t *abs_info;
    size_t abs_info_count;
} events_state;

/* An entry in the array of ABS_XXX values */
typedef struct ABSEntry {
    /* Minimum ABS_XXX value. */
    uint32_t    min;
    /* Maximum ABS_XXX value. */
    uint32_t    max;
    /* 'fuzz;, and 'flat' ABS_XXX values are always zero here. */
    uint32_t    fuzz;
    uint32_t    flat;
} ABSEntry;


/* modify this each time you change the events_device structure. you
 * will also need to upadte events_state_load and events_state_save
 */
#define  EVENTS_STATE_SAVE_VERSION  2

#undef  QFIELD_STRUCT
#define QFIELD_STRUCT  events_state

QFIELD_BEGIN(events_state_fields)
    QFIELD_INT32(pending),
    QFIELD_INT32(page),
    QFIELD_BUFFER(events),
    QFIELD_INT32(first),
    QFIELD_INT32(last),
    QFIELD_INT32(state),
QFIELD_END

static void  events_state_save(QEMUFile*  f, void*  opaque)
{
    events_state*  s = opaque;

    qemu_put_struct(f, events_state_fields, s);
}

static int  events_state_load(QEMUFile*  f, void* opaque, int  version_id)
{
    events_state*  s = opaque;

    if (version_id != EVENTS_STATE_SAVE_VERSION)
        return -1;

    return qemu_get_struct(f, events_state_fields, s);
}

static void enqueue_event(events_state *s, unsigned int type, unsigned int code, int value)
{
    int  enqueued = s->last - s->first;

    if (enqueued < 0)
        enqueued += MAX_EVENTS;

    if (enqueued + 3 > MAX_EVENTS) {
        fprintf(stderr, "##KBD: Full queue, lose event\n");
        return;
    }

    if(s->first == s->last) {
	if (s->state == STATE_LIVE)
	  qemu_irq_raise(s->irq);
	else {
	  s->state = STATE_BUFFERED;
	}
    }

    //fprintf(stderr, "##KBD: type=%d code=%d value=%d\n", type, code, value);

    s->events[s->last] = type;
    s->last = (s->last + 1) & (MAX_EVENTS-1);
    s->events[s->last] = code;
    s->last = (s->last + 1) & (MAX_EVENTS-1);
    s->events[s->last] = value;
    s->last = (s->last + 1) & (MAX_EVENTS-1);
}

static unsigned dequeue_event(events_state *s)
{
    unsigned n;

    if(s->first == s->last) {
        return 0;
    }

    n = s->events[s->first];

    s->first = (s->first + 1) & (MAX_EVENTS - 1);

    if(s->first == s->last) {
        qemu_irq_lower(s->irq);
    }
#ifdef TARGET_I386
    /*
     * Adding the logic to handle edge-triggered interrupts for x86
     * because the exisiting goldfish events device basically provides
     * level-trigger interrupts only.
     *
     * Logic: When an event (including the type/code/value) is fetched
     * by the driver, if there is still another event in the event
     * queue, the goldfish event device will re-assert the IRQ so that
     * the driver can be notified to fetch the event again.
     */
    else if (((s->first + 2) & (MAX_EVENTS - 1)) < s->last ||
               (s->first & (MAX_EVENTS - 1)) > s->last) { /* if there still is an event */
        qemu_irq_lower(s->irq);
        qemu_irq_raise(s->irq);
    }
#endif
    return n;
}

static int get_page_len(events_state *s)
{
    int page = s->page;
    if (page == PAGE_NAME) {
        const char* name = s->name;
        return strlen(name);
    } if (page >= PAGE_EVBITS && page <= PAGE_EVBITS + EV_MAX)
        return s->ev_bits[page - PAGE_EVBITS].len;
    if (page == PAGE_ABSDATA)
        return s->abs_info_count * sizeof(s->abs_info[0]);
    return 0;
}

static int get_page_data(events_state *s, int offset)
{
    int page_len = get_page_len(s);
    int page = s->page;
    if (offset > page_len)
        return 0;
    if (page == PAGE_NAME) {
        const char* name = s->name;
        return name[offset];
    } if (page >= PAGE_EVBITS && page <= PAGE_EVBITS + EV_MAX)
        return s->ev_bits[page - PAGE_EVBITS].bits[offset];
    if (page == PAGE_ABSDATA) {
        return s->abs_info[offset / sizeof(s->abs_info[0])];
    }
    return 0;
}

static uint32_t events_read(void *x, target_phys_addr_t off)
{
    events_state *s = (events_state *) x;
    int offset = off; // - s->base;

    /* This gross hack below is used to ensure that we
     * only raise the IRQ when the kernel driver is
     * properly ready! If done before this, the driver
     * becomes confused and ignores all input events
     * as soon as one was buffered!
     */
    if (offset == REG_LEN && s->page == PAGE_ABSDATA) {
	if (s->state == STATE_BUFFERED)
	  qemu_irq_raise(s->irq);
	s->state = STATE_LIVE;
    }

    if (offset == REG_READ)
        return dequeue_event(s);
    else if (offset == REG_LEN)
        return get_page_len(s);
    else if (offset >= REG_DATA)
        return get_page_data(s, offset - REG_DATA);
    return 0; // this shouldn't happen, if the driver does the right thing
}

static void events_write(void *x, target_phys_addr_t off, uint32_t val)
{
    events_state *s = (events_state *) x;
    int offset = off; // - s->base;
    if (offset == REG_SET_PAGE)
        s->page = val;
}

static CPUReadMemoryFunc *events_readfn[] = {
   events_read,
   events_read,
   events_read
};

static CPUWriteMemoryFunc *events_writefn[] = {
   events_write,
   events_write,
   events_write
};

static void events_put_keycode(void *x, int keycode)
{
    events_state *s = (events_state *) x;

    enqueue_event(s, EV_KEY, keycode&0x1ff, (keycode&0x200) ? 1 : 0);
}

static void events_put_mouse(void *opaque, int dx, int dy, int dz, int buttons_state)
{
    events_state *s = (events_state *) opaque;
    /* in the Android emulator, we use dz == 0 for touchscreen events,
     * and dz == 1 for trackball events. See the kbd_mouse_event calls
     * in android/skin/trackball.c and android/skin/window.c
     */
    if (dz == 0) {
        if (androidHwConfig_isScreenMultiTouch(android_hw)) {
            /* Convert mouse event into multi-touch event */
            multitouch_update_pointer(MTES_MOUSE, 0, dx, dy,
                                      (buttons_state & 1) ? 0x81 : 0);
        } else if (androidHwConfig_isScreenTouch(android_hw)) {
            enqueue_event(s, EV_ABS, ABS_X, dx);
            enqueue_event(s, EV_ABS, ABS_Y, dy);
            enqueue_event(s, EV_ABS, ABS_Z, dz);
            enqueue_event(s, EV_KEY, BTN_TOUCH, buttons_state&1);
            enqueue_event(s, EV_SYN, 0, 0);
        }
    } else {
        enqueue_event(s, EV_REL, REL_X, dx);
        enqueue_event(s, EV_REL, REL_Y, dy);
        enqueue_event(s, EV_SYN, 0, 0);
    }
}

static void  events_put_generic(void*  opaque, int  type, int  code, int  value)
{
    events_state *s = (events_state *) opaque;

    enqueue_event(s, type, code, value);
}

/* set bits [bitl..bith] in the ev_bits[type] array
 */
static void
events_set_bits(events_state *s, int type, int bitl, int bith)
{
    uint8_t *bits;
    uint8_t maskl, maskh;
    int il, ih;
    il = bitl / 8;
    ih = bith / 8;
    if (ih >= s->ev_bits[type].len) {
        bits = qemu_mallocz(ih + 1);
        if (bits == NULL)
            return;
        memcpy(bits, s->ev_bits[type].bits, s->ev_bits[type].len);
        qemu_free(s->ev_bits[type].bits);
        s->ev_bits[type].bits = bits;
        s->ev_bits[type].len = ih + 1;
    }
    else
        bits = s->ev_bits[type].bits;
    maskl = 0xffU << (bitl & 7);
    maskh = 0xffU >> (7 - (bith & 7));
    if (il >= ih)
        maskh &= maskl;
    else {
        bits[il] |= maskl;
        while (++il < ih)
            bits[il] = 0xff;
    }
    bits[ih] |= maskh;
}

static void
events_set_bit(events_state* s, int  type, int  bit)
{
    events_set_bits(s, type, bit, bit);
}

static void
events_clr_bit(events_state* s, int type, int bit)
{
    int ii = bit / 8;
    if (ii < s->ev_bits[type].len) {
        uint8_t* bits = s->ev_bits[type].bits;
        uint8_t  mask = 0x01U << (bit & 7);
        bits[ii] &= ~mask;
    }
}

void events_dev_init(uint32_t base, qemu_irq irq)
{
    events_state *s;
    int iomemtype;
    AndroidHwConfig*  config = android_hw;

    s = (events_state *) qemu_mallocz(sizeof(events_state));

    /* now set the events capability bits depending on hardware configuration */
    /* apparently, the EV_SYN array is used to indicate which other
     * event classes to consider.
     */

    /* configure EV_KEY array
     *
     * All Android devices must have the following keys:
     *   KEY_HOME, KEY_BACK, KEY_SEND (Call), KEY_END (EndCall),
     *   KEY_SOFT1 (Menu), VOLUME_UP, VOLUME_DOWN
     *
     *   Note that previous models also had a KEY_SOFT2,
     *   and a KEY_POWER  which we still support here.
     *
     *   Newer models have a KEY_SEARCH key, which we always
     *   enable here.
     *
     * A Dpad will send: KEY_DOWN / UP / LEFT / RIGHT / CENTER
     *
     * The KEY_CAMERA button isn't very useful if there is no camera.
     *
     * BTN_MOUSE is sent when the trackball is pressed
     * BTN_TOUCH is sent when the touchscreen is pressed
     */
    events_set_bit (s, EV_SYN, EV_KEY );

    events_set_bit(s, EV_KEY, KEY_HOME);
    events_set_bit(s, EV_KEY, KEY_BACK);
    events_set_bit(s, EV_KEY, KEY_SEND);
    events_set_bit(s, EV_KEY, KEY_END);
    events_set_bit(s, EV_KEY, KEY_SOFT1);
    events_set_bit(s, EV_KEY, KEY_VOLUMEUP);
    events_set_bit(s, EV_KEY, KEY_VOLUMEDOWN);
    events_set_bit(s, EV_KEY, KEY_SOFT2);
    events_set_bit(s, EV_KEY, KEY_POWER);
    events_set_bit(s, EV_KEY, KEY_SEARCH);

    if (config->hw_dPad) {
        events_set_bit(s, EV_KEY, KEY_DOWN);
        events_set_bit(s, EV_KEY, KEY_UP);
        events_set_bit(s, EV_KEY, KEY_LEFT);
        events_set_bit(s, EV_KEY, KEY_RIGHT);
        events_set_bit(s, EV_KEY, KEY_CENTER);
    }

    if (config->hw_trackBall) {
        events_set_bit(s, EV_KEY, BTN_MOUSE);
    }
    if (androidHwConfig_isScreenTouch(config)) {
        events_set_bit(s, EV_KEY, BTN_TOUCH);
    }

    if (strcmp(config->hw_camera_back, "none") ||
        strcmp(config->hw_camera_front, "none")) {
        /* Camera emulation is enabled. */
        events_set_bit(s, EV_KEY, KEY_CAMERA);
    }

    if (config->hw_keyboard) {
        /* since we want to implement Unicode reverse-mapping
         * allow any kind of key, even those not available on
         * the skin.
         *
         * the previous code did set the [1..0x1ff] range, but
         * we don't want to enable certain bits in the middle
         * of the range that are registered for mouse/trackball/joystick
         * events.
         *
         * see "linux_keycodes.h" for the list of events codes.
         */
        events_set_bits(s, EV_KEY, 1, 0xff);
        events_set_bits(s, EV_KEY, 0x160, 0x1ff);

        /* If there is a keyboard, but no DPad, we need to clear the
         * corresponding bits. Doing this is simpler than trying to exclude
         * the DPad values from the ranges above.
         */
        if (!config->hw_dPad) {
            events_clr_bit(s, EV_KEY, KEY_DOWN);
            events_clr_bit(s, EV_KEY, KEY_UP);
            events_clr_bit(s, EV_KEY, KEY_LEFT);
            events_clr_bit(s, EV_KEY, KEY_RIGHT);
            events_clr_bit(s, EV_KEY, KEY_CENTER);
        }
    }

    /* configure EV_REL array
     *
     * EV_REL events are sent when the trackball is moved
     */
    if (config->hw_trackBall) {
        events_set_bit (s, EV_SYN, EV_REL );
        events_set_bits(s, EV_REL, REL_X, REL_Y);
    }

    /* configure EV_ABS array.
     *
     * EV_ABS events are sent when the touchscreen is pressed
     */
    if (!androidHwConfig_isScreenNoTouch(config)) {
        ABSEntry* abs_values;

        events_set_bit (s, EV_SYN, EV_ABS );
        events_set_bits(s, EV_ABS, ABS_X, ABS_Z);
        /* Allocate the absinfo to report the min/max bounds for each
         * absolute dimension. The array must contain 3, or ABS_MAX tuples
         * of (min,max,fuzz,flat) 32-bit values.
         *
         * min and max are the bounds
         * fuzz corresponds to the device's fuziness, we set it to 0
         * flat corresponds to the flat position for JOEYDEV devices,
         * we also set it to 0.
         *
         * There is no need to save/restore this array in a snapshot
         * since the values only depend on the hardware configuration.
         */
        s->abs_info_count = androidHwConfig_isScreenMultiTouch(config) ? ABS_MAX * 4 : 3 * 4;
        const int abs_size = sizeof(uint32_t) * s->abs_info_count;
        s->abs_info = malloc(abs_size);
        memset(s->abs_info, 0, abs_size);
        abs_values = (ABSEntry*)s->abs_info;

        abs_values[ABS_X].max = config->hw_lcd_width-1;
        abs_values[ABS_Y].max = config->hw_lcd_height-1;
        abs_values[ABS_Z].max = 1;

        if (androidHwConfig_isScreenMultiTouch(config)) {
            /*
             * Setup multitouch.
             */
            events_set_bit(s, EV_ABS, ABS_MT_SLOT);
            events_set_bit(s, EV_ABS, ABS_MT_POSITION_X);
            events_set_bit(s, EV_ABS, ABS_MT_POSITION_Y);
            events_set_bit(s, EV_ABS, ABS_MT_TRACKING_ID);
            events_set_bit(s, EV_ABS, ABS_MT_TOUCH_MAJOR);
            events_set_bit(s, EV_ABS, ABS_MT_PRESSURE);

            abs_values[ABS_MT_SLOT].max = multitouch_get_max_slot();
            abs_values[ABS_MT_TRACKING_ID].max = abs_values[ABS_MT_SLOT].max + 1;
            abs_values[ABS_MT_POSITION_X].max = abs_values[ABS_X].max;
            abs_values[ABS_MT_POSITION_Y].max = abs_values[ABS_Y].max;
            abs_values[ABS_MT_TOUCH_MAJOR].max = 0x7fffffff; // TODO: Make it less random
            abs_values[ABS_MT_PRESSURE].max = 0x100; // TODO: Make it less random
        }
    }

    /* configure EV_SW array
     *
     * EV_SW events are sent to indicate that the keyboard lid
     * was closed or opened (done when we switch layouts through
     * KP-7 or KP-9).
     *
     * We only support this when hw.keyboard.lid is true.
     */
    if (config->hw_keyboard && config->hw_keyboard_lid) {
        events_set_bit(s, EV_SYN, EV_SW);
        events_set_bit(s, EV_SW, 0);
    }

    iomemtype = cpu_register_io_memory(events_readfn, events_writefn, s);

    cpu_register_physical_memory(base, 0xfff, iomemtype);

    qemu_add_kbd_event_handler(events_put_keycode, s);
    qemu_add_mouse_event_handler(events_put_mouse, s, 1, "goldfish-events");

    s->base = base;
    s->irq = irq;

    s->first = 0;
    s->last = 0;
    s->state = STATE_INIT;
    s->name = qemu_strdup(config->hw_keyboard_charmap);

    /* This function migh fire buffered events to the device, so
     * ensure that it is called after initialization is complete
     */
    user_event_register_generic(s, events_put_generic);

    register_savevm( "events_state", 0, EVENTS_STATE_SAVE_VERSION,
                      events_state_save, events_state_load, s );
}
