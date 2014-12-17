/* AUTOMATICALLY GENERATED, DO NOT MODIFY */

/*
 * schema-defined QAPI types
 *
 * Copyright IBM, Corp. 2011
 *
 * Authors:
 *  Anthony Liguori   <aliguori@us.ibm.com>
 *
 * This work is licensed under the terms of the GNU LGPL, version 2.1 or later.
 * See the COPYING.LIB file in the top-level directory.
 *
 */

#ifndef QAPI_TYPES_H
#define QAPI_TYPES_H

#include "qapi/qapi-types-core.h"

typedef struct NameInfo NameInfo;

typedef struct NameInfoList
{
    NameInfo *value;
    struct NameInfoList *next;
} NameInfoList;

typedef struct VersionInfo VersionInfo;

typedef struct VersionInfoList
{
    VersionInfo *value;
    struct VersionInfoList *next;
} VersionInfoList;

typedef struct KvmInfo KvmInfo;

typedef struct KvmInfoList
{
    KvmInfo *value;
    struct KvmInfoList *next;
} KvmInfoList;

extern const char *RunState_lookup[];
typedef enum RunState
{
    RUN_STATE_DEBUG = 0,
    RUN_STATE_INMIGRATE = 1,
    RUN_STATE_INTERNAL_ERROR = 2,
    RUN_STATE_IO_ERROR = 3,
    RUN_STATE_PAUSED = 4,
    RUN_STATE_POSTMIGRATE = 5,
    RUN_STATE_PRELAUNCH = 6,
    RUN_STATE_FINISH_MIGRATE = 7,
    RUN_STATE_RESTORE_VM = 8,
    RUN_STATE_RUNNING = 9,
    RUN_STATE_SAVE_VM = 10,
    RUN_STATE_SHUTDOWN = 11,
    RUN_STATE_WATCHDOG = 12,
    RUN_STATE_MAX = 13,
} RunState;

typedef struct StatusInfo StatusInfo;

typedef struct StatusInfoList
{
    StatusInfo *value;
    struct StatusInfoList *next;
} StatusInfoList;

typedef struct UuidInfo UuidInfo;

typedef struct UuidInfoList
{
    UuidInfo *value;
    struct UuidInfoList *next;
} UuidInfoList;

typedef struct ChardevInfo ChardevInfo;

typedef struct ChardevInfoList
{
    ChardevInfo *value;
    struct ChardevInfoList *next;
} ChardevInfoList;

typedef struct CommandInfo CommandInfo;

typedef struct CommandInfoList
{
    CommandInfo *value;
    struct CommandInfoList *next;
} CommandInfoList;

typedef struct MigrationStats MigrationStats;

typedef struct MigrationStatsList
{
    MigrationStats *value;
    struct MigrationStatsList *next;
} MigrationStatsList;

typedef struct MigrationInfo MigrationInfo;

typedef struct MigrationInfoList
{
    MigrationInfo *value;
    struct MigrationInfoList *next;
} MigrationInfoList;

typedef struct MouseInfo MouseInfo;

typedef struct MouseInfoList
{
    MouseInfo *value;
    struct MouseInfoList *next;
} MouseInfoList;

typedef struct CpuInfo CpuInfo;

typedef struct CpuInfoList
{
    CpuInfo *value;
    struct CpuInfoList *next;
} CpuInfoList;

typedef struct BlockDeviceInfo BlockDeviceInfo;

typedef struct BlockDeviceInfoList
{
    BlockDeviceInfo *value;
    struct BlockDeviceInfoList *next;
} BlockDeviceInfoList;

extern const char *BlockDeviceIoStatus_lookup[];
typedef enum BlockDeviceIoStatus
{
    BLOCK_DEVICE_IO_STATUS_OK = 0,
    BLOCK_DEVICE_IO_STATUS_FAILED = 1,
    BLOCK_DEVICE_IO_STATUS_NOSPACE = 2,
    BLOCK_DEVICE_IO_STATUS_MAX = 3,
} BlockDeviceIoStatus;

typedef struct BlockInfo BlockInfo;

typedef struct BlockInfoList
{
    BlockInfo *value;
    struct BlockInfoList *next;
} BlockInfoList;

typedef struct BlockDeviceStats BlockDeviceStats;

typedef struct BlockDeviceStatsList
{
    BlockDeviceStats *value;
    struct BlockDeviceStatsList *next;
} BlockDeviceStatsList;

typedef struct BlockStats BlockStats;

typedef struct BlockStatsList
{
    BlockStats *value;
    struct BlockStatsList *next;
} BlockStatsList;

typedef struct VncClientInfo VncClientInfo;

typedef struct VncClientInfoList
{
    VncClientInfo *value;
    struct VncClientInfoList *next;
} VncClientInfoList;

typedef struct VncInfo VncInfo;

typedef struct VncInfoList
{
    VncInfo *value;
    struct VncInfoList *next;
} VncInfoList;

typedef struct SpiceChannel SpiceChannel;

typedef struct SpiceChannelList
{
    SpiceChannel *value;
    struct SpiceChannelList *next;
} SpiceChannelList;

typedef struct SpiceInfo SpiceInfo;

typedef struct SpiceInfoList
{
    SpiceInfo *value;
    struct SpiceInfoList *next;
} SpiceInfoList;

typedef struct BalloonInfo BalloonInfo;

typedef struct BalloonInfoList
{
    BalloonInfo *value;
    struct BalloonInfoList *next;
} BalloonInfoList;

typedef struct PciMemoryRange PciMemoryRange;

typedef struct PciMemoryRangeList
{
    PciMemoryRange *value;
    struct PciMemoryRangeList *next;
} PciMemoryRangeList;

typedef struct PciMemoryRegion PciMemoryRegion;

typedef struct PciMemoryRegionList
{
    PciMemoryRegion *value;
    struct PciMemoryRegionList *next;
} PciMemoryRegionList;

typedef struct PciBridgeInfo PciBridgeInfo;

typedef struct PciBridgeInfoList
{
    PciBridgeInfo *value;
    struct PciBridgeInfoList *next;
} PciBridgeInfoList;

typedef struct PciDeviceInfo PciDeviceInfo;

typedef struct PciDeviceInfoList
{
    PciDeviceInfo *value;
    struct PciDeviceInfoList *next;
} PciDeviceInfoList;

typedef struct PciInfo PciInfo;

typedef struct PciInfoList
{
    PciInfo *value;
    struct PciInfoList *next;
} PciInfoList;

struct NameInfo
{
    bool has_name;
    char * name;
};

void qapi_free_NameInfoList(NameInfoList * obj);
void qapi_free_NameInfo(NameInfo * obj);

struct VersionInfo
{
    struct 
    {
        int64_t major;
        int64_t minor;
        int64_t micro;
    } qemu;
    char * package;
};

void qapi_free_VersionInfoList(VersionInfoList * obj);
void qapi_free_VersionInfo(VersionInfo * obj);

struct KvmInfo
{
    bool enabled;
    bool present;
};

void qapi_free_KvmInfoList(KvmInfoList * obj);
void qapi_free_KvmInfo(KvmInfo * obj);

struct StatusInfo
{
    bool running;
    bool singlestep;
    RunState status;
};

void qapi_free_StatusInfoList(StatusInfoList * obj);
void qapi_free_StatusInfo(StatusInfo * obj);

struct UuidInfo
{
    char * UUID;
};

void qapi_free_UuidInfoList(UuidInfoList * obj);
void qapi_free_UuidInfo(UuidInfo * obj);

struct ChardevInfo
{
    char * label;
    char * filename;
};

void qapi_free_ChardevInfoList(ChardevInfoList * obj);
void qapi_free_ChardevInfo(ChardevInfo * obj);

struct CommandInfo
{
    char * name;
};

void qapi_free_CommandInfoList(CommandInfoList * obj);
void qapi_free_CommandInfo(CommandInfo * obj);

struct MigrationStats
{
    int64_t transferred;
    int64_t remaining;
    int64_t total;
};

void qapi_free_MigrationStatsList(MigrationStatsList * obj);
void qapi_free_MigrationStats(MigrationStats * obj);

struct MigrationInfo
{
    bool has_status;
    char * status;
    bool has_ram;
    MigrationStats * ram;
    bool has_disk;
    MigrationStats * disk;
};

void qapi_free_MigrationInfoList(MigrationInfoList * obj);
void qapi_free_MigrationInfo(MigrationInfo * obj);

struct MouseInfo
{
    char * name;
    int64_t index;
    bool current;
    bool absolute;
};

void qapi_free_MouseInfoList(MouseInfoList * obj);
void qapi_free_MouseInfo(MouseInfo * obj);

struct CpuInfo
{
    int64_t CPU;
    bool current;
    bool halted;
    bool has_pc;
    int64_t pc;
    bool has_nip;
    int64_t nip;
    bool has_npc;
    int64_t npc;
    bool has_PC;
    int64_t PC;
    int64_t thread_id;
};

void qapi_free_CpuInfoList(CpuInfoList * obj);
void qapi_free_CpuInfo(CpuInfo * obj);

struct BlockDeviceInfo
{
    char * file;
    bool ro;
    char * drv;
    bool has_backing_file;
    char * backing_file;
    bool encrypted;
};

void qapi_free_BlockDeviceInfoList(BlockDeviceInfoList * obj);
void qapi_free_BlockDeviceInfo(BlockDeviceInfo * obj);

struct BlockInfo
{
    char * device;
    char * type;
    bool removable;
    bool locked;
    bool has_inserted;
    BlockDeviceInfo * inserted;
    bool has_tray_open;
    bool tray_open;
    bool has_io_status;
    BlockDeviceIoStatus io_status;
};

void qapi_free_BlockInfoList(BlockInfoList * obj);
void qapi_free_BlockInfo(BlockInfo * obj);

struct BlockDeviceStats
{
    int64_t rd_bytes;
    int64_t wr_bytes;
    int64_t rd_operations;
    int64_t wr_operations;
    int64_t flush_operations;
    int64_t flush_total_time_ns;
    int64_t wr_total_time_ns;
    int64_t rd_total_time_ns;
    int64_t wr_highest_offset;
};

void qapi_free_BlockDeviceStatsList(BlockDeviceStatsList * obj);
void qapi_free_BlockDeviceStats(BlockDeviceStats * obj);

struct BlockStats
{
    bool has_device;
    char * device;
    BlockDeviceStats * stats;
    bool has_parent;
    BlockStats * parent;
};

void qapi_free_BlockStatsList(BlockStatsList * obj);
void qapi_free_BlockStats(BlockStats * obj);

struct VncClientInfo
{
    char * host;
    char * family;
    char * service;
    bool has_x509_dname;
    char * x509_dname;
    bool has_sasl_username;
    char * sasl_username;
};

void qapi_free_VncClientInfoList(VncClientInfoList * obj);
void qapi_free_VncClientInfo(VncClientInfo * obj);

struct VncInfo
{
    bool enabled;
    bool has_host;
    char * host;
    bool has_family;
    char * family;
    bool has_service;
    char * service;
    bool has_auth;
    char * auth;
    bool has_clients;
    VncClientInfoList * clients;
};

void qapi_free_VncInfoList(VncInfoList * obj);
void qapi_free_VncInfo(VncInfo * obj);

struct SpiceChannel
{
    char * host;
    char * family;
    char * port;
    int64_t connection_id;
    int64_t channel_type;
    int64_t channel_id;
    bool tls;
};

void qapi_free_SpiceChannelList(SpiceChannelList * obj);
void qapi_free_SpiceChannel(SpiceChannel * obj);

struct SpiceInfo
{
    bool enabled;
    bool has_host;
    char * host;
    bool has_port;
    int64_t port;
    bool has_tls_port;
    int64_t tls_port;
    bool has_auth;
    char * auth;
    bool has_compiled_version;
    char * compiled_version;
    bool has_channels;
    SpiceChannelList * channels;
};

void qapi_free_SpiceInfoList(SpiceInfoList * obj);
void qapi_free_SpiceInfo(SpiceInfo * obj);

struct BalloonInfo
{
    int64_t actual;
    bool has_mem_swapped_in;
    int64_t mem_swapped_in;
    bool has_mem_swapped_out;
    int64_t mem_swapped_out;
    bool has_major_page_faults;
    int64_t major_page_faults;
    bool has_minor_page_faults;
    int64_t minor_page_faults;
    bool has_free_mem;
    int64_t free_mem;
    bool has_total_mem;
    int64_t total_mem;
};

void qapi_free_BalloonInfoList(BalloonInfoList * obj);
void qapi_free_BalloonInfo(BalloonInfo * obj);

struct PciMemoryRange
{
    int64_t base;
    int64_t limit;
};

void qapi_free_PciMemoryRangeList(PciMemoryRangeList * obj);
void qapi_free_PciMemoryRange(PciMemoryRange * obj);

struct PciMemoryRegion
{
    int64_t bar;
    char * type;
    int64_t address;
    int64_t size;
    bool has_prefetch;
    bool prefetch;
    bool has_mem_type_64;
    bool mem_type_64;
};

void qapi_free_PciMemoryRegionList(PciMemoryRegionList * obj);
void qapi_free_PciMemoryRegion(PciMemoryRegion * obj);

struct PciBridgeInfo
{
    struct 
    {
        int64_t number;
        int64_t secondary;
        int64_t subordinate;
        PciMemoryRange * io_range;
        PciMemoryRange * memory_range;
        PciMemoryRange * prefetchable_range;
    } bus;
    bool has_devices;
    PciDeviceInfoList * devices;
};

void qapi_free_PciBridgeInfoList(PciBridgeInfoList * obj);
void qapi_free_PciBridgeInfo(PciBridgeInfo * obj);

struct PciDeviceInfo
{
    int64_t bus;
    int64_t slot;
    int64_t function;
    struct 
    {
        bool has_desc;
        char * desc;
        int64_t class;
    } class_info;
    struct 
    {
        int64_t device;
        int64_t vendor;
    } id;
    bool has_irq;
    int64_t irq;
    char * qdev_id;
    bool has_pci_bridge;
    PciBridgeInfo * pci_bridge;
    PciMemoryRegionList * regions;
};

void qapi_free_PciDeviceInfoList(PciDeviceInfoList * obj);
void qapi_free_PciDeviceInfo(PciDeviceInfo * obj);

struct PciInfo
{
    int64_t bus;
    PciDeviceInfoList * devices;
};

void qapi_free_PciInfoList(PciInfoList * obj);
void qapi_free_PciInfo(PciInfo * obj);

#endif
