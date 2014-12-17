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
#include "power_supply.h"


enum {
	/* status register */
	BATTERY_INT_STATUS	    = 0x00,
	/* set this to enable IRQ */
	BATTERY_INT_ENABLE	    = 0x04,

	BATTERY_AC_ONLINE       = 0x08,
	BATTERY_STATUS          = 0x0C,
	BATTERY_HEALTH          = 0x10,
	BATTERY_PRESENT         = 0x14,
	BATTERY_CAPACITY        = 0x18,

	BATTERY_STATUS_CHANGED	= 1U << 0,
	AC_STATUS_CHANGED   	= 1U << 1,
	BATTERY_INT_MASK        = BATTERY_STATUS_CHANGED | AC_STATUS_CHANGED,
};


struct goldfish_battery_state {
    struct goldfish_device dev;
    // IRQs
    uint32_t int_status;
    // irq enable mask for int_status
    uint32_t int_enable;

    int ac_online;
    int status;
    int health;
    int present;
    int capacity;
};

/* update this each time you update the battery_state struct */
#define  BATTERY_STATE_SAVE_VERSION  1

#define  QFIELD_STRUCT  struct goldfish_battery_state
QFIELD_BEGIN(goldfish_battery_fields)
    QFIELD_INT32(int_status),
    QFIELD_INT32(int_enable),
    QFIELD_INT32(ac_online),
    QFIELD_INT32(status),
    QFIELD_INT32(health),
    QFIELD_INT32(present),
    QFIELD_INT32(capacity),
QFIELD_END

static void  goldfish_battery_save(QEMUFile*  f, void* opaque)
{
    struct goldfish_battery_state*  s = opaque;

    qemu_put_struct(f, goldfish_battery_fields, s);
}

static int   goldfish_battery_load(QEMUFile*  f, void*  opaque, int  version_id)
{
    struct goldfish_battery_state*  s = opaque;

    if (version_id != BATTERY_STATE_SAVE_VERSION)
        return -1;

    return qemu_get_struct(f, goldfish_battery_fields, s);
}

static struct goldfish_battery_state *battery_state;

static uint32_t goldfish_battery_read(void *opaque, target_phys_addr_t offset)
{
    uint32_t ret;
    struct goldfish_battery_state *s = opaque;

    switch(offset) {
        case BATTERY_INT_STATUS:
            // return current buffer status flags
            ret = s->int_status & s->int_enable;
            if (ret) {
                goldfish_device_set_irq(&s->dev, 0, 0);
                s->int_status = 0;
            }
            return ret;

		case BATTERY_INT_ENABLE:
		    return s->int_enable;
		case BATTERY_AC_ONLINE:
		    return s->ac_online;
		case BATTERY_STATUS:
		    return s->status;
		case BATTERY_HEALTH:
		    return s->health;
		case BATTERY_PRESENT:
		    return s->present;
		case BATTERY_CAPACITY:
		    return s->capacity;

        default:
            cpu_abort (cpu_single_env, "goldfish_battery_read: Bad offset %x\n", offset);
            return 0;
    }
}

static void goldfish_battery_write(void *opaque, target_phys_addr_t offset, uint32_t val)
{
    struct goldfish_battery_state *s = opaque;

    switch(offset) {
        case BATTERY_INT_ENABLE:
            /* enable interrupts */
            s->int_enable = val;
//            s->int_status = (AUDIO_INT_WRITE_BUFFER_1_EMPTY | AUDIO_INT_WRITE_BUFFER_2_EMPTY);
//            goldfish_device_set_irq(&s->dev, 0, (s->int_status & s->int_enable));
            break;

        default:
            cpu_abort (cpu_single_env, "goldfish_audio_write: Bad offset %x\n", offset);
    }
}

static CPUReadMemoryFunc *goldfish_battery_readfn[] = {
    goldfish_battery_read,
    goldfish_battery_read,
    goldfish_battery_read
};


static CPUWriteMemoryFunc *goldfish_battery_writefn[] = {
    goldfish_battery_write,
    goldfish_battery_write,
    goldfish_battery_write
};

void goldfish_battery_init()
{
    struct goldfish_battery_state *s;

    s = (struct goldfish_battery_state *)qemu_mallocz(sizeof(*s));
    s->dev.name = "goldfish-battery";
    s->dev.base = 0;    // will be allocated dynamically
    s->dev.size = 0x1000;
    s->dev.irq_count = 1;

    // default values for the battery
    s->ac_online = 1;
    s->status = POWER_SUPPLY_STATUS_CHARGING;
    s->health = POWER_SUPPLY_HEALTH_GOOD;
    s->present = 1;     // battery is present
    s->capacity = 50;   // 50% charged

    battery_state = s;

    goldfish_device_add(&s->dev, goldfish_battery_readfn, goldfish_battery_writefn, s);

    register_savevm( "battery_state", 0, BATTERY_STATE_SAVE_VERSION,
                     goldfish_battery_save, goldfish_battery_load, s);
}

void goldfish_battery_set_prop(int ac, int property, int value)
{
    int new_status = (ac ? AC_STATUS_CHANGED : BATTERY_STATUS_CHANGED);

    if (ac) {
        switch (property) {
            case POWER_SUPPLY_PROP_ONLINE:
                battery_state->ac_online = value;
                break;
        }
    } else {
         switch (property) {
            case POWER_SUPPLY_PROP_STATUS:
                battery_state->status = value;
                break;
            case POWER_SUPPLY_PROP_HEALTH:
                battery_state->health = value;
                break;
            case POWER_SUPPLY_PROP_PRESENT:
                battery_state->present = value;
                break;
            case POWER_SUPPLY_PROP_CAPACITY:
                battery_state->capacity = value;
                break;
        }
    }

    if (new_status != battery_state->int_status) {
        battery_state->int_status |= new_status;
        goldfish_device_set_irq(&battery_state->dev, 0, (battery_state->int_status & battery_state->int_enable));
    }
}

void goldfish_battery_display(void (* callback)(void *data, const char* string), void *data)
{
    char          buffer[100];
    const char*   value;

    sprintf(buffer, "AC: %s\r\n", (battery_state->ac_online ? "online" : "offline"));
    callback(data, buffer);

    switch (battery_state->status) {
	    case POWER_SUPPLY_STATUS_CHARGING:
	        value = "Charging";
	        break;
	    case POWER_SUPPLY_STATUS_DISCHARGING:
	        value = "Discharging";
	        break;
	    case POWER_SUPPLY_STATUS_NOT_CHARGING:
	        value = "Not charging";
	        break;
	    case POWER_SUPPLY_STATUS_FULL:
	        value = "Full";
	        break;
        default:
	        value = "Unknown";
	        break;
    }
    sprintf(buffer, "status: %s\r\n", value);
    callback(data, buffer);

    switch (battery_state->health) {
	    case POWER_SUPPLY_HEALTH_GOOD:
	        value = "Good";
	        break;
	    case POWER_SUPPLY_HEALTH_OVERHEAT:
	        value = "Overhead";
	        break;
	    case POWER_SUPPLY_HEALTH_DEAD:
	        value = "Dead";
	        break;
	    case POWER_SUPPLY_HEALTH_OVERVOLTAGE:
	        value = "Overvoltage";
	        break;
	    case POWER_SUPPLY_HEALTH_UNSPEC_FAILURE:
	        value = "Unspecified failure";
	        break;
        default:
	        value = "Unknown";
	        break;
    }
    sprintf(buffer, "health: %s\r\n", value);
    callback(data, buffer);

    sprintf(buffer, "present: %s\r\n", (battery_state->present ? "true" : "false"));
    callback(data, buffer);

    sprintf(buffer, "capacity: %d\r\n", battery_state->capacity);
    callback(data, buffer);
}
