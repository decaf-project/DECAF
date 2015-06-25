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
#include "sysemu.h"
#include "mips.h"
#include "goldfish_device.h"
#include "android/globals.h"
#include "audio/audio.h"
#include "blockdev.h"
#ifdef CONFIG_MEMCHECK
#include "memcheck/memcheck_api.h"
#endif  // CONFIG_MEMCHECK

#include "android/utils/debug.h"

#define  D(...)  VERBOSE_PRINT(init,__VA_ARGS__)

#define MIPS_CPU_SAVE_VERSION  1
#define GOLDFISH_IO_SPACE       0x1f000000
#define GOLDFISH_INTERRUPT	0x1f000000
#define GOLDFISH_DEVICEBUS	0x1f001000
#define GOLDFISH_TTY		0x1f002000
#define GOLDFISH_RTC		0x1f003000
#define GOLDFISH_AUDIO		0x1f004000
#define GOLDFISH_MMC		0x1f005000
#define GOLDFISH_MEMLOG		0x1f006000
#define GOLDFISH_DEVICES	0x1f010000

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

#define VIRT_TO_PHYS_ADDEND (-((int64_t)(int32_t)0x80000000))

#define PHYS_TO_VIRT(x) ((x) | ~(target_ulong)0x7fffffff)

static void android_load_kernel(CPUState *env, int ram_size, const char *kernel_filename,
              const char *kernel_cmdline, const char *initrd_filename)
{
    int initrd_size;
    ram_addr_t initrd_offset;
    uint64_t kernel_entry, kernel_low, kernel_high;
    unsigned int cmdline;

    /* Load the kernel.  */
    if (!kernel_filename) {
        fprintf(stderr, "Kernel image must be specified\n");
        exit(1);
    }
    if (load_elf(kernel_filename, VIRT_TO_PHYS_ADDEND,
         (uint64_t *)&kernel_entry, (uint64_t *)&kernel_low,
         (uint64_t *)&kernel_high) < 0) {
    fprintf(stderr, "qemu: could not load kernel '%s'\n", kernel_filename);
    exit(1);
    }
    env->active_tc.PC = (int32_t)kernel_entry;

    /* load initrd */
    initrd_size = 0;
    initrd_offset = 0;
    if (initrd_filename) {
    initrd_size = get_image_size (initrd_filename);
    if (initrd_size > 0) {
        initrd_offset = (kernel_high + ~TARGET_PAGE_MASK) & TARGET_PAGE_MASK;
            if (initrd_offset + initrd_size > ram_size) {
        fprintf(stderr,
                        "qemu: memory too small for initial ram disk '%s'\n",
                        initrd_filename);
                exit(1);
            }
            initrd_size = load_image_targphys(initrd_filename,
                                               initrd_offset,
                                               ram_size - initrd_offset);

    }
        if (initrd_size == (target_ulong) -1) {
        fprintf(stderr, "qemu: could not load initial ram disk '%s'\n",
            initrd_filename);
            exit(1);
        }
    }

    /* Store command line in top page of memory
     * kernel will copy the command line to a loca buffer
     */
    cmdline = ram_size - TARGET_PAGE_SIZE;
    char kernel_cmd[1024];
    if (initrd_size > 0)
        sprintf (kernel_cmd, "%s rd_start=0x" TARGET_FMT_lx " rd_size=%li",
                       kernel_cmdline,
                       PHYS_TO_VIRT(initrd_offset), initrd_size);
    else
        strcpy (kernel_cmd, kernel_cmdline);

    cpu_physical_memory_write(ram_size - TARGET_PAGE_SIZE, (void *)kernel_cmd, strlen(kernel_cmd) + 1);
   
#if 0
    if (initrd_size > 0)
        sprintf (phys_ram_base+cmdline, "%s rd_start=0x" TARGET_FMT_lx " rd_size=%li",
                       kernel_cmdline,
                       PHYS_TO_VIRT(initrd_offset), initrd_size);
    else
        strcpy (phys_ram_base+cmdline, kernel_cmdline);
#endif

    env->active_tc.gpr[4] = PHYS_TO_VIRT(cmdline);/* a0 */
    env->active_tc.gpr[5] = ram_size;       /* a1 */
    env->active_tc.gpr[6] = 0;          /* a2 */
    env->active_tc.gpr[7] = 0;          /* a3 */

}


static void android_mips_init_(ram_addr_t ram_size,
    const char *boot_device,
    const char *kernel_filename,
    const char *kernel_cmdline,
    const char *initrd_filename,
    const char *cpu_model)
{
    CPUState *env;
    qemu_irq *goldfish_pic;
    int i;
    ram_addr_t ram_offset;

    if (!cpu_model)
        cpu_model = "24Kf";

    env = cpu_init(cpu_model);

    register_savevm( "cpu", 0, MIPS_CPU_SAVE_VERSION, cpu_save, cpu_load, env );

    if (ram_size > GOLDFISH_IO_SPACE)
        ram_size = GOLDFISH_IO_SPACE;   /* avoid overlap of ram and IO regs */
    ram_offset = qemu_ram_alloc(NULL, "android_mips", ram_size);
    cpu_register_physical_memory(0, ram_size, ram_offset | IO_MEM_RAM);

    /* Init internal devices */
    cpu_mips_irq_init_cpu(env);
    cpu_mips_clock_init(env);

    goldfish_pic = goldfish_interrupt_init(GOLDFISH_INTERRUPT,
					   env->irq[2], env->irq[3]);
    goldfish_device_init(goldfish_pic, GOLDFISH_DEVICES, 0x7f0000, 10, 22);

    goldfish_device_bus_init(GOLDFISH_DEVICEBUS, 1);

    goldfish_timer_and_rtc_init(GOLDFISH_RTC, 3);

    goldfish_tty_add(serial_hds[0], 0, GOLDFISH_TTY, 4);
    for(i = 1; i < MAX_SERIAL_PORTS; i++) {
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
    goldfish_audio_init(GOLDFISH_AUDIO, 0, audio_input_source);
#endif
    {
        DriveInfo* info = drive_get( IF_IDE, 0, 0 );
        if (info != NULL) {
            goldfish_mmc_init(GOLDFISH_MMC, 0, info->bdrv);
	}
    }
    goldfish_memlog_init(GOLDFISH_MEMLOG);

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

    android_load_kernel(env, ram_size, kernel_filename, kernel_cmdline, initrd_filename);
}


QEMUMachine android_mips_machine = {
    "android_mips",
    "MIPS Android Emulator",
    android_mips_init_,
    0,
    0,
    1,
    NULL
};

static void android_mips_init(void)
{
    qemu_register_machine(&android_mips_machine);
}

machine_init(android_mips_init);
