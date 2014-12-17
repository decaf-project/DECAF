/*
 * QEMU System Emulator
 *
 * Copyright (c) 2003-2008 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>
#include <zlib.h>

/* Needed early for CONFIG_BSD etc. */
#include "config-host.h"

#ifndef _WIN32
#include <libgen.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <termios.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#if defined(__NetBSD__)
#include <net/if_tap.h>
#endif
#ifdef __linux__
#include <linux/if_tun.h>
#endif
#include <arpa/inet.h>
#include <dirent.h>
#include <netdb.h>
#include <sys/select.h>
#ifdef CONFIG_BSD
#include <sys/stat.h>
#if defined(__FreeBSD__) || defined(__DragonFly__)
#include <libutil.h>
#else
#include <util.h>
#endif
#elif defined (__GLIBC__) && defined (__FreeBSD_kernel__)
#include <freebsd/stdlib.h>
#else
#ifdef __linux__
#include <pty.h>
#include <malloc.h>
#include <linux/rtc.h>

/* For the benefit of older linux systems which don't supply it,
   we use a local copy of hpet.h. */
/* #include <linux/hpet.h> */
#include "hpet.h"

#include <linux/ppdev.h>
#include <linux/parport.h>
#endif
#ifdef __sun__
#include <sys/stat.h>
#include <sys/ethernet.h>
#include <sys/sockio.h>
#include <netinet/arp.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h> // must come after ip.h
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <net/if.h>
#include <syslog.h>
#include <stropts.h>
#endif
#endif
#endif

#if defined(__OpenBSD__)
#include <util.h>
#endif

#if defined(CONFIG_VDE)
#include <libvdeplug.h>
#endif

#ifdef _WIN32
#include <windows.h>
#include <malloc.h>
#include <sys/timeb.h>
#include <mmsystem.h>
#define getopt_long_only getopt_long
#define memalign(align, size) malloc(size)
#endif

#include "cpus.h"
#include "arch_init.h"

#ifdef CONFIG_SDL
#ifdef __APPLE__
#include <SDL.h>
int qemu_main(int argc, char **argv, char **envp);
int main(int argc, char **argv)
{
    qemu_main(argc, argv, NULL);
}
#undef main
#define main qemu_main
#endif
#endif /* CONFIG_SDL */

#ifdef CONFIG_COCOA
int qemu_main(int argc, char **argv, char **envp);
#undef main
#define main qemu_main
#endif /* CONFIG_COCOA */

#include "hw/hw.h"
#include "hw/boards.h"
#include "hw/usb.h"
#include "hw/pcmcia.h"
#include "hw/pc.h"
#include "hw/isa.h"
#include "hw/baum.h"
#include "hw/bt.h"
#include "hw/watchdog.h"
#include "hw/smbios.h"
#include "hw/xen.h"
#include "bt-host.h"
#include "net.h"
#include "monitor.h"
#include "console.h"
#include "sysemu.h"
#include "gdbstub.h"
#include "qemu-timer.h"
#include "qemu-char.h"
#include "cache-utils.h"
#include "block.h"
#include "dma.h"
#include "audio/audio.h"
#include "migration.h"
#include "kvm.h"
#include "balloon.h"
#include "qemu-option.h"

#include "disas.h"

#include "exec-all.h"

#include "qemu_socket.h"

#if defined(CONFIG_SLIRP)
#include "libslirp.h"
#endif



#define DEFAULT_RAM_SIZE 128

/* Max number of USB devices that can be specified on the commandline.  */
#define MAX_USB_CMDLINE 8

/* Max number of bluetooth switches on the commandline.  */
#define MAX_BT_CMDLINE 10

/* XXX: use a two level table to limit memory usage */
#define MAX_IOPORTS 65536

static const char *data_dir;
const char *bios_name = NULL;
static void *ioport_opaque[MAX_IOPORTS];
static IOPortReadFunc *ioport_read_table[3][MAX_IOPORTS];
static IOPortWriteFunc *ioport_write_table[3][MAX_IOPORTS];
/* Note: drives_table[MAX_DRIVES] is a dummy block driver if none available
   to store the VM snapshots */
DriveInfo drives_table[MAX_DRIVES+1];
int nb_drives;
enum vga_retrace_method vga_retrace_method = VGA_RETRACE_DUMB;
DisplayType display_type = DT_DEFAULT;
const char* keyboard_layout = NULL;
ram_addr_t ram_size;
const char *mem_path = NULL;
#ifdef MAP_POPULATE
int mem_prealloc = 0; /* force preallocation of physical target memory */
#endif
int nb_nics;
NICInfo nd_table[MAX_NICS];
int vm_running;
static int autostart;
static int rtc_utc = 1;
static int rtc_date_offset = -1; /* -1 means no change */
int cirrus_vga_enabled = 1;
int std_vga_enabled = 0;
int vmsvga_enabled = 0;
int xenfb_enabled = 0;
QEMUClock *rtc_clock;
static int full_screen = 0;
#ifdef CONFIG_SDL
static int no_frame = 0;
#endif
int no_quit = 0;
CharDriverState *serial_hds[MAX_SERIAL_PORTS];
CharDriverState *parallel_hds[MAX_PARALLEL_PORTS];
CharDriverState *virtcon_hds[MAX_VIRTIO_CONSOLES];
#ifdef TARGET_I386
int win2k_install_hack = 0;
int rtc_td_hack = 0;
#endif
int usb_enabled = 0;
int singlestep = 0;
int smp_cpus = 1;
const char *vnc_display;
int acpi_enabled = 1;
int no_hpet = 0;
int no_virtio_balloon = 0;
int fd_bootchk = 1;
int no_reboot = 0;
int no_shutdown = 0;
int cursor_hide = 1;
int graphic_rotate = 0;
WatchdogTimerModel *watchdog = NULL;
int watchdog_action = WDT_RESET;
const char *option_rom[MAX_OPTION_ROMS];
int nb_option_roms;
int semihosting_enabled = 0;
#ifdef TARGET_ARM
int old_param = 0;
#endif
const char *qemu_name;
int alt_grab = 0;
#if defined(TARGET_SPARC) || defined(TARGET_PPC)
unsigned int nb_prom_envs = 0;
const char *prom_envs[MAX_PROM_ENVS];
#endif
int nb_drives_opt;
struct drive_opt drives_opt[MAX_DRIVES];

int nb_numa_nodes;
uint64_t node_mem[MAX_NODES];
uint64_t node_cpumask[MAX_NODES];

static QEMUTimer *nographic_timer;

uint8_t qemu_uuid[16];

/***********************************************************/
/* x86 ISA bus support */

target_phys_addr_t isa_mem_base = 0;
PicState2 *isa_pic;

static IOPortReadFunc default_ioport_readb, default_ioport_readw, default_ioport_readl;
static IOPortWriteFunc default_ioport_writeb, default_ioport_writew, default_ioport_writel;

static uint32_t ioport_read(int index, uint32_t address)
{
    static IOPortReadFunc *default_func[3] = {
        default_ioport_readb,
        default_ioport_readw,
        default_ioport_readl
    };
    IOPortReadFunc *func = ioport_read_table[index][address];
    if (!func)
        func = default_func[index];
    return func(ioport_opaque[address], address);
}

static void ioport_write(int index, uint32_t address, uint32_t data)
{
    static IOPortWriteFunc *default_func[3] = {
        default_ioport_writeb,
        default_ioport_writew,
        default_ioport_writel
    };
    IOPortWriteFunc *func = ioport_write_table[index][address];
    if (!func)
        func = default_func[index];
    func(ioport_opaque[address], address, data);
}

static uint32_t default_ioport_readb(void *opaque, uint32_t address)
{
#ifdef DEBUG_UNUSED_IOPORT
    fprintf(stderr, "unused inb: port=0x%04x\n", address);
#endif
    return 0xff;
}

static void default_ioport_writeb(void *opaque, uint32_t address, uint32_t data)
{
#ifdef DEBUG_UNUSED_IOPORT
    fprintf(stderr, "unused outb: port=0x%04x data=0x%02x\n", address, data);
#endif
}

/* default is to make two byte accesses */
static uint32_t default_ioport_readw(void *opaque, uint32_t address)
{
    uint32_t data;
    data = ioport_read(0, address);
    address = (address + 1) & (MAX_IOPORTS - 1);
    data |= ioport_read(0, address) << 8;
    return data;
}

static void default_ioport_writew(void *opaque, uint32_t address, uint32_t data)
{
    ioport_write(0, address, data & 0xff);
    address = (address + 1) & (MAX_IOPORTS - 1);
    ioport_write(0, address, (data >> 8) & 0xff);
}

static uint32_t default_ioport_readl(void *opaque, uint32_t address)
{
#ifdef DEBUG_UNUSED_IOPORT
    fprintf(stderr, "unused inl: port=0x%04x\n", address);
#endif
    return 0xffffffff;
}

static void default_ioport_writel(void *opaque, uint32_t address, uint32_t data)
{
#ifdef DEBUG_UNUSED_IOPORT
    fprintf(stderr, "unused outl: port=0x%04x data=0x%02x\n", address, data);
#endif
}


/***************/
/* ballooning */

static QEMUBalloonEvent *qemu_balloon_event;
void *qemu_balloon_event_opaque;

void qemu_add_balloon_handler(QEMUBalloonEvent *func, void *opaque)
{
    qemu_balloon_event = func;
    qemu_balloon_event_opaque = opaque;
}

void qemu_balloon(ram_addr_t target)
{
    if (qemu_balloon_event)
        qemu_balloon_event(qemu_balloon_event_opaque, target);
}

ram_addr_t qemu_balloon_status(void)
{
    if (qemu_balloon_event)
        return qemu_balloon_event(qemu_balloon_event_opaque, 0);
    return 0;
}

/***********************************************************/
/* host time/date access */
void qemu_get_timedate(struct tm *tm, int offset)
{
    time_t ti;
    struct tm *ret;

    time(&ti);
    ti += offset;
    if (rtc_date_offset == -1) {
        if (rtc_utc)
            ret = gmtime(&ti);
        else
            ret = localtime(&ti);
    } else {
        ti -= rtc_date_offset;
        ret = gmtime(&ti);
    }

    memcpy(tm, ret, sizeof(struct tm));
}

int qemu_timedate_diff(struct tm *tm)
{
    time_t seconds;

    if (rtc_date_offset == -1)
        if (rtc_utc)
            seconds = mktimegm(tm);
        else
            seconds = mktime(tm);
    else
        seconds = mktimegm(tm) + rtc_date_offset;

    return seconds - time(NULL);
}

#ifdef _WIN32
static void socket_cleanup(void)
{
    WSACleanup();
}

static int socket_init(void)
{
    WSADATA Data;
    int ret, err;

    ret = WSAStartup(MAKEWORD(2,2), &Data);
    if (ret != 0) {
        err = WSAGetLastError();
        fprintf(stderr, "WSAStartup: %d\n", err);
        return -1;
    }
    atexit(socket_cleanup);
    return 0;
}
#endif

int get_param_value(char *buf, int buf_size,
                    const char *tag, const char *str)
{
    const char *p;
    char option[128];

    p = str;
    for(;;) {
        p = get_opt_name(option, sizeof(option), p, '=');
        if (*p != '=')
            break;
        p++;
        if (!strcmp(tag, option)) {
            (void)get_opt_value(buf, buf_size, p);
            return strlen(buf);
        } else {
            p = get_opt_value(NULL, 0, p);
        }
        if (*p != ',')
            break;
        p++;
    }
    return 0;
}

int check_params(char *buf, int buf_size,
                 const char * const *params, const char *str)
{
    const char *p;
    int i;

    p = str;
    while (*p != '\0') {
        p = get_opt_name(buf, buf_size, p, '=');
        if (*p != '=') {
            return -1;
        }
        p++;
        for (i = 0; params[i] != NULL; i++) {
            if (!strcmp(params[i], buf)) {
                break;
            }
        }
        if (params[i] == NULL) {
            return -1;
        }
        p = get_opt_value(NULL, 0, p);
        if (*p != ',') {
            break;
        }
        p++;
    }
    return 0;
}

/***********************************************************/
/* Bluetooth support */
static int nb_hcis;
static int cur_hci;
static struct HCIInfo *hci_table[MAX_NICS];

static struct bt_vlan_s {
    struct bt_scatternet_s net;
    int id;
    struct bt_vlan_s *next;
} *first_bt_vlan;

/* find or alloc a new bluetooth "VLAN" */
static struct bt_scatternet_s *qemu_find_bt_vlan(int id)
{
    struct bt_vlan_s **pvlan, *vlan;
    for (vlan = first_bt_vlan; vlan != NULL; vlan = vlan->next) {
        if (vlan->id == id)
            return &vlan->net;
    }
    vlan = qemu_mallocz(sizeof(struct bt_vlan_s));
    vlan->id = id;
    pvlan = &first_bt_vlan;
    while (*pvlan != NULL)
        pvlan = &(*pvlan)->next;
    *pvlan = vlan;
    return &vlan->net;
}

static void null_hci_send(struct HCIInfo *hci, const uint8_t *data, int len)
{
}

static int null_hci_addr_set(struct HCIInfo *hci, const uint8_t *bd_addr)
{
    return -ENOTSUP;
}

static struct HCIInfo null_hci = {
    .cmd_send = null_hci_send,
    .sco_send = null_hci_send,
    .acl_send = null_hci_send,
    .bdaddr_set = null_hci_addr_set,
};

struct HCIInfo *qemu_next_hci(void)
{
    if (cur_hci == nb_hcis)
        return &null_hci;

    return hci_table[cur_hci++];
}

static struct HCIInfo *hci_init(const char *str)
{
    char *endp;
    struct bt_scatternet_s *vlan = 0;

    if (!strcmp(str, "null"))
        /* null */
        return &null_hci;
    else if (!strncmp(str, "host", 4) && (str[4] == '\0' || str[4] == ':'))
        /* host[:hciN] */
        return bt_host_hci(str[4] ? str + 5 : "hci0");
    else if (!strncmp(str, "hci", 3)) {
        /* hci[,vlan=n] */
        if (str[3]) {
            if (!strncmp(str + 3, ",vlan=", 6)) {
                vlan = qemu_find_bt_vlan(strtol(str + 9, &endp, 0));
                if (*endp)
                    vlan = 0;
            }
        } else
            vlan = qemu_find_bt_vlan(0);
        if (vlan)
           return bt_new_hci(vlan);
    }

    fprintf(stderr, "qemu: Unknown bluetooth HCI `%s'.\n", str);

    return 0;
}

static int bt_hci_parse(const char *str)
{
    struct HCIInfo *hci;
    bdaddr_t bdaddr;

    if (nb_hcis >= MAX_NICS) {
        fprintf(stderr, "qemu: Too many bluetooth HCIs (max %i).\n", MAX_NICS);
        return -1;
    }

    hci = hci_init(str);
    if (!hci)
        return -1;

    bdaddr.b[0] = 0x52;
    bdaddr.b[1] = 0x54;
    bdaddr.b[2] = 0x00;
    bdaddr.b[3] = 0x12;
    bdaddr.b[4] = 0x34;
    bdaddr.b[5] = 0x56 + nb_hcis;
    hci->bdaddr_set(hci, bdaddr.b);

    hci_table[nb_hcis++] = hci;

    return 0;
}

static void bt_vhci_add(int vlan_id)
{
    struct bt_scatternet_s *vlan = qemu_find_bt_vlan(vlan_id);

    if (!vlan->slave)
        fprintf(stderr, "qemu: warning: adding a VHCI to "
                        "an empty scatternet %i\n", vlan_id);

    bt_vhci_init(bt_new_hci(vlan));
}

static struct bt_device_s *bt_device_add(const char *opt)
{
    struct bt_scatternet_s *vlan;
    int vlan_id = 0;
    char *endp = strstr(opt, ",vlan=");
    int len = (endp ? endp - opt : strlen(opt)) + 1;
    char devname[10];

    pstrcpy(devname, MIN(sizeof(devname), len), opt);

    if (endp) {
        vlan_id = strtol(endp + 6, &endp, 0);
        if (*endp) {
            fprintf(stderr, "qemu: unrecognised bluetooth vlan Id\n");
            return 0;
        }
    }

    vlan = qemu_find_bt_vlan(vlan_id);

    if (!vlan->slave)
        fprintf(stderr, "qemu: warning: adding a slave device to "
                        "an empty scatternet %i\n", vlan_id);

    if (!strcmp(devname, "keyboard"))
        return bt_keyboard_init(vlan);

    fprintf(stderr, "qemu: unsupported bluetooth device `%s'\n", devname);
    return 0;
}

static int bt_parse(const char *opt)
{
    const char *endp, *p;
    int vlan;

    if (strstart(opt, "hci", &endp)) {
        if (!*endp || *endp == ',') {
            if (*endp)
                if (!strstart(endp, ",vlan=", 0))
                    opt = endp + 1;

            return bt_hci_parse(opt);
       }
    } else if (strstart(opt, "vhci", &endp)) {
        if (!*endp || *endp == ',') {
            if (*endp) {
                if (strstart(endp, ",vlan=", &p)) {
                    vlan = strtol(p, (char **) &endp, 0);
                    if (*endp) {
                        fprintf(stderr, "qemu: bad scatternet '%s'\n", p);
                        return 1;
                    }
                } else {
                    fprintf(stderr, "qemu: bad parameter '%s'\n", endp + 1);
                    return 1;
                }
            } else
                vlan = 0;

            bt_vhci_add(vlan);
            return 0;
        }
    } else if (strstart(opt, "device:", &endp))
        return !bt_device_add(endp);

    fprintf(stderr, "qemu: bad bluetooth parameter '%s'\n", opt);
    return 1;
}

/***********************************************************/
/* QEMU Block devices */

#define HD_ALIAS "index=%d,media=disk"
#define CDROM_ALIAS "index=2,media=cdrom"
#define FD_ALIAS "index=%d,if=floppy"
#define PFLASH_ALIAS "if=pflash"
#define MTD_ALIAS "if=mtd"
#define SD_ALIAS "index=0,if=sd"

static int drive_init_func(QemuOpts *opts, void *opaque)
{
    int *use_scsi = opaque;
    int fatal_error = 0;

    if (drive_init(opts, *use_scsi, &fatal_error) == NULL) {
        if (fatal_error)
            return 1;
    }
    return 0;
}

static int drive_enable_snapshot(QemuOpts *opts, void *opaque)
{
    if (NULL == qemu_opt_get(opts, "snapshot")) {
        qemu_opt_set(opts, "snapshot", "on");
    }
    return 0;
}

static int drive_opt_get_free_idx(void)
{
    int index;

    for (index = 0; index < MAX_DRIVES; index++)
        if (!drives_opt[index].used) {
            drives_opt[index].used = 1;
            return index;
        }

    return -1;
}

static int drive_get_free_idx(void)
{
    int index;

    for (index = 0; index < MAX_DRIVES; index++)
        if (!drives_table[index].used) {
            drives_table[index].used = 1;
            return index;
        }

    return -1;
}

int drive_add(const char *file, const char *fmt, ...)
{
    va_list ap;
    int index = drive_opt_get_free_idx();

    if (nb_drives_opt >= MAX_DRIVES || index == -1) {
        fprintf(stderr, "qemu: too many drives\n");
        return -1;
    }

    drives_opt[index].file = file;
    va_start(ap, fmt);
    vsnprintf(drives_opt[index].opt,
              sizeof(drives_opt[0].opt), fmt, ap);
    va_end(ap);

    nb_drives_opt++;
    return index;
}

void drive_remove(int index)
{
    drives_opt[index].used = 0;
    nb_drives_opt--;
}

int drive_get_index(BlockInterfaceType type, int bus, int unit)
{
    int index;

    /* seek interface, bus and unit */

    for (index = 0; index < MAX_DRIVES; index++)
        if (drives_table[index].type == type &&
	    drives_table[index].bus == bus &&
	    drives_table[index].unit == unit &&
	    drives_table[index].used)
        return index;

    return -1;
}

int drive_get_max_bus(BlockInterfaceType type)
{
    int max_bus;
    int index;

    max_bus = -1;
    for (index = 0; index < nb_drives; index++) {
        if(drives_table[index].type == type &&
           drives_table[index].bus > max_bus)
            max_bus = drives_table[index].bus;
    }
    return max_bus;
}

const char *drive_get_serial(BlockDriverState *bdrv)
{
    int index;

    for (index = 0; index < nb_drives; index++)
        if (drives_table[index].bdrv == bdrv)
            return drives_table[index].serial;

    return "\0";
}

BlockInterfaceErrorAction drive_get_onerror(BlockDriverState *bdrv)
{
    int index;

    for (index = 0; index < nb_drives; index++)
        if (drives_table[index].bdrv == bdrv)
            return drives_table[index].onerror;

    return BLOCK_ERR_STOP_ENOSPC;
}

static void bdrv_format_print(void *opaque, const char *name)
{
    fprintf(stderr, " %s", name);
}

void drive_uninit(BlockDriverState *bdrv)
{
    int i;

    for (i = 0; i < MAX_DRIVES; i++)
        if (drives_table[i].bdrv == bdrv) {
            drives_table[i].bdrv = NULL;
            drives_table[i].used = 0;
            drive_remove(drives_table[i].drive_opt_idx);
            nb_drives--;
            break;
        }
}

int drive_init(struct drive_opt *arg, int snapshot, void *opaque)
{
    char buf[128];
    char file[1024];
    char devname[128];
    char serial[21];
    const char *mediastr = "";
    BlockInterfaceType type;
    enum { MEDIA_DISK, MEDIA_CDROM } media;
    int bus_id, unit_id;
    int cyls, heads, secs, translation;
    BlockDriverState *bdrv;
    BlockDriver *drv = NULL;
    QEMUMachine *machine = opaque;
    int max_devs;
    int index;
    int cache;
    int bdrv_flags, onerror;
    int drives_table_idx;
    char *str = arg->opt;
    static const char * const params[] = { "bus", "unit", "if", "index",
                                           "cyls", "heads", "secs", "trans",
                                           "media", "snapshot", "file",
                                           "cache", "format", "serial", "werror",
                                           NULL };

    if (check_params(buf, sizeof(buf), params, str) < 0) {
         fprintf(stderr, "qemu: unknown parameter '%s' in '%s'\n",
                         buf, str);
         return -1;
    }

    file[0] = 0;
    cyls = heads = secs = 0;
    bus_id = 0;
    unit_id = -1;
    translation = BIOS_ATA_TRANSLATION_AUTO;
    index = -1;
    cache = 3;

    if (machine->use_scsi) {
        type = IF_SCSI;
        max_devs = MAX_SCSI_DEVS;
        pstrcpy(devname, sizeof(devname), "scsi");
    } else {
        type = IF_IDE;
        max_devs = MAX_IDE_DEVS;
        pstrcpy(devname, sizeof(devname), "ide");
    }
    media = MEDIA_DISK;

    /* extract parameters */

    if (get_param_value(buf, sizeof(buf), "bus", str)) {
        bus_id = strtol(buf, NULL, 0);
	if (bus_id < 0) {
	    fprintf(stderr, "qemu: '%s' invalid bus id\n", str);
	    return -1;
	}
    }

    if (get_param_value(buf, sizeof(buf), "unit", str)) {
        unit_id = strtol(buf, NULL, 0);
	if (unit_id < 0) {
	    fprintf(stderr, "qemu: '%s' invalid unit id\n", str);
	    return -1;
	}
    }

    if (get_param_value(buf, sizeof(buf), "if", str)) {
        pstrcpy(devname, sizeof(devname), buf);
        if (!strcmp(buf, "ide")) {
	    type = IF_IDE;
            max_devs = MAX_IDE_DEVS;
        } else if (!strcmp(buf, "scsi")) {
	    type = IF_SCSI;
            max_devs = MAX_SCSI_DEVS;
        } else if (!strcmp(buf, "floppy")) {
	    type = IF_FLOPPY;
            max_devs = 0;
        } else if (!strcmp(buf, "pflash")) {
	    type = IF_PFLASH;
            max_devs = 0;
	} else if (!strcmp(buf, "mtd")) {
	    type = IF_MTD;
            max_devs = 0;
	} else if (!strcmp(buf, "sd")) {
	    type = IF_SD;
            max_devs = 0;
        } else if (!strcmp(buf, "virtio")) {
            type = IF_VIRTIO;
            max_devs = 0;
	} else if (!strcmp(buf, "xen")) {
	    type = IF_XEN;
            max_devs = 0;
	} else {
            fprintf(stderr, "qemu: '%s' unsupported bus type '%s'\n", str, buf);
            return -1;
	}
    }

    if (get_param_value(buf, sizeof(buf), "index", str)) {
        index = strtol(buf, NULL, 0);
	if (index < 0) {
	    fprintf(stderr, "qemu: '%s' invalid index\n", str);
	    return -1;
	}
    }

    if (get_param_value(buf, sizeof(buf), "cyls", str)) {
        cyls = strtol(buf, NULL, 0);
    }

    if (get_param_value(buf, sizeof(buf), "heads", str)) {
        heads = strtol(buf, NULL, 0);
    }

    if (get_param_value(buf, sizeof(buf), "secs", str)) {
        secs = strtol(buf, NULL, 0);
    }

    if (cyls || heads || secs) {
        if (cyls < 1 || cyls > 16383) {
            fprintf(stderr, "qemu: '%s' invalid physical cyls number\n", str);
	    return -1;
	}
        if (heads < 1 || heads > 16) {
            fprintf(stderr, "qemu: '%s' invalid physical heads number\n", str);
	    return -1;
	}
        if (secs < 1 || secs > 63) {
            fprintf(stderr, "qemu: '%s' invalid physical secs number\n", str);
	    return -1;
	}
    }

    if (get_param_value(buf, sizeof(buf), "trans", str)) {
        if (!cyls) {
            fprintf(stderr,
                    "qemu: '%s' trans must be used with cyls,heads and secs\n",
                    str);
            return -1;
        }
        if (!strcmp(buf, "none"))
            translation = BIOS_ATA_TRANSLATION_NONE;
        else if (!strcmp(buf, "lba"))
            translation = BIOS_ATA_TRANSLATION_LBA;
        else if (!strcmp(buf, "auto"))
            translation = BIOS_ATA_TRANSLATION_AUTO;
	else {
            fprintf(stderr, "qemu: '%s' invalid translation type\n", str);
	    return -1;
	}
    }

    if (get_param_value(buf, sizeof(buf), "media", str)) {
        if (!strcmp(buf, "disk")) {
	    media = MEDIA_DISK;
	} else if (!strcmp(buf, "cdrom")) {
            if (cyls || secs || heads) {
                fprintf(stderr,
                        "qemu: '%s' invalid physical CHS format\n", str);
	        return -1;
            }
	    media = MEDIA_CDROM;
	} else {
	    fprintf(stderr, "qemu: '%s' invalid media\n", str);
	    return -1;
	}
    }

    if (get_param_value(buf, sizeof(buf), "snapshot", str)) {
        if (!strcmp(buf, "on"))
	    snapshot = 1;
        else if (!strcmp(buf, "off"))
	    snapshot = 0;
	else {
	    fprintf(stderr, "qemu: '%s' invalid snapshot option\n", str);
	    return -1;
	}
    }

    if (get_param_value(buf, sizeof(buf), "cache", str)) {
        if (!strcmp(buf, "off") || !strcmp(buf, "none"))
            cache = 0;
        else if (!strcmp(buf, "writethrough"))
            cache = 1;
        else if (!strcmp(buf, "writeback"))
            cache = 2;
        else {
           fprintf(stderr, "qemu: invalid cache option\n");
           return -1;
        }
    }

    if (get_param_value(buf, sizeof(buf), "format", str)) {
       if (strcmp(buf, "?") == 0) {
            fprintf(stderr, "qemu: Supported formats:");
            bdrv_iterate_format(bdrv_format_print, NULL);
            fprintf(stderr, "\n");
	    return -1;
        }
        drv = bdrv_find_format(buf);
        if (!drv) {
            fprintf(stderr, "qemu: '%s' invalid format\n", buf);
            return -1;
        }
    }

    if (arg->file == NULL)
        get_param_value(file, sizeof(file), "file", str);
    else
        pstrcpy(file, sizeof(file), arg->file);

    if (!get_param_value(serial, sizeof(serial), "serial", str))
	    memset(serial, 0,  sizeof(serial));

    onerror = BLOCK_ERR_STOP_ENOSPC;
    if (get_param_value(buf, sizeof(serial), "werror", str)) {
        if (type != IF_IDE && type != IF_SCSI && type != IF_VIRTIO) {
            fprintf(stderr, "werror is no supported by this format\n");
            return -1;
        }
        if (!strcmp(buf, "ignore"))
            onerror = BLOCK_ERR_IGNORE;
        else if (!strcmp(buf, "enospc"))
            onerror = BLOCK_ERR_STOP_ENOSPC;
        else if (!strcmp(buf, "stop"))
            onerror = BLOCK_ERR_STOP_ANY;
        else if (!strcmp(buf, "report"))
            onerror = BLOCK_ERR_REPORT;
        else {
            fprintf(stderr, "qemu: '%s' invalid write error action\n", buf);
            return -1;
        }
    }

    /* compute bus and unit according index */

    if (index != -1) {
        if (bus_id != 0 || unit_id != -1) {
            fprintf(stderr,
                    "qemu: '%s' index cannot be used with bus and unit\n", str);
            return -1;
        }
        if (max_devs == 0)
        {
            unit_id = index;
            bus_id = 0;
        } else {
            unit_id = index % max_devs;
            bus_id = index / max_devs;
        }
    }

    /* if user doesn't specify a unit_id,
     * try to find the first free
     */

    if (unit_id == -1) {
       unit_id = 0;
       while (drive_get_index(type, bus_id, unit_id) != -1) {
           unit_id++;
           if (max_devs && unit_id >= max_devs) {
               unit_id -= max_devs;
               bus_id++;
           }
       }
    }

    /* check unit id */

    if (max_devs && unit_id >= max_devs) {
        fprintf(stderr, "qemu: '%s' unit %d too big (max is %d)\n",
                        str, unit_id, max_devs - 1);
        return -1;
    }

    /*
     * ignore multiple definitions
     */

    if (drive_get_index(type, bus_id, unit_id) != -1)
        return -2;

    /* init */

    if (type == IF_IDE || type == IF_SCSI)
        mediastr = (media == MEDIA_CDROM) ? "-cd" : "-hd";
    if (max_devs)
        snprintf(buf, sizeof(buf), "%s%i%s%i",
                 devname, bus_id, mediastr, unit_id);
    else
        snprintf(buf, sizeof(buf), "%s%s%i",
                 devname, mediastr, unit_id);
    bdrv = bdrv_new(buf);
    drives_table_idx = drive_get_free_idx();
    drives_table[drives_table_idx].bdrv = bdrv;
    drives_table[drives_table_idx].type = type;
    drives_table[drives_table_idx].bus = bus_id;
    drives_table[drives_table_idx].unit = unit_id;
    drives_table[drives_table_idx].onerror = onerror;
    drives_table[drives_table_idx].drive_opt_idx = arg - drives_opt;
    strncpy(drives_table[drives_table_idx].serial, serial, sizeof(serial));
    nb_drives++;

    switch(type) {
    case IF_IDE:
    case IF_SCSI:
    case IF_XEN:
        switch(media) {
	case MEDIA_DISK:
            if (cyls != 0) {
                bdrv_set_geometry_hint(bdrv, cyls, heads, secs);
                bdrv_set_translation_hint(bdrv, translation);
            }
	    break;
	case MEDIA_CDROM:
            bdrv_set_type_hint(bdrv, BDRV_TYPE_CDROM);
	    break;
	}
        break;
    case IF_SD:
        /* FIXME: This isn't really a floppy, but it's a reasonable
           approximation.  */
    case IF_FLOPPY:
        bdrv_set_type_hint(bdrv, BDRV_TYPE_FLOPPY);
        break;
    case IF_PFLASH:
    case IF_MTD:
    case IF_VIRTIO:
        break;
    case IF_COUNT:
        abort();
    }
    if (!file[0])
        return -2;
    bdrv_flags = 0;
    if (snapshot) {
        bdrv_flags |= BDRV_O_SNAPSHOT;
        cache = 2; /* always use write-back with snapshot */
    }
    if (cache == 0) /* no caching */
        bdrv_flags |= BDRV_O_NOCACHE;
    else if (cache == 2) /* write-back */
        bdrv_flags |= BDRV_O_CACHE_WB;
    else if (cache == 3) /* not specified */
        bdrv_flags |= BDRV_O_CACHE_DEF;
    if (bdrv_open2(bdrv, file, bdrv_flags, drv) < 0) {
        fprintf(stderr, "qemu: could not open disk image %s\n",
                        file);
        return -1;
    }
    if (bdrv_key_required(bdrv))
        autostart = 0;
    return drives_table_idx;
}

static void numa_add(const char *optarg)
{
    char option[128];
    char *endptr;
    unsigned long long value, endvalue;
    int nodenr;

    optarg = get_opt_name(option, 128, optarg, ',') + 1;
    if (!strcmp(option, "node")) {
        if (get_param_value(option, 128, "nodeid", optarg) == 0) {
            nodenr = nb_numa_nodes;
        } else {
            nodenr = strtoull(option, NULL, 10);
        }

        if (get_param_value(option, 128, "mem", optarg) == 0) {
            node_mem[nodenr] = 0;
        } else {
            value = strtoull(option, &endptr, 0);
            switch (*endptr) {
            case 0: case 'M': case 'm':
                value <<= 20;
                break;
            case 'G': case 'g':
                value <<= 30;
                break;
            }
            node_mem[nodenr] = value;
        }
        if (get_param_value(option, 128, "cpus", optarg) == 0) {
            node_cpumask[nodenr] = 0;
        } else {
            value = strtoull(option, &endptr, 10);
            if (value >= 64) {
                value = 63;
                fprintf(stderr, "only 64 CPUs in NUMA mode supported.\n");
            } else {
                if (*endptr == '-') {
                    endvalue = strtoull(endptr+1, &endptr, 10);
                    if (endvalue >= 63) {
                        endvalue = 62;
                        fprintf(stderr,
                            "only 63 CPUs in NUMA mode supported.\n");
                    }
                    value = (1 << (endvalue + 1)) - (1 << value);
                } else {
                    value = 1 << value;
                }
            }
            node_cpumask[nodenr] = value;
        }
        nb_numa_nodes++;
    }
    return;
}

/***********************************************************/
/* USB devices */

static USBPort *used_usb_ports;
static USBPort *free_usb_ports;

/* ??? Maybe change this to register a hub to keep track of the topology.  */
void qemu_register_usb_port(USBPort *port, void *opaque, int index,
                            usb_attachfn attach)
{
    port->opaque = opaque;
    port->index = index;
    port->attach = attach;
    port->next = free_usb_ports;
    free_usb_ports = port;
}

int usb_device_add_dev(USBDevice *dev)
{
    USBPort *port;

    /* Find a USB port to add the device to.  */
    port = free_usb_ports;
    if (!port->next) {
        USBDevice *hub;

        /* Create a new hub and chain it on.  */
        free_usb_ports = NULL;
        port->next = used_usb_ports;
        used_usb_ports = port;

        hub = usb_hub_init(VM_USB_HUB_SIZE);
        usb_attach(port, hub);
        port = free_usb_ports;
    }

    free_usb_ports = port->next;
    port->next = used_usb_ports;
    used_usb_ports = port;
    usb_attach(port, dev);
    return 0;
}

static void usb_msd_password_cb(void *opaque, int err)
{
    USBDevice *dev = opaque;

    if (!err)
        usb_device_add_dev(dev);
    else
        dev->handle_destroy(dev);
}

static int usb_device_add(const char *devname, int is_hotplug)
{
    const char *p;
    USBDevice *dev;

    if (!free_usb_ports)
        return -1;

    if (strstart(devname, "host:", &p)) {
        dev = usb_host_device_open(p);
    } else if (!strcmp(devname, "mouse")) {
        dev = usb_mouse_init();
    } else if (!strcmp(devname, "tablet")) {
        dev = usb_tablet_init();
    } else if (!strcmp(devname, "keyboard")) {
        dev = usb_keyboard_init();
    } else if (strstart(devname, "disk:", &p)) {
        BlockDriverState *bs;

        dev = usb_msd_init(p);
        if (!dev)
            return -1;
        bs = usb_msd_get_bdrv(dev);
        if (bdrv_key_required(bs)) {
            autostart = 0;
            if (is_hotplug) {
                monitor_read_bdrv_key_start(cur_mon, bs, usb_msd_password_cb,
                                            dev);
                return 0;
            }
        }
    } else if (!strcmp(devname, "wacom-tablet")) {
        dev = usb_wacom_init();
    } else if (strstart(devname, "serial:", &p)) {
        dev = usb_serial_init(p);
#ifdef CONFIG_BRLAPI
    } else if (!strcmp(devname, "braille")) {
        dev = usb_baum_init();
#endif
    } else if (strstart(devname, "net:", &p)) {
        int nic = nb_nics;

        if (net_client_init(NULL, "nic", p) < 0)
            return -1;
        nd_table[nic].model = "usb";
        dev = usb_net_init(&nd_table[nic]);
    } else if (!strcmp(devname, "bt") || strstart(devname, "bt:", &p)) {
        dev = usb_bt_init(devname[2] ? hci_init(p) :
                        bt_new_hci(qemu_find_bt_vlan(0)));
    } else {
        return -1;
    }
    if (!dev)
        return -1;

    return usb_device_add_dev(dev);
}

int usb_device_del_addr(int bus_num, int addr)
{
    USBPort *port;
    USBPort **lastp;
    USBDevice *dev;

    if (!used_usb_ports)
        return -1;

    if (bus_num != 0)
        return -1;

    lastp = &used_usb_ports;
    port = used_usb_ports;
    while (port && port->dev->addr != addr) {
        lastp = &port->next;
        port = port->next;
    }

    if (!port)
        return -1;

    dev = port->dev;
    *lastp = port->next;
    usb_attach(port, NULL);
    dev->handle_destroy(dev);
    port->next = free_usb_ports;
    free_usb_ports = port;
    return 0;
}

static int usb_device_del(const char *devname)
{
    int bus_num, addr;
    const char *p;

    if (strstart(devname, "host:", &p))
        return usb_host_device_close(p);

    if (!used_usb_ports)
        return -1;

    p = strchr(devname, '.');
    if (!p)
        return -1;
    bus_num = strtoul(devname, NULL, 0);
    addr = strtoul(p + 1, NULL, 0);

    return usb_device_del_addr(bus_num, addr);
}

void do_usb_add(Monitor *mon, const char *devname)
{
    usb_device_add(devname, 1);
}

void do_usb_del(Monitor *mon, const char *devname)
{
    usb_device_del(devname);
}

void usb_info(Monitor *mon)
{
    USBDevice *dev;
    USBPort *port;
    const char *speed_str;

    if (!usb_enabled) {
        monitor_printf(mon, "USB support not enabled\n");
        return;
    }

    for (port = used_usb_ports; port; port = port->next) {
        dev = port->dev;
        if (!dev)
            continue;
        switch(dev->speed) {
        case USB_SPEED_LOW:
            speed_str = "1.5";
            break;
        case USB_SPEED_FULL:
            speed_str = "12";
            break;
        case USB_SPEED_HIGH:
            speed_str = "480";
            break;
        default:
            speed_str = "?";
            break;
        }
        monitor_printf(mon, "  Device %d.%d, Speed %s Mb/s, Product %s\n",
                       0, dev->addr, speed_str, dev->devname);
    }
}

/***********************************************************/
/* PCMCIA/Cardbus */

static struct pcmcia_socket_entry_s {
    PCMCIASocket *socket;
    struct pcmcia_socket_entry_s *next;
} *pcmcia_sockets = 0;

void pcmcia_socket_register(PCMCIASocket *socket)
{
    struct pcmcia_socket_entry_s *entry;

    entry = qemu_malloc(sizeof(struct pcmcia_socket_entry_s));
    entry->socket = socket;
    entry->next = pcmcia_sockets;
    pcmcia_sockets = entry;
}

void pcmcia_socket_unregister(PCMCIASocket *socket)
{
    struct pcmcia_socket_entry_s *entry, **ptr;

    ptr = &pcmcia_sockets;
    for (entry = *ptr; entry; ptr = &entry->next, entry = *ptr)
        if (entry->socket == socket) {
            *ptr = entry->next;
            qemu_free(entry);
        }
}

void pcmcia_info(Monitor *mon)
{
    struct pcmcia_socket_entry_s *iter;

    if (!pcmcia_sockets)
        monitor_printf(mon, "No PCMCIA sockets\n");

    for (iter = pcmcia_sockets; iter; iter = iter->next)
        monitor_printf(mon, "%s: %s\n", iter->socket->slot_string,
                       iter->socket->attached ? iter->socket->card_string :
                       "Empty");
}

/***********************************************************/
/* machine registration */

static QEMUMachine *first_machine = NULL;
QEMUMachine *current_machine = NULL;

int qemu_register_machine(QEMUMachine *m)
{
    QEMUMachine **pm;
    pm = &first_machine;
    while (*pm != NULL)
        pm = &(*pm)->next;
    m->next = NULL;
    *pm = m;
    return 0;
}

static QEMUMachine *find_machine(const char *name)
{
    QEMUMachine *m;

    for(m = first_machine; m != NULL; m = m->next) {
        if (!strcmp(m->name, name))
            return m;
    }
    return NULL;
}

static QEMUMachine *find_default_machine(void)
{
    QEMUMachine *m;

    for(m = first_machine; m != NULL; m = m->next) {
        if (m->is_default) {
            return m;
        }
    }
    return NULL;
}

/***********************************************************/
/* main execution loop */

static void gui_update(void *opaque)
{
    uint64_t interval = GUI_REFRESH_INTERVAL;
    DisplayState *ds = opaque;
    DisplayChangeListener *dcl = ds->listeners;

    dpy_refresh(ds);

    while (dcl != NULL) {
        if (dcl->gui_timer_interval &&
            dcl->gui_timer_interval < interval)
            interval = dcl->gui_timer_interval;
        dcl = dcl->next;
    }
    qemu_mod_timer(ds->gui_timer, interval + qemu_get_clock_ms(rt_clock));
}

static void nographic_update(void *opaque)
{
    uint64_t interval = GUI_REFRESH_INTERVAL;

    qemu_mod_timer(nographic_timer, interval + qemu_get_clock_ms(rt_clock));
}

struct vm_change_state_entry {
    VMChangeStateHandler *cb;
    void *opaque;
    QLIST_ENTRY (vm_change_state_entry) entries;
};

static QLIST_HEAD(vm_change_state_head, vm_change_state_entry) vm_change_state_head;

VMChangeStateEntry *qemu_add_vm_change_state_handler(VMChangeStateHandler *cb,
                                                     void *opaque)
{
    VMChangeStateEntry *e;

    e = qemu_mallocz(sizeof (*e));

    e->cb = cb;
    e->opaque = opaque;
    QLIST_INSERT_HEAD(&vm_change_state_head, e, entries);
    return e;
}

void qemu_del_vm_change_state_handler(VMChangeStateEntry *e)
{
    QLIST_REMOVE (e, entries);
    qemu_free (e);
}

void vm_state_notify(int running, int reason)
{
    VMChangeStateEntry *e;

    for (e = vm_change_state_head.lh_first; e; e = e->entries.le_next) {
        e->cb(e->opaque, running, reason);
    }
}

void vm_start(void)
{
    if (!vm_running) {
        cpu_enable_ticks();
        vm_running = 1;
        vm_state_notify(1, 0);
        //qemu_rearm_alarm_timer(alarm_timer);
        resume_all_vcpus();
    }
}

/* reset/shutdown handler */

typedef struct QEMUResetEntry {
    QTAILQ_ENTRY(QEMUResetEntry) entry;
    QEMUResetHandler *func;
    void *opaque;
} QEMUResetEntry;

static QTAILQ_HEAD(reset_handlers, QEMUResetEntry) reset_handlers =
    QTAILQ_HEAD_INITIALIZER(reset_handlers);
static int reset_requested;
static int shutdown_requested, shutdown_signal = -1;
static pid_t shutdown_pid;
static int powerdown_requested;
int debug_requested;
static int vmstop_requested;

int qemu_shutdown_requested(void)
{
    int r = shutdown_requested;
    shutdown_requested = 0;
    return r;
}

int qemu_reset_requested(void)
{
    int r = reset_requested;
    reset_requested = 0;
    return r;
}

int qemu_powerdown_requested(void)
{
    int r = powerdown_requested;
    powerdown_requested = 0;
    return r;
}

static int qemu_debug_requested(void)
{
    int r = debug_requested;
    debug_requested = 0;
    return r;
}

static int qemu_vmstop_requested(void)
{
    int r = vmstop_requested;
    vmstop_requested = 0;
    return r;
}

void qemu_register_reset(QEMUResetHandler *func, int order, void *opaque)
{
    QEMUResetEntry **pre, *re;

    pre = &first_reset_entry;
    while (*pre != NULL && (*pre)->order >= order) {
        pre = &(*pre)->next;
    }
    re = qemu_mallocz(sizeof(QEMUResetEntry));
    re->func = func;
    re->opaque = opaque;
    re->order = order;
    re->next = NULL;
    *pre = re;
}

void qemu_system_reset(void)
{
    QEMUResetEntry *re;

    /* reset all devices */
    QTAILQ_FOREACH_SAFE(re, &reset_handlers, entry, nre) {
        re->func(re->opaque);
    }
}

void qemu_system_reset_request(void)
{
    if (no_reboot) {
        shutdown_requested = 1;
    } else {
        reset_requested = 1;
    }
    qemu_notify_event();
}

void qemu_system_killed(int signal, pid_t pid)
{
    shutdown_signal = signal;
    shutdown_pid = pid;
    qemu_system_shutdown_request();
}

void qemu_system_shutdown_request(void)
{
    shutdown_requested = 1;
    qemu_notify_event();
}

void qemu_system_powerdown_request(void)
{
    powerdown_requested = 1;
    qemu_notify_event();
}

#ifdef CONFIG_IOTHREAD
static void qemu_system_vmstop_request(int reason)
{
    vmstop_requested = reason;
    qemu_notify_event();
}
#endif

void main_loop_wait(int timeout)
{
    fd_set rfds, wfds, xfds;
    int ret, nfds;
    struct timeval tv;

    qemu_bh_update_timeout(&timeout);

    os_host_main_loop_wait(&timeout);


    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;

    /* poll any events */

    /* XXX: separate device handlers from system ones */
    nfds = -1;
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_ZERO(&xfds);
    qemu_iohandler_fill(&nfds, &rfds, &wfds, &xfds);
    if (slirp_is_inited()) {
        slirp_select_fill(&nfds, &rfds, &wfds, &xfds);
    }

    qemu_mutex_unlock_iothread();
    ret = select(nfds + 1, &rfds, &wfds, &xfds, &tv);
    qemu_mutex_lock_iothread();
    qemu_iohandler_poll(&rfds, &wfds, &xfds, ret);
    if (slirp_is_inited()) {
        if (ret < 0) {
            FD_ZERO(&rfds);
            FD_ZERO(&wfds);
            FD_ZERO(&xfds);
        }
        slirp_select_poll(&rfds, &wfds, &xfds);
    }

    qemu_run_all_timers();

    /* Check bottom-halves last in case any of the earlier events triggered
       them.  */
    qemu_bh_poll();

}

static int vm_can_run(void)
{
    if (powerdown_requested)
        return 0;
    if (reset_requested)
        return 0;
    if (shutdown_requested)
        return 0;
    if (debug_requested)
        return 0;
    return 1;
}

static void main_loop(void)
{
    int r;

#ifdef CONFIG_IOTHREAD
    qemu_system_ready = 1;
    qemu_cond_broadcast(&qemu_system_cond);
#endif

    for (;;) {
        do {
#ifdef CONFIG_PROFILER
            int64_t ti;
#endif
#ifndef CONFIG_IOTHREAD
            tcg_cpu_exec();
#endif
#ifdef CONFIG_PROFILER
            ti = profile_getclock();
#endif
            main_loop_wait(qemu_calculate_timeout());
#ifdef CONFIG_PROFILER
            dev_time += profile_getclock() - ti;
#endif
        } while (vm_can_run());

        if (qemu_debug_requested())
            vm_stop(EXCP_DEBUG);
        if (qemu_shutdown_requested()) {
            if (no_shutdown) {
                vm_stop(0);
                no_shutdown = 0;
            } else
                break;
        }
        if (qemu_reset_requested()) {
            pause_all_vcpus();
            qemu_system_reset();
            resume_all_vcpus();
        }
        if (qemu_powerdown_requested())
            qemu_system_powerdown();
        if ((r = qemu_vmstop_requested()))
            vm_stop(r);
    }
    pause_all_vcpus();
}

static void version(void)
{
    printf("QEMU PC emulator version " QEMU_VERSION QEMU_PKGVERSION ", Copyright (c) 2003-2008 Fabrice Bellard\n");
}

static void help(int exitcode)
{
    version();
    printf("usage: %s [options] [disk_image]\n"
           "\n"
           "'disk_image' is a raw hard image image for IDE hard disk 0\n"
           "\n"
#define DEF(option, opt_arg, opt_enum, opt_help)        \
           opt_help
#define DEFHEADING(text) stringify(text) "\n"
#include "qemu-options.def"
#undef DEF
#undef DEFHEADING
#undef GEN_DOCS
           "\n"
           "During emulation, the following keys are useful:\n"
           "ctrl-alt-f      toggle full screen\n"
           "ctrl-alt-n      switch to virtual console 'n'\n"
           "ctrl-alt        toggle mouse and keyboard grab\n"
           "\n"
           "When using -nographic, press 'ctrl-a h' to get some help.\n"
           ,
           "qemu",
           DEFAULT_RAM_SIZE,
#ifndef _WIN32
           DEFAULT_NETWORK_SCRIPT,
           DEFAULT_NETWORK_DOWN_SCRIPT,
#endif
           DEFAULT_GDBSTUB_PORT,
           "/tmp/qemu.log");
    exit(exitcode);
}

#define HAS_ARG 0x0001

enum {
#define DEF(option, opt_arg, opt_enum, opt_help)        \
    opt_enum,
#define DEFHEADING(text)
#include "qemu-options.def"
#undef DEF
#undef DEFHEADING
#undef GEN_DOCS
};

typedef struct QEMUOption {
    const char *name;
    int flags;
    int index;
} QEMUOption;

static const QEMUOption qemu_options[] = {
    { "h", 0, QEMU_OPTION_h },
#define DEF(option, opt_arg, opt_enum, opt_help)        \
    { option, opt_arg, opt_enum },
#define DEFHEADING(text)
#include "qemu-options.def"
#undef DEF
#undef DEFHEADING
#undef GEN_DOCS
    { NULL, 0, 0 },
};

static void select_vgahw (const char *p)
{
    const char *opts;

    cirrus_vga_enabled = 0;
    std_vga_enabled = 0;
    vmsvga_enabled = 0;
    xenfb_enabled = 0;
    if (strstart(p, "std", &opts)) {
        std_vga_enabled = 1;
    } else if (strstart(p, "cirrus", &opts)) {
        cirrus_vga_enabled = 1;
    } else if (strstart(p, "vmware", &opts)) {
        vmsvga_enabled = 1;
    } else if (strstart(p, "xenfb", &opts)) {
        xenfb_enabled = 1;
    } else if (!strstart(p, "none", &opts)) {
    invalid_vga:
        fprintf(stderr, "Unknown vga type: %s\n", p);
        exit(1);
    }
    while (*opts) {
        const char *nextopt;

        if (strstart(opts, ",retrace=", &nextopt)) {
            opts = nextopt;
            if (strstart(opts, "dumb", &nextopt))
                vga_retrace_method = VGA_RETRACE_DUMB;
            else if (strstart(opts, "precise", &nextopt))
                vga_retrace_method = VGA_RETRACE_PRECISE;
            else goto invalid_vga;
        } else goto invalid_vga;
        opts = nextopt;
    }
}

#define MAX_NET_CLIENTS 32

#ifdef _WIN32
/* Look for support files in the same directory as the executable.  */
static char *find_datadir(const char *argv0)
{
    char *p;
    char buf[MAX_PATH];
    DWORD len;

    len = GetModuleFileName(NULL, buf, sizeof(buf) - 1);
    if (len == 0) {
        return NULL;
    }

    buf[len] = 0;
    p = buf + len - 1;
    while (p != buf && *p != '\\')
        p--;
    *p = 0;
    if (access(buf, R_OK) == 0) {
        return qemu_strdup(buf);
    }
    return NULL;
}
#else /* !_WIN32 */

/* Find a likely location for support files using the location of the binary.
   For installed binaries this will be "$bindir/../share/qemu".  When
   running from the build tree this will be "$bindir/../pc-bios".  */
#define SHARE_SUFFIX "/share/qemu"
#define BUILD_SUFFIX "/pc-bios"
static char *find_datadir(const char *argv0)
{
    char *dir;
    char *p = NULL;
    char *res;
#ifdef PATH_MAX
    char buf[PATH_MAX];
#endif
    size_t max_len;

#if defined(__linux__)
    {
        int len;
        len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
        if (len > 0) {
            buf[len] = 0;
            p = buf;
        }
    }
#elif defined(__FreeBSD__)
    {
        int len;
        len = readlink("/proc/curproc/file", buf, sizeof(buf) - 1);
        if (len > 0) {
            buf[len] = 0;
            p = buf;
        }
    }
#endif
    /* If we don't have any way of figuring out the actual executable
       location then try argv[0].  */
    if (!p) {
#ifdef PATH_MAX
        p = buf;
#endif
        p = realpath(argv0, p);
        if (!p) {
            return NULL;
        }
    }
    dir = dirname(p);
    dir = dirname(dir);

    max_len = strlen(dir) +
        MAX(strlen(SHARE_SUFFIX), strlen(BUILD_SUFFIX)) + 1;
    res = qemu_mallocz(max_len);
    snprintf(res, max_len, "%s%s", dir, SHARE_SUFFIX);
    if (access(res, R_OK)) {
        snprintf(res, max_len, "%s%s", dir, BUILD_SUFFIX);
        if (access(res, R_OK)) {
            qemu_free(res);
            res = NULL;
        }
    }
#ifndef PATH_MAX
    free(p);
#endif
    return res;
}
#undef SHARE_SUFFIX
#undef BUILD_SUFFIX
#endif

char *qemu_find_file(int type, const char *name)
{
    int len;
    const char *subdir;
    char *buf;

    /* If name contains path separators then try it as a straight path.  */
    if ((strchr(name, '/') || strchr(name, '\\'))
        && access(name, R_OK) == 0) {
        return qemu_strdup(name);
    }
    switch (type) {
    case QEMU_FILE_TYPE_BIOS:
        subdir = "";
        break;
    case QEMU_FILE_TYPE_KEYMAP:
        subdir = "keymaps/";
        break;
    default:
        abort();
    }
    len = strlen(data_dir) + strlen(name) + strlen(subdir) + 2;
    buf = qemu_mallocz(len);
    snprintf(buf, len, "%s/%s%s", data_dir, subdir, name);
    if (access(buf, R_OK)) {
        qemu_free(buf);
        return NULL;
    }
    return buf;
}

int main(int argc, char **argv, char **envp)
{
    const char *gdbstub_dev = NULL;
    uint32_t boot_devices_bitmap = 0;
    int i;
    int snapshot, linux_boot, net_boot;
    const char *icount_option = NULL;
    const char *initrd_filename;
    const char *kernel_filename, *kernel_cmdline;
    const char *boot_devices = "";
    DisplayState *ds;
    DisplayChangeListener *dcl;
    int cyls, heads, secs, translation;
    const char *net_clients[MAX_NET_CLIENTS];
    int nb_net_clients;
    const char *bt_opts[MAX_BT_CMDLINE];
    int nb_bt_opts;
    int hda_index;
    int optind;
    const char *r, *optarg;
    CharDriverState *monitor_hd = NULL;
    const char *monitor_device;
    const char *serial_devices[MAX_SERIAL_PORTS];
    int serial_device_index;
    const char *parallel_devices[MAX_PARALLEL_PORTS];
    int parallel_device_index;
    const char *virtio_consoles[MAX_VIRTIO_CONSOLES];
    int virtio_console_index;
    const char *loadvm = NULL;
    QEMUMachine *machine;
    const char *cpu_model;
    const char *usb_devices[MAX_USB_CMDLINE];
    int usb_devices_index;
    int tb_size;
    const char *pid_file = NULL;
    const char *incoming = NULL;
    CPUState *env;
    int show_vnc_port = 0;

    init_clocks();

    qemu_cache_utils_init(envp);

    QLIST_INIT (&vm_change_state_head);
    os_setup_early_signal_handling();

    module_call_init(MODULE_INIT_MACHINE);
    machine = find_default_machine();
    cpu_model = NULL;
    initrd_filename = NULL;
    ram_size = 0;
    snapshot = 0;
    kernel_filename = NULL;
    kernel_cmdline = "";
    cyls = heads = secs = 0;
    translation = BIOS_ATA_TRANSLATION_AUTO;
    monitor_device = "vc:80Cx24C";

    serial_devices[0] = "vc:80Cx24C";
    for(i = 1; i < MAX_SERIAL_PORTS; i++)
        serial_devices[i] = NULL;
    serial_device_index = 0;

    parallel_devices[0] = "vc:80Cx24C";
    for(i = 1; i < MAX_PARALLEL_PORTS; i++)
        parallel_devices[i] = NULL;
    parallel_device_index = 0;

    for(i = 0; i < MAX_VIRTIO_CONSOLES; i++)
        virtio_consoles[i] = NULL;
    virtio_console_index = 0;

    for (i = 0; i < MAX_NODES; i++) {
        node_mem[i] = 0;
        node_cpumask[i] = 0;
    }

    usb_devices_index = 0;

    nb_net_clients = 0;
    nb_bt_opts = 0;
    nb_drives = 0;
    nb_drives_opt = 0;
    nb_numa_nodes = 0;
    hda_index = -1;

    nb_nics = 0;

    tb_size = 0;
    autostart= 1;

    register_watchdogs();

    optind = 1;
    for(;;) {
        if (optind >= argc)
            break;
        r = argv[optind];
        if (r[0] != '-') {
	    hda_index = drive_add(argv[optind++], HD_ALIAS, 0);
        } else {
            const QEMUOption *popt;

            optind++;
            /* Treat --foo the same as -foo.  */
            if (r[1] == '-')
                r++;
            popt = qemu_options;
            for(;;) {
                if (!popt->name) {
                    fprintf(stderr, "%s: invalid option -- '%s'\n",
                            argv[0], r);
                    exit(1);
                }
                if (!strcmp(popt->name, r + 1))
                    break;
                popt++;
            }
            if (popt->flags & HAS_ARG) {
                if (optind >= argc) {
                    fprintf(stderr, "%s: option '%s' requires an argument\n",
                            argv[0], r);
                    exit(1);
                }
                optarg = argv[optind++];
            } else {
                optarg = NULL;
            }

            switch(popt->index) {
            case QEMU_OPTION_M:
                machine = find_machine(optarg);
                if (!machine) {
                    QEMUMachine *m;
                    printf("Supported machines are:\n");
                    for(m = first_machine; m != NULL; m = m->next) {
                        printf("%-10s %s%s\n",
                               m->name, m->desc,
                               m->is_default ? " (default)" : "");
                    }
                    exit(*optarg != '?');
                }
                break;
            case QEMU_OPTION_cpu:
                /* hw initialization will check this */
                if (*optarg == '?') {
/* XXX: implement xxx_cpu_list for targets that still miss it */
#if defined(cpu_list)
                    cpu_list(stdout, &fprintf);
#endif
                    exit(0);
                } else {
                    cpu_model = optarg;
                }
                break;
            case QEMU_OPTION_initrd:
                initrd_filename = optarg;
                break;
            case QEMU_OPTION_hda:
                if (cyls == 0)
                    hda_index = drive_add(optarg, HD_ALIAS, 0);
                else
                    hda_index = drive_add(optarg, HD_ALIAS
			     ",cyls=%d,heads=%d,secs=%d%s",
                             0, cyls, heads, secs,
                             translation == BIOS_ATA_TRANSLATION_LBA ?
                                 ",trans=lba" :
                             translation == BIOS_ATA_TRANSLATION_NONE ?
                                 ",trans=none" : "");
                 break;
            case QEMU_OPTION_hdb:
            case QEMU_OPTION_hdc:
            case QEMU_OPTION_hdd:
                drive_add(optarg, HD_ALIAS, popt->index - QEMU_OPTION_hda);
                break;
            case QEMU_OPTION_drive:
                drive_add(NULL, "%s", optarg);
	        break;
            case QEMU_OPTION_mtdblock:
                drive_add(optarg, MTD_ALIAS);
                break;
            case QEMU_OPTION_sd:
                drive_add(optarg, SD_ALIAS);
                break;
            case QEMU_OPTION_pflash:
                drive_add(optarg, PFLASH_ALIAS);
                break;
            case QEMU_OPTION_snapshot:
                snapshot = 1;
                break;
            case QEMU_OPTION_hdachs:
                {
                    const char *p;
                    p = optarg;
                    cyls = strtol(p, (char **)&p, 0);
                    if (cyls < 1 || cyls > 16383)
                        goto chs_fail;
                    if (*p != ',')
                        goto chs_fail;
                    p++;
                    heads = strtol(p, (char **)&p, 0);
                    if (heads < 1 || heads > 16)
                        goto chs_fail;
                    if (*p != ',')
                        goto chs_fail;
                    p++;
                    secs = strtol(p, (char **)&p, 0);
                    if (secs < 1 || secs > 63)
                        goto chs_fail;
                    if (*p == ',') {
                        p++;
                        if (!strcmp(p, "none"))
                            translation = BIOS_ATA_TRANSLATION_NONE;
                        else if (!strcmp(p, "lba"))
                            translation = BIOS_ATA_TRANSLATION_LBA;
                        else if (!strcmp(p, "auto"))
                            translation = BIOS_ATA_TRANSLATION_AUTO;
                        else
                            goto chs_fail;
                    } else if (*p != '\0') {
                    chs_fail:
                        fprintf(stderr, "qemu: invalid physical CHS format\n");
                        exit(1);
                    }
		    if (hda_index != -1)
                        snprintf(drives_opt[hda_index].opt,
                                 sizeof(drives_opt[hda_index].opt),
                                 HD_ALIAS ",cyls=%d,heads=%d,secs=%d%s",
                                 0, cyls, heads, secs,
			         translation == BIOS_ATA_TRANSLATION_LBA ?
			     	    ",trans=lba" :
			         translation == BIOS_ATA_TRANSLATION_NONE ?
			             ",trans=none" : "");
                }
                break;
            case QEMU_OPTION_numa:
                if (nb_numa_nodes >= MAX_NODES) {
                    fprintf(stderr, "qemu: too many NUMA nodes\n");
                    exit(1);
                }
                numa_add(optarg);
                break;
            case QEMU_OPTION_nographic:
                display_type = DT_NOGRAPHIC;
                break;
#ifdef CONFIG_CURSES
            case QEMU_OPTION_curses:
                display_type = DT_CURSES;
                break;
#endif
            case QEMU_OPTION_portrait:
                graphic_rotate = 1;
                break;
            case QEMU_OPTION_kernel:
                kernel_filename = optarg;
                break;
            case QEMU_OPTION_append:
                kernel_cmdline = optarg;
                break;
            case QEMU_OPTION_cdrom:
                drive_add(optarg, CDROM_ALIAS);
                break;
            case QEMU_OPTION_boot:
                boot_devices = optarg;
                /* We just do some generic consistency checks */
                {
                    /* Could easily be extended to 64 devices if needed */
                    const char *p;

                    boot_devices_bitmap = 0;
                    for (p = boot_devices; *p != '\0'; p++) {
                        /* Allowed boot devices are:
                         * a b     : floppy disk drives
                         * c ... f : IDE disk drives
                         * g ... m : machine implementation dependant drives
                         * n ... p : network devices
                         * It's up to each machine implementation to check
                         * if the given boot devices match the actual hardware
                         * implementation and firmware features.
                         */
                        if (*p < 'a' || *p > 'q') {
                            fprintf(stderr, "Invalid boot device '%c'\n", *p);
                            exit(1);
                        }
                        if (boot_devices_bitmap & (1 << (*p - 'a'))) {
                            fprintf(stderr,
                                    "Boot device '%c' was given twice\n",*p);
                            exit(1);
                        }
                        boot_devices_bitmap |= 1 << (*p - 'a');
                    }
                }
                break;
            case QEMU_OPTION_fda:
            case QEMU_OPTION_fdb:
                drive_add(optarg, FD_ALIAS, popt->index - QEMU_OPTION_fda);
                break;
#ifdef TARGET_I386
            case QEMU_OPTION_no_fd_bootchk:
                fd_bootchk = 0;
                break;
#endif
            case QEMU_OPTION_net:
                if (nb_net_clients >= MAX_NET_CLIENTS) {
                    fprintf(stderr, "qemu: too many network clients\n");
                    exit(1);
                }
                net_clients[nb_net_clients] = optarg;
                nb_net_clients++;
                break;
#ifdef CONFIG_SLIRP
            case QEMU_OPTION_tftp:
		tftp_prefix = optarg;
                break;
            case QEMU_OPTION_bootp:
                bootp_filename = optarg;
                break;
            case QEMU_OPTION_redir:
                net_slirp_redir(NULL, optarg, NULL);
                break;
#endif
            case QEMU_OPTION_bt:
                if (nb_bt_opts >= MAX_BT_CMDLINE) {
                    fprintf(stderr, "qemu: too many bluetooth options\n");
                    exit(1);
                }
                bt_opts[nb_bt_opts++] = optarg;
                break;
#ifdef HAS_AUDIO
            case QEMU_OPTION_audio_help:
                AUD_help ();
                exit (0);
                break;
            case QEMU_OPTION_soundhw:
                select_soundhw (optarg);
                break;
#endif
            case QEMU_OPTION_h:
                help(0);
                break;
            case QEMU_OPTION_version:
                version();
                exit(0);
                break;
            case QEMU_OPTION_m: {
                uint64_t value;
                char *ptr;

                value = strtoul(optarg, &ptr, 10);
                switch (*ptr) {
                case 0: case 'M': case 'm':
                    value <<= 20;
                    break;
                case 'G': case 'g':
                    value <<= 30;
                    break;
                default:
                    fprintf(stderr, "qemu: invalid ram size: %s\n", optarg);
                    exit(1);
                }

                /* On 32-bit hosts, QEMU is limited by virtual address space */
                if (value > (2047 << 20) && HOST_LONG_BITS == 32) {
                    fprintf(stderr, "qemu: at most 2047 MB RAM can be simulated\n");
                    exit(1);
                }
                if (value != (uint64_t)(ram_addr_t)value) {
                    fprintf(stderr, "qemu: ram size too large\n");
                    exit(1);
                }
                ram_size = value;
                break;
            }
            case QEMU_OPTION_d:
                {
                    int mask;
                    const CPULogItem *item;

                    mask = cpu_str_to_log_mask(optarg);
                    if (!mask) {
                        printf("Log items (comma separated):\n");
                    for(item = cpu_log_items; item->mask != 0; item++) {
                        printf("%-10s %s\n", item->name, item->help);
                    }
                    exit(1);
                    }
                    cpu_set_log(mask);
                }
                break;
            case QEMU_OPTION_s:
                gdbstub_dev = "tcp::" DEFAULT_GDBSTUB_PORT;
                break;
            case QEMU_OPTION_gdb:
                gdbstub_dev = optarg;
                break;
            case QEMU_OPTION_L:
                data_dir = optarg;
                break;
            case QEMU_OPTION_bios:
                bios_name = optarg;
                break;
            case QEMU_OPTION_singlestep:
                singlestep = 1;
                break;
            case QEMU_OPTION_S:
                autostart = 0;
                break;
#ifndef _WIN32
	    case QEMU_OPTION_k:
		keyboard_layout = optarg;
		break;
#endif
            case QEMU_OPTION_localtime:
                rtc_utc = 0;
                break;
            case QEMU_OPTION_vga:
                select_vgahw (optarg);
                break;
#if defined(TARGET_PPC) || defined(TARGET_SPARC)
            case QEMU_OPTION_g:
                {
                    const char *p;
                    int w, h, depth;
                    p = optarg;
                    w = strtol(p, (char **)&p, 10);
                    if (w <= 0) {
                    graphic_error:
                        fprintf(stderr, "qemu: invalid resolution or depth\n");
                        exit(1);
                    }
                    if (*p != 'x')
                        goto graphic_error;
                    p++;
                    h = strtol(p, (char **)&p, 10);
                    if (h <= 0)
                        goto graphic_error;
                    if (*p == 'x') {
                        p++;
                        depth = strtol(p, (char **)&p, 10);
                        if (depth != 8 && depth != 15 && depth != 16 &&
                            depth != 24 && depth != 32)
                            goto graphic_error;
                    } else if (*p == '\0') {
                        depth = graphic_depth;
                    } else {
                        goto graphic_error;
                    }

                    graphic_width = w;
                    graphic_height = h;
                    graphic_depth = depth;
                }
                break;
#endif
            case QEMU_OPTION_echr:
                {
                    char *r;
                    term_escape_char = strtol(optarg, &r, 0);
                    if (r == optarg)
                        printf("Bad argument to echr\n");
                    break;
                }
            case QEMU_OPTION_monitor:
                monitor_device = optarg;
                break;
            case QEMU_OPTION_serial:
                if (serial_device_index >= MAX_SERIAL_PORTS) {
                    fprintf(stderr, "qemu: too many serial ports\n");
                    exit(1);
                }
                serial_devices[serial_device_index] = optarg;
                serial_device_index++;
                break;
            case QEMU_OPTION_watchdog:
                i = select_watchdog(optarg);
                if (i > 0)
                    exit (i == 1 ? 1 : 0);
                break;
            case QEMU_OPTION_watchdog_action:
                if (select_watchdog_action(optarg) == -1) {
                    fprintf(stderr, "Unknown -watchdog-action parameter\n");
                    exit(1);
                }
                break;
            case QEMU_OPTION_virtiocon:
                if (virtio_console_index >= MAX_VIRTIO_CONSOLES) {
                    fprintf(stderr, "qemu: too many virtio consoles\n");
                    exit(1);
                }
                virtio_consoles[virtio_console_index] = optarg;
                virtio_console_index++;
                break;
            case QEMU_OPTION_parallel:
                if (parallel_device_index >= MAX_PARALLEL_PORTS) {
                    fprintf(stderr, "qemu: too many parallel ports\n");
                    exit(1);
                }
                parallel_devices[parallel_device_index] = optarg;
                parallel_device_index++;
                break;
	    case QEMU_OPTION_loadvm:
		loadvm = optarg;
		break;
            case QEMU_OPTION_full_screen:
                full_screen = 1;
                break;
#ifdef CONFIG_SDL
            case QEMU_OPTION_no_frame:
                no_frame = 1;
                break;
            case QEMU_OPTION_alt_grab:
                alt_grab = 1;
                break;
            case QEMU_OPTION_no_quit:
                no_quit = 1;
                break;
            case QEMU_OPTION_sdl:
                display_type = DT_SDL;
                break;
#endif
            case QEMU_OPTION_pidfile:
                pid_file = optarg;
                break;
#ifdef TARGET_I386
            case QEMU_OPTION_win2k_hack:
                win2k_install_hack = 1;
                break;
            case QEMU_OPTION_rtc_td_hack:
                rtc_td_hack = 1;
                break;
            case QEMU_OPTION_acpitable:
                if(acpi_table_add(optarg) < 0) {
                    fprintf(stderr, "Wrong acpi table provided\n");
                    exit(1);
                }
                break;
            case QEMU_OPTION_smbios:
                if(smbios_entry_add(optarg) < 0) {
                    fprintf(stderr, "Wrong smbios provided\n");
                    exit(1);
                }
                break;
#endif
#ifdef CONFIG_KQEMU
            case QEMU_OPTION_no_kqemu:
                kqemu_allowed = 0;
                break;
            case QEMU_OPTION_kernel_kqemu:
                kqemu_allowed = 2;
                break;
#endif
#ifdef CONFIG_KVM
            case QEMU_OPTION_enable_kvm:
                kvm_allowed = 1;
#ifdef CONFIG_KQEMU
                kqemu_allowed = 0;
#endif
                break;
#endif
            case QEMU_OPTION_usb:
                usb_enabled = 1;
                break;
            case QEMU_OPTION_usbdevice:
                usb_enabled = 1;
                if (usb_devices_index >= MAX_USB_CMDLINE) {
                    fprintf(stderr, "Too many USB devices\n");
                    exit(1);
                }
                usb_devices[usb_devices_index] = optarg;
                usb_devices_index++;
                break;
            case QEMU_OPTION_smp:
                smp_cpus = atoi(optarg);
                if (smp_cpus < 1) {
                    fprintf(stderr, "Invalid number of CPUs\n");
                    exit(1);
                }
                break;
	    case QEMU_OPTION_vnc:
                display_type = DT_VNC;
		vnc_display = optarg;
		break;
#ifdef TARGET_I386
            case QEMU_OPTION_no_acpi:
                acpi_enabled = 0;
                break;
            case QEMU_OPTION_no_hpet:
                no_hpet = 1;
                break;
            case QEMU_OPTION_no_virtio_balloon:
                no_virtio_balloon = 1;
                break;
#endif
            case QEMU_OPTION_no_reboot:
                no_reboot = 1;
                break;
            case QEMU_OPTION_no_shutdown:
                no_shutdown = 1;
                break;
            case QEMU_OPTION_show_cursor:
                cursor_hide = 0;
                break;
            case QEMU_OPTION_uuid:
                if(qemu_uuid_parse(optarg, qemu_uuid) < 0) {
                    fprintf(stderr, "Fail to parse UUID string."
                            " Wrong format.\n");
                    exit(1);
                }
                break;
	    case QEMU_OPTION_option_rom:
		if (nb_option_roms >= MAX_OPTION_ROMS) {
		    fprintf(stderr, "Too many option ROMs\n");
		    exit(1);
		}
		option_rom[nb_option_roms] = optarg;
		nb_option_roms++;
		break;
#if defined(TARGET_ARM) || defined(TARGET_M68K)
            case QEMU_OPTION_semihosting:
                semihosting_enabled = 1;
                break;
#endif
            case QEMU_OPTION_name:
                qemu_name = optarg;
                break;
#if defined(TARGET_SPARC) || defined(TARGET_PPC)
            case QEMU_OPTION_prom_env:
                if (nb_prom_envs >= MAX_PROM_ENVS) {
                    fprintf(stderr, "Too many prom variables\n");
                    exit(1);
                }
                prom_envs[nb_prom_envs] = optarg;
                nb_prom_envs++;
                break;
#endif
#ifdef TARGET_ARM
            case QEMU_OPTION_old_param:
                old_param = 1;
                break;
#endif
            case QEMU_OPTION_clock:
                configure_alarms(optarg);
                break;
            case QEMU_OPTION_startdate:
                {
                    struct tm tm;
                    time_t rtc_start_date = 0;
                    if (!strcmp(optarg, "now")) {
                        rtc_date_offset = -1;
                    } else {
                        if (sscanf(optarg, "%d-%d-%dT%d:%d:%d",
                               &tm.tm_year,
                               &tm.tm_mon,
                               &tm.tm_mday,
                               &tm.tm_hour,
                               &tm.tm_min,
                               &tm.tm_sec) == 6) {
                            /* OK */
                        } else if (sscanf(optarg, "%d-%d-%d",
                                          &tm.tm_year,
                                          &tm.tm_mon,
                                          &tm.tm_mday) == 3) {
                            tm.tm_hour = 0;
                            tm.tm_min = 0;
                            tm.tm_sec = 0;
                        } else {
                            goto date_fail;
                        }
                        tm.tm_year -= 1900;
                        tm.tm_mon--;
                        rtc_start_date = mktimegm(&tm);
                        if (rtc_start_date == -1) {
                        date_fail:
                            fprintf(stderr, "Invalid date format. Valid format are:\n"
                                    "'now' or '2006-06-17T16:01:21' or '2006-06-17'\n");
                            exit(1);
                        }
                        rtc_date_offset = time(NULL) - rtc_start_date;
                    }
                }
                break;
            case QEMU_OPTION_tb_size:
                tb_size = strtol(optarg, NULL, 0);
                if (tb_size < 0)
                    tb_size = 0;
                break;
            case QEMU_OPTION_icount:
                icount_option = optarg;
                break;
            case QEMU_OPTION_incoming:
                incoming = optarg;
                break;
#ifdef CONFIG_XEN
            case QEMU_OPTION_xen_domid:
                xen_domid = atoi(optarg);
                break;
            case QEMU_OPTION_xen_create:
                xen_mode = XEN_CREATE;
                break;
            case QEMU_OPTION_xen_attach:
                xen_mode = XEN_ATTACH;
                break;
#endif
            }
        }
    }

    /* If no data_dir is specified then try to find it relative to the
       executable path.  */
    if (!data_dir) {
        data_dir = find_datadir(argv[0]);
    }
    /* If all else fails use the install patch specified when building.  */
    if (!data_dir) {
        data_dir = CONFIG_QEMU_SHAREDIR;
    }

    if (pid_file && qemu_create_pidfile(pid_file) != 0) {
        os_pidfile_error();
        exit(1);
    }

#if defined(CONFIG_KVM) && defined(CONFIG_KQEMU)
    if (kvm_allowed && kqemu_allowed) {
        fprintf(stderr,
                "You can not enable both KVM and kqemu at the same time\n");
        exit(1);
    }
#endif

    machine->max_cpus = machine->max_cpus ?: 1; /* Default to UP */
    if (smp_cpus > machine->max_cpus) {
        fprintf(stderr, "Number of SMP cpus requested (%d), exceeds max cpus "
                "supported by machine `%s' (%d)\n", smp_cpus,  machine->name,
                machine->max_cpus);
        exit(1);
    }

    if (display_type == DT_NOGRAPHIC) {
       if (serial_device_index == 0)
           serial_devices[0] = "stdio";
       if (parallel_device_index == 0)
           parallel_devices[0] = "null";
       if (strncmp(monitor_device, "vc", 2) == 0)
           monitor_device = "stdio";
    }

#ifdef CONFIG_KQEMU
    if (smp_cpus > 1)
        kqemu_allowed = 0;
#endif
    if (qemu_init_main_loop()) {
        fprintf(stderr, "qemu_init_main_loop failed\n");
        exit(1);
    }
    linux_boot = (kernel_filename != NULL);
    net_boot = (boot_devices_bitmap >> ('n' - 'a')) & 0xF;

    if (!linux_boot && *kernel_cmdline != '\0') {
        fprintf(stderr, "-append only allowed with -kernel option\n");
        exit(1);
    }

    if (!linux_boot && initrd_filename != NULL) {
        fprintf(stderr, "-initrd only allowed with -kernel option\n");
        exit(1);
    }

    /* boot to floppy or the default cd if no hard disk defined yet */
    if (!boot_devices[0]) {
        boot_devices = "cad";
    }
    os_set_line_buffering();

    if (init_timer_alarm() < 0) {
        fprintf(stderr, "could not initialize alarm timer\n");
        exit(1);
    }
    configure_icount(icount_option);

#ifdef _WIN32
    socket_init();
#endif

    /* init network clients */
    if (nb_net_clients == 0) {
        /* if no clients, we use a default config */
        net_clients[nb_net_clients++] = "nic";
#ifdef CONFIG_SLIRP
        net_clients[nb_net_clients++] = "user";
#endif
    }

    for(i = 0;i < nb_net_clients; i++) {
        if (net_client_parse(net_clients[i]) < 0)
            exit(1);
    }
    net_client_check();

#ifdef TARGET_I386
    /* XXX: this should be moved in the PC machine instantiation code */
    if (net_boot != 0) {
        int netroms = 0;
	for (i = 0; i < nb_nics && i < 4; i++) {
	    const char *model = nd_table[i].model;
	    char buf[1024];
            char *filename;
            if (net_boot & (1 << i)) {
                if (model == NULL)
                    model = "ne2k_pci";
                snprintf(buf, sizeof(buf), "pxe-%s.bin", model);
                filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, buf);
                if (filename && get_image_size(filename) > 0) {
                    if (nb_option_roms >= MAX_OPTION_ROMS) {
                        fprintf(stderr, "Too many option ROMs\n");
                        exit(1);
                    }
                    option_rom[nb_option_roms] = qemu_strdup(buf);
                    nb_option_roms++;
                    netroms++;
                }
                if (filename) {
                    qemu_free(filename);
                }
            }
	}
	if (netroms == 0) {
	    fprintf(stderr, "No valid PXE rom found for network device\n");
	    exit(1);
	}
    }
#endif

    /* init the bluetooth world */
    for (i = 0; i < nb_bt_opts; i++)
        if (bt_parse(bt_opts[i]))
            exit(1);

    /* init the memory */
    if (ram_size == 0)
        ram_size = DEFAULT_RAM_SIZE * 1024 * 1024;

#ifdef CONFIG_KQEMU
    /* FIXME: This is a nasty hack because kqemu can't cope with dynamic
       guest ram allocation.  It needs to go away.  */
    if (kqemu_allowed) {
        kqemu_phys_ram_size = ram_size + 8 * 1024 * 1024 + 4 * 1024 * 1024;
        kqemu_phys_ram_base = qemu_vmalloc(kqemu_phys_ram_size);
        if (!kqemu_phys_ram_base) {
            fprintf(stderr, "Could not allocate physical memory\n");
            exit(1);
        }
    }
#endif

    /* init the dynamic translator */
    cpu_exec_init_all(tb_size * 1024 * 1024);

    bdrv_init();

    /* we always create the cdrom drive, even if no disk is there */

    if (nb_drives_opt < MAX_DRIVES)
        drive_add(NULL, CDROM_ALIAS);

    /* we always create at least one floppy */

    if (nb_drives_opt < MAX_DRIVES)
        drive_add(NULL, FD_ALIAS, 0);

    /* we always create one sd slot, even if no card is in it */

    if (nb_drives_opt < MAX_DRIVES) {
        drive_add(NULL, SD_ALIAS);
    }

    /* open the virtual block devices */
    if (snapshot)
        qemu_opts_foreach(qemu_find_opts("drive"), drive_enable_snapshot, NULL, 0);
    if (qemu_opts_foreach(qemu_find_opts("drive"), drive_init_func, &machine->use_scsi, 1) != 0)
        exit(1);

    //register_savevm("timer", 0, 2, timer_save, timer_load, NULL);
    register_savevm_live("ram", 0, 3, ram_save_live, NULL, ram_load, NULL);

    /* must be after terminal init, SDL library changes signal handlers */
    os_setup_signal_handling();

    /* Maintain compatibility with multiple stdio monitors */
    if (!strcmp(monitor_device,"stdio")) {
        for (i = 0; i < MAX_SERIAL_PORTS; i++) {
            const char *devname = serial_devices[i];
            if (devname && !strcmp(devname,"mon:stdio")) {
                monitor_device = NULL;
                break;
            } else if (devname && !strcmp(devname,"stdio")) {
                monitor_device = NULL;
                serial_devices[i] = "mon:stdio";
                break;
            }
        }
    }

    if (nb_numa_nodes > 0) {
        int i;

        if (nb_numa_nodes > smp_cpus) {
            nb_numa_nodes = smp_cpus;
        }

        /* If no memory size if given for any node, assume the default case
         * and distribute the available memory equally across all nodes
         */
        for (i = 0; i < nb_numa_nodes; i++) {
            if (node_mem[i] != 0)
                break;
        }
        if (i == nb_numa_nodes) {
            uint64_t usedmem = 0;

            /* On Linux, the each node's border has to be 8MB aligned,
             * the final node gets the rest.
             */
            for (i = 0; i < nb_numa_nodes - 1; i++) {
                node_mem[i] = (ram_size / nb_numa_nodes) & ~((1 << 23UL) - 1);
                usedmem += node_mem[i];
            }
            node_mem[i] = ram_size - usedmem;
        }

        for (i = 0; i < nb_numa_nodes; i++) {
            if (node_cpumask[i] != 0)
                break;
        }
        /* assigning the VCPUs round-robin is easier to implement, guest OSes
         * must cope with this anyway, because there are BIOSes out there in
         * real machines which also use this scheme.
         */
        if (i == nb_numa_nodes) {
            for (i = 0; i < smp_cpus; i++) {
                node_cpumask[i % nb_numa_nodes] |= 1 << i;
            }
        }
    }

    if (kvm_enabled()) {
        int ret;

        ret = kvm_init(smp_cpus);
        if (ret < 0) {
            fprintf(stderr, "failed to initialize KVM\n");
            exit(1);
        }
    }

    if (monitor_device) {
        monitor_hd = qemu_chr_open("monitor", monitor_device, NULL);
        if (!monitor_hd) {
            fprintf(stderr, "qemu: could not open monitor device '%s'\n", monitor_device);
            exit(1);
        }
    }

    for(i = 0; i < MAX_SERIAL_PORTS; i++) {
        const char *devname = serial_devices[i];
        if (devname && strcmp(devname, "none")) {
            char label[32];
            snprintf(label, sizeof(label), "serial%d", i);
            serial_hds[i] = qemu_chr_open(label, devname, NULL);
            if (!serial_hds[i]) {
                fprintf(stderr, "qemu: could not open serial device '%s'\n",
                        devname);
                exit(1);
            }
        }
    }

    for(i = 0; i < MAX_PARALLEL_PORTS; i++) {
        const char *devname = parallel_devices[i];
        if (devname && strcmp(devname, "none")) {
            char label[32];
            snprintf(label, sizeof(label), "parallel%d", i);
            parallel_hds[i] = qemu_chr_open(label, devname, NULL);
            if (!parallel_hds[i]) {
                fprintf(stderr, "qemu: could not open parallel device '%s'\n",
                        devname);
                exit(1);
            }
        }
    }

    for(i = 0; i < MAX_VIRTIO_CONSOLES; i++) {
        const char *devname = virtio_consoles[i];
        if (devname && strcmp(devname, "none")) {
            char label[32];
            snprintf(label, sizeof(label), "virtcon%d", i);
            virtcon_hds[i] = qemu_chr_open(label, devname, NULL);
            if (!virtcon_hds[i]) {
                fprintf(stderr, "qemu: could not open virtio console '%s'\n",
                        devname);
                exit(1);
            }
        }
    }

    module_call_init(MODULE_INIT_DEVICE);

    machine->init(ram_size, boot_devices,
                  kernel_filename, kernel_cmdline, initrd_filename, cpu_model);


    for (env = first_cpu; env != NULL; env = env->next_cpu) {
        for (i = 0; i < nb_numa_nodes; i++) {
            if (node_cpumask[i] & (1 << env->cpu_index)) {
                env->numa_node = i;
            }
        }
    }

    current_machine = machine;

    /* Set KVM's vcpu state to qemu's initial CPUState. */
    if (kvm_enabled()) {
        int ret;

        ret = kvm_sync_vcpus();
        if (ret < 0) {
            fprintf(stderr, "failed to initialize vcpus\n");
            exit(1);
        }
    }

    /* init USB devices */
    if (usb_enabled) {
        for(i = 0; i < usb_devices_index; i++) {
            if (usb_device_add(usb_devices[i], 0) < 0) {
                fprintf(stderr, "Warning: could not add USB device %s\n",
                        usb_devices[i]);
            }
        }
    }

    if (!display_state)
        dumb_display_init();
    /* just use the first displaystate for the moment */
    ds = display_state;

    if (display_type == DT_DEFAULT) {
#if defined(CONFIG_SDL) || defined(CONFIG_COCOA)
        display_type = DT_SDL;
#else
        display_type = DT_VNC;
        vnc_display = "localhost:0,to=99";
        show_vnc_port = 1;
#endif
    }


    switch (display_type) {
    case DT_NOGRAPHIC:
        break;
#if defined(CONFIG_CURSES)
    case DT_CURSES:
        curses_display_init(ds, full_screen);
        break;
#endif
#if defined(CONFIG_SDL)
    case DT_SDL:
        sdl_display_init(ds, full_screen, no_frame);
        break;
#elif defined(CONFIG_COCOA)
    case DT_SDL:
        cocoa_display_init(ds, full_screen);
        break;
#endif
    case DT_VNC:
        vnc_display_init(ds);
        if (vnc_display_open(ds, vnc_display) < 0)
            exit(1);

        if (show_vnc_port) {
            printf("VNC server running on `%s'\n", vnc_display_local_addr(ds));
        }
        break;
    default:
        break;
    }
    dpy_resize(ds);

    dcl = ds->listeners;
    while (dcl != NULL) {
        if (dcl->dpy_refresh != NULL) {
            ds->gui_timer = qemu_new_timer_ms(rt_clock, gui_update, ds);
            qemu_mod_timer(ds->gui_timer, qemu_get_clock_ms(rt_clock));
        }
        dcl = dcl->next;
    }

    if (display_type == DT_NOGRAPHIC || display_type == DT_VNC) {
        nographic_timer = qemu_new_timer_ms(rt_clock, nographic_update, NULL);
        qemu_mod_timer(nographic_timer, qemu_get_clock_ms(rt_clock));
    }

    text_consoles_set_display(display_state);
    qemu_chr_initial_reset();

    if (monitor_device && monitor_hd)
        monitor_init(monitor_hd, MONITOR_USE_READLINE | MONITOR_IS_DEFAULT);

    for(i = 0; i < MAX_SERIAL_PORTS; i++) {
        const char *devname = serial_devices[i];
        if (devname && strcmp(devname, "none")) {
            if (strstart(devname, "vc", 0))
                qemu_chr_printf(serial_hds[i], "serial%d console\r\n", i);
        }
    }

    for(i = 0; i < MAX_PARALLEL_PORTS; i++) {
        const char *devname = parallel_devices[i];
        if (devname && strcmp(devname, "none")) {
            if (strstart(devname, "vc", 0))
                qemu_chr_printf(parallel_hds[i], "parallel%d console\r\n", i);
        }
    }

    for(i = 0; i < MAX_VIRTIO_CONSOLES; i++) {
        const char *devname = virtio_consoles[i];
        if (virtcon_hds[i] && devname) {
            if (strstart(devname, "vc", 0))
                qemu_chr_printf(virtcon_hds[i], "virtio console%d\r\n", i);
        }
    }

    if (gdbstub_dev && gdbserver_start(gdbstub_dev) < 0) {
        fprintf(stderr, "qemu: could not open gdbserver on device '%s'\n",
                gdbstub_dev);
        exit(1);
    }

    if (loadvm)
        do_loadvm(cur_mon, loadvm);

    if (incoming) {
        autostart = 0; /* fixme how to deal with -daemonize */
        qemu_start_incoming_migration(incoming);
    }

    if (autostart)
        vm_start();

    os_setup_post();

    main_loop();
    quit_timers();
    net_cleanup();

    return 0;
}
