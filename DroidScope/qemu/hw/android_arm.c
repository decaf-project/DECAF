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
#include "hw.h"
#include "boards.h"
#include "devices.h"
#include "net.h"
#include "arm_pic.h"
#include "sysemu.h"
#include "goldfish_device.h"
#include "android/globals.h"
#include "audio/audio.h"
#include "arm-misc.h"
#include "console.h"
#include "blockdev.h"
#include "goldfish_pipe.h"
#ifdef CONFIG_MEMCHECK
#include "memcheck/memcheck_api.h"
#endif  // CONFIG_MEMCHECK

#include "android/utils/debug.h"

#define  D(...)  VERBOSE_PRINT(init,__VA_ARGS__)

#define ARM_CPU_SAVE_VERSION  1

char* audio_input_source = NULL;

void goldfish_memlog_init(uint32_t base);

static struct goldfish_device event0_device = {
    .name = "goldfish_events",
    .id = 0,
    .size = 0x1000,
    .irq_count = 1
};

static struct goldfish_device nand_device = {
    .name = "goldfish_nand",
    .id = 0,
    .size = 0x1000
};

/* Board init.  */

#define TEST_SWITCH 1
#if TEST_SWITCH
uint32_t switch_test_write(void *opaque, uint32_t state)
{
    goldfish_switch_set_state(opaque, state);
    return state;
}
#endif
static void android_arm_init_(ram_addr_t ram_size,
    const char *boot_device,
    const char *kernel_filename,
    const char *kernel_cmdline,
    const char *initrd_filename,
    const char *cpu_model)
{
    CPUState *env;
    qemu_irq *cpu_pic;
    qemu_irq *goldfish_pic;
    int i;
    struct arm_boot_info  info;
    ram_addr_t ram_offset;

    if (!cpu_model)
        cpu_model = "arm926";

    env = cpu_init(cpu_model);
    register_savevm( "cpu", 0, ARM_CPU_SAVE_VERSION, cpu_save, cpu_load, env );

    ram_offset = qemu_ram_alloc(NULL,"android_arm",ram_size);
    cpu_register_physical_memory(0, ram_size, ram_offset | IO_MEM_RAM);

    cpu_pic = arm_pic_init_cpu(env);
    goldfish_pic = goldfish_interrupt_init(0xff000000, cpu_pic[ARM_PIC_CPU_IRQ], cpu_pic[ARM_PIC_CPU_FIQ]);
    goldfish_device_init(goldfish_pic, 0xff010000, 0x7f0000, 10, 22);

    goldfish_device_bus_init(0xff001000, 1);

    goldfish_timer_and_rtc_init(0xff003000, 3);

    goldfish_tty_add(serial_hds[0], 0, 0xff002000, 4);
    for(i = 1; i < MAX_SERIAL_PORTS; i++) {
        //printf("android_arm_init serial %d %x\n", i, serial_hds[i]);
        if(serial_hds[i]) {
            goldfish_tty_add(serial_hds[i], i, 0, 0);
        }
    }

    for(i = 0; i < MAX_NICS; i++) {
        if (nd_table[i].vlan) {
            if (nd_table[i].model == NULL
                || strcmp(nd_table[i].model, "smc91c111") == 0) {
                struct goldfish_device *smc_device;
                smc_device = qemu_mallocz(sizeof(*smc_device));
                smc_device->name = "smc91x";
                smc_device->id = i;
                smc_device->size = 0x1000;
                smc_device->irq_count = 1;
                goldfish_add_device_no_io(smc_device);
                smc91c111_init(&nd_table[i], smc_device->base, goldfish_pic[smc_device->irq]);
            } else {
                fprintf(stderr, "qemu: Unsupported NIC: %s\n", nd_table[0].model);
                exit (1);
            }
        }
    }

    goldfish_fb_init(0);
#ifdef HAS_AUDIO
    goldfish_audio_init(0xff004000, 0, audio_input_source);
#endif
    {
        DriveInfo* info = drive_get( IF_IDE, 0, 0 );
        if (info != NULL) {
            goldfish_mmc_init(0xff005000, 0, info->bdrv);
        }
    }

    goldfish_memlog_init(0xff006000);

    if (android_hw->hw_battery)
        goldfish_battery_init();

    goldfish_add_device_no_io(&event0_device);
    events_dev_init(event0_device.base, goldfish_pic[event0_device.irq]);

#ifdef CONFIG_NAND
    goldfish_add_device_no_io(&nand_device);
    nand_dev_init(nand_device.base);
#endif
#ifdef CONFIG_TRACE
    extern const char *trace_filename;
    /* Init trace device if either tracing, or memory checking is enabled. */
    if (trace_filename != NULL
#ifdef CONFIG_MEMCHECK
        || memcheck_enabled
#endif  // CONFIG_MEMCHECK
       ) {
        trace_dev_init();
    }
    if (trace_filename != NULL) {
        D( "Trace file name is set to %s\n", trace_filename );
    } else  {
        D("Trace file name is not set\n");
    }
#endif

    pipe_dev_init();

#if TEST_SWITCH
    {
        void *sw;
        sw = goldfish_switch_add("test", NULL, NULL, 0);
        goldfish_switch_set_state(sw, 1);
        goldfish_switch_add("test2", switch_test_write, sw, 1);
    }
#endif

    memset(&info, 0, sizeof info);
    info.ram_size        = ram_size;
    info.kernel_filename = kernel_filename;
    info.kernel_cmdline  = kernel_cmdline;
    info.initrd_filename = initrd_filename;
    info.nb_cpus         = 1;
    info.board_id        = 1441;

    arm_load_kernel(env, &info);
}

QEMUMachine android_arm_machine = {
    "android_arm",
    "ARM Android Emulator",
    android_arm_init_,
    0,
    0,
    1,
    NULL
};

static void android_arm_init(void)
{
    qemu_register_machine(&android_arm_machine);
}

machine_init(android_arm_init);
