
// w2k_spy.h
// 08-27-2000 Sven B. Schreiber
// sbs@orgon.com

// =================================================================
// PROGRAM IDENTIFICATION
// =================================================================

#define DRV_BUILD           1
#define DRV_VERSION_HIGH    1
#define DRV_VERSION_LOW     0

// -----------------------------------------------------------------

#define DRV_DAY             27
#define DRV_MONTH           08
#define DRV_YEAR            2000

// -----------------------------------------------------------------

#define DRV_MODULE          w2k_spy
#define DRV_NAME            SBS Windows 2000 Spy Device
#define DRV_COMPANY         Sven B. Schreiber
#define DRV_AUTHOR          Sven B. Schreiber
#define DRV_EMAIL           sbs@orgon.com
#define DRV_PREFIX          SBS

// =================================================================
// HEADER FILES
// =================================================================

#include "drvinfo.h"        // defines more DRV_* items
#include "w2k_def.h"        // undocumented definitions

////////////////////////////////////////////////////////////////////
#ifdef _W2K_SPY_SYS_
////////////////////////////////////////////////////////////////////

// =================================================================
// MACROS
// =================================================================

#define min(_a,_b) (((_a) < (_b)) ? (_a) : (_b))
#define max(_a,_b) (((_a) > (_b)) ? (_a) : (_b))

// -----------------------------------------------------------------

#define MUTEX_INITIALIZE(_mutex) \
        KeInitializeMutex        \
            (&(_mutex), 0)

#define MUTEX_WAIT(_mutex)       \
        KeWaitForMutexObject     \
            (&(_mutex), Executive, KernelMode, FALSE, NULL)

#define MUTEX_RELEASE(_mutex)    \
        KeReleaseMutex           \
            (&(_mutex), FALSE)

// -----------------------------------------------------------------

#define UNICODE_LENGTH(_u) \
        ((_u) != NULL ? ((_u)->Length / WORD_) : 0)

#define UNICODE_BUFFER(_u) \
        ((_u) != NULL ? ((_u)->Buffer) : NULL)

#define OBJECT_NAME(_o)    \
        ((_o) != NULL ? ((_o)->ObjectName) : NULL)

// =================================================================
// CONSTANTS
// =================================================================

#define MAX_PATH            260

#define MAXBYTE             0xFF
#define MAXWORD             0xFFFF
#define MAXDWORD            0xFFFFFFFF

// -----------------------------------------------------------------

#define IMAGE_DIRECTORY_ENTRY_EXPORT             0
#define IMAGE_DIRECTORY_ENTRY_IMPORT             1
#define IMAGE_DIRECTORY_ENTRY_RESOURCE           2
#define IMAGE_DIRECTORY_ENTRY_EXCEPTION          3
#define IMAGE_DIRECTORY_ENTRY_SECURITY           4
#define IMAGE_DIRECTORY_ENTRY_BASERELOC          5
#define IMAGE_DIRECTORY_ENTRY_DEBUG              6
#define IMAGE_DIRECTORY_ENTRY_COPYRIGHT          7
#define IMAGE_DIRECTORY_ENTRY_GLOBALPTR          8
#define IMAGE_DIRECTORY_ENTRY_TLS                9
#define IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG       10
#define IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT      11
#define IMAGE_DIRECTORY_ENTRY_IAT               12
#define IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT      13
#define IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR    14

#define IMAGE_NUMBEROF_DIRECTORY_ENTRIES        16

// =================================================================
// WINDOWS 2000 IMAGE STRUCTURES
// =================================================================

typedef struct _IMAGE_FILE_HEADER
    {
    WORD  Machine;
    WORD  NumberOfSections;
    DWORD TimeDateStamp;
    DWORD PointerToSymbolTable;
    DWORD NumberOfSymbols;
    WORD  SizeOfOptionalHeader;
    WORD  Characteristics;
    }
    IMAGE_FILE_HEADER, *PIMAGE_FILE_HEADER;

// -----------------------------------------------------------------

typedef struct _IMAGE_DATA_DIRECTORY
    {
    DWORD VirtualAddress;
    DWORD Size;
    }
    IMAGE_DATA_DIRECTORY, *PIMAGE_DATA_DIRECTORY;

// -----------------------------------------------------------------

typedef struct _IMAGE_OPTIONAL_HEADER
    {
    WORD                 Magic;
    BYTE                 MajorLinkerVersion;
    BYTE                 MinorLinkerVersion;
    DWORD                SizeOfCode;
    DWORD                SizeOfInitializedData;
    DWORD                SizeOfUninitializedData;
    DWORD                AddressOfEntryPoint;
    DWORD                BaseOfCode;
    DWORD                BaseOfData;
    DWORD                ImageBase;
    DWORD                SectionAlignment;
    DWORD                FileAlignment;
    WORD                 MajorOperatingSystemVersion;
    WORD                 MinorOperatingSystemVersion;
    WORD                 MajorImageVersion;
    WORD                 MinorImageVersion;
    WORD                 MajorSubsystemVersion;
    WORD                 MinorSubsystemVersion;
    DWORD                Win32VersionValue;
    DWORD                SizeOfImage;
    DWORD                SizeOfHeaders;
    DWORD                CheckSum;
    WORD                 Subsystem;
    WORD                 DllCharacteristics;
    DWORD                SizeOfStackReserve;
    DWORD                SizeOfStackCommit;
    DWORD                SizeOfHeapReserve;
    DWORD                SizeOfHeapCommit;
    DWORD                LoaderFlags;
    DWORD                NumberOfRvaAndSizes;
    IMAGE_DATA_DIRECTORY DataDirectory
                         [IMAGE_NUMBEROF_DIRECTORY_ENTRIES];
    }
    IMAGE_OPTIONAL_HEADER, *PIMAGE_OPTIONAL_HEADER;

// -----------------------------------------------------------------

typedef struct _IMAGE_NT_HEADERS
    {
    DWORD                 Signature;
    IMAGE_FILE_HEADER     FileHeader;
    IMAGE_OPTIONAL_HEADER OptionalHeader;
    }
    IMAGE_NT_HEADERS, *PIMAGE_NT_HEADERS;

// -----------------------------------------------------------------

typedef struct _IMAGE_EXPORT_DIRECTORY
    {
    DWORD Characteristics;
    DWORD TimeDateStamp;
    WORD  MajorVersion;
    WORD  MinorVersion;
    DWORD Name;
    DWORD Base;
    DWORD NumberOfFunctions;
    DWORD NumberOfNames;
    DWORD AddressOfFunctions;
    DWORD AddressOfNames;
    DWORD AddressOfNameOrdinals;
    }
    IMAGE_EXPORT_DIRECTORY, *PIMAGE_EXPORT_DIRECTORY;

// =================================================================
// WINDOWS 2000 MODULE INFORMATION
// =================================================================

#define SystemModuleInformation 11 // SYSTEMINFOCLASS

// -----------------------------------------------------------------

typedef struct _MODULE_INFO
    {
    DWORD dReserved1;
    DWORD dReserved2;
    PVOID pBase;
    DWORD dSize;
    DWORD dFlags;
    WORD  wIndex;
    WORD  wRank;
    WORD  wLoadCount;
    WORD  wNameOffset;
    BYTE  abPath [MAXIMUM_FILENAME_LENGTH];
    }
    MODULE_INFO, *PMODULE_INFO, **PPMODULE_INFO;

#define MODULE_INFO_ sizeof (MODULE_INFO)

// -----------------------------------------------------------------

typedef struct _MODULE_LIST
    {
    DWORD       dModules;
    MODULE_INFO aModules [];
    }
    MODULE_LIST, *PMODULE_LIST, **PPMODULE_LIST;

#define MODULE_LIST_ sizeof (MODULE_LIST)

// =================================================================
// WINDOWS 2000 API PROTOTYPES
// =================================================================

PIMAGE_NT_HEADERS NTAPI
RtlImageNtHeader (PVOID Base);

NTSTATUS NTAPI
ZwQuerySystemInformation (DWORD  SystemInformationClass,
                          PVOID  SystemInformation,
                          DWORD  SystemInformationLength,
                          PDWORD ReturnLength);

////////////////////////////////////////////////////////////////////
#else // #ifdef _W2K_SPY_SYS_
////////////////////////////////////////////////////////////////////

// =================================================================
// CONSTANTS
// =================================================================

//#define PAGE_SHIFT               12
#define PTI_SHIFT                12
//#define PDI_SHIFT                22

#define MAXIMUM_FILENAME_LENGTH 256

// -----------------------------------------------------------------

typedef LARGE_INTEGER
        PHYSICAL_ADDRESS, *PPHYSICAL_ADDRESS, **PPPHYSICAL_ADDRESS;

typedef LONG
        NTSTATUS, *PNTSTATUS, **PPNTSTATUS;

// -----------------------------------------------------------------
/*
typedef enum _NT_PRODUCT_TYPE
    {
    NtProductInvalid,
    NtProductWinNt,
    NtProductLanManNt,
    NtProductServer
    }
    NT_PRODUCT_TYPE, *PNT_PRODUCT_TYPE, **PPNT_PRODUCT_TYPE;*/

////////////////////////////////////////////////////////////////////
#endif // #ifdef _W2K_SPY_SYS_
////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////
#ifndef _RC_PASS_
////////////////////////////////////////////////////////////////////

// =================================================================
// MACROS
// =================================================================

#define PTR_ADD(_base,_offset) \
        ((PVOID) ((PBYTE) (_base) + (DWORD) (_offset)))

// =================================================================
// CONSTANTS
// =================================================================

#define SPY_VERSION         DRV_VERSION_BINARY // see drvinfo.h
#define SPY_TAG             '>YPS'             // SPY>

#define SPY_CALLS           0x00000100 // max api call nesting level
#define SPY_NAME            0x00000400 // max object name length
#define SPY_HANDLES         0x00001000 // max number of handles
#define SPY_NAME_BUFFER     0x00100000 // object name buffer size
#define SPY_DATA_BUFFER     0x00100000 // protocol data buffer size

// -----------------------------------------------------------------

#define FILE_DEVICE_SPY     0x8000
#define SPY_IO_BASE         0x0800

// -----------------------------------------------------------------

#define SPY_IO(_code,_read,_write)                        \
        CTL_CODE ((FILE_DEVICE_SPY),                      \
                  ((SPY_IO_BASE) + (_code)),              \
                  (METHOD_BUFFERED),                      \
                  (((_read)  ? (FILE_READ_ACCESS)  : 0) | \
                   ((_write) ? (FILE_WRITE_ACCESS) : 0)))

// -----------------------------------------------------------------
//      symbol                    code  read  write

#define SPY_IO_VERSION_INFO SPY_IO ( 0, TRUE, FALSE)
#define SPY_IO_OS_INFO      SPY_IO ( 1, TRUE, FALSE)
#define SPY_IO_SEGMENT      SPY_IO ( 2, TRUE, FALSE)
#define SPY_IO_INTERRUPT    SPY_IO ( 3, TRUE, FALSE)
#define SPY_IO_PHYSICAL     SPY_IO ( 4, TRUE, FALSE)
#define SPY_IO_CPU_INFO     SPY_IO ( 5, TRUE, FALSE)
#define SPY_IO_PDE_ARRAY    SPY_IO ( 6, TRUE, FALSE)
#define SPY_IO_PAGE_ENTRY   SPY_IO ( 7, TRUE, FALSE)
#define SPY_IO_MEMORY_DATA  SPY_IO ( 8, TRUE, FALSE)
#define SPY_IO_MEMORY_BLOCK SPY_IO ( 9, TRUE, FALSE)
#define SPY_IO_HANDLE_INFO  SPY_IO (10, TRUE, FALSE)
#define SPY_IO_HOOK_INFO    SPY_IO (11, TRUE, FALSE)
#define SPY_IO_HOOK_INSTALL SPY_IO (12, TRUE, TRUE )
#define SPY_IO_HOOK_REMOVE  SPY_IO (13, TRUE, TRUE )
#define SPY_IO_HOOK_PAUSE   SPY_IO (14, TRUE, TRUE )
#define SPY_IO_HOOK_FILTER  SPY_IO (15, TRUE, TRUE )
#define SPY_IO_HOOK_RESET   SPY_IO (16, TRUE, TRUE )
#define SPY_IO_HOOK_READ    SPY_IO (17, TRUE, FALSE)
#define SPY_IO_HOOK_WRITE   SPY_IO (18, TRUE, TRUE )
#define SPY_IO_MODULE_INFO  SPY_IO (19, TRUE, FALSE)
#define SPY_IO_PE_HEADER    SPY_IO (20, TRUE, FALSE)
#define SPY_IO_PE_EXPORT    SPY_IO (21, TRUE, FALSE)
#define SPY_IO_PE_SYMBOL    SPY_IO (22, TRUE, FALSE)
#define SPY_IO_CALL         SPY_IO (23, TRUE, TRUE )

// -----------------------------------------------------------------

#define SDT_SYMBOLS_NT4     0xD3
#define SDT_SYMBOLS_NT5     0xF8
#define SDT_SYMBOLS_MAX     SDT_SYMBOLS_NT5

// -----------------------------------------------------------------

#define IMAGE_FILE_HEADER_      sizeof (IMAGE_FILE_HEADER)
#define IMAGE_DATA_DIRECTORY_   sizeof (IMAGE_DATA_DIRECTORY)
#define IMAGE_OPTIONAL_HEADER_  sizeof (IMAGE_OPTIONAL_HEADER)
#define IMAGE_NT_HEADERS_       sizeof (IMAGE_NT_HEADERS)
#define IMAGE_EXPORT_DIRECTORY_ sizeof (IMAGE_EXPORT_DIRECTORY)

// -----------------------------------------------------------------

#define INVALID_ADDRESS     ((PVOID) -1)

// =================================================================
// INTEL X86 STRUCTURES, PART 1 OF 3
// =================================================================

typedef DWORD X86_REGISTER, *PX86_REGISTER, **PPX86_REGISTER;

// -----------------------------------------------------------------

typedef struct _X86_SELECTOR
    {
    union
        {
        struct
            {
            WORD wValue;            // packed value
            WORD wReserved;
            };
        struct
            {
            unsigned RPL      :  2; // requested privilege level
            unsigned TI       :  1; // table indicator: 0=gdt, 1=ldt
            unsigned Index    : 13; // index into descriptor table
            unsigned Reserved : 16;
            };
        };
    }
    X86_SELECTOR, *PX86_SELECTOR, **PPX86_SELECTOR;

#define X86_SELECTOR_ sizeof (X86_SELECTOR)

// -----------------------------------------------------------------

typedef struct _X86_DESCRIPTOR
    {
    union
        {
        struct
            {
            DWORD dValueLow;        // packed value
            DWORD dValueHigh;
            };
        struct
            {
            unsigned Limit1   : 16; // bits 15..00
            unsigned Base1    : 16; // bits 15..00
            unsigned Base2    :  8; // bits 23..16
            unsigned Type     :  4; // segment type
            unsigned S        :  1; // type (0=system, 1=code/data)
            unsigned DPL      :  2; // descriptor privilege level
            unsigned P        :  1; // segment present
            unsigned Limit2   :  4; // bits 19..16
            unsigned AVL      :  1; // available to programmer 
            unsigned Reserved :  1;
            unsigned DB       :  1; // 0=16-bit, 1=32-bit
            unsigned G        :  1; // granularity (1=4KB)
            unsigned Base3    :  8; // bits 31..24
            };
        };
    }
    X86_DESCRIPTOR, *PX86_DESCRIPTOR, **PPX86_DESCRIPTOR;

#define X86_DESCRIPTOR_ sizeof (X86_DESCRIPTOR)

// -----------------------------------------------------------------

typedef struct _X86_GATE
    {
    union
        {
        struct
            {
            DWORD dValueLow;          // packed value
            DWORD dValueHigh;
            };
        struct
            {
            unsigned Offset1    : 16; // bits 15..00
            unsigned Selector   : 16; // segment selector
            unsigned Parameters :  5; // parameters
            unsigned Reserved   :  3;
            unsigned Type       :  4; // gate type and size
            unsigned S          :  1; // always 0
            unsigned DPL        :  2; // descriptor privilege level
            unsigned P          :  1; // segment present
            unsigned Offset2    : 16; // bits 31..16
            };
        };
    }
    X86_GATE, *PX86_GATE, **PPX86_GATE;

#define X86_GATE_ sizeof (X86_GATE)

// -----------------------------------------------------------------

typedef struct _X86_TABLE
    {
    WORD wReserved;                   // force 32-bit alignment
    WORD wLimit;                      // table limit
    union
        {
        PX86_DESCRIPTOR pDescriptors; // used by sgdt instruction
        PX86_GATE       pGates;       // used by sidt instruction
        };
    }
    X86_TABLE, *PX86_TABLE, **PPX86_TABLE;

#define X86_TABLE_ sizeof (X86_TABLE)

// =================================================================
// INTEL X86 STRUCTURES, PART 2 OF 3
// =================================================================

typedef struct _X86_PDBR // page-directory base register (cr3)
    {
    union
        {
        struct
            {
            DWORD dValue;            // packed value
            };
        struct
            {
            unsigned Reserved1 :  3;
            unsigned PWT       :  1; // page-level write-through
            unsigned PCD       :  1; // page-level cache disabled
            unsigned Reserved2 :  7;
            unsigned PFN       : 20; // page-frame number
            };
        };
    }
    X86_PDBR, *PX86_PDBR, **PPX86_PDBR;

#define X86_PDBR_ sizeof (X86_PDBR)

// -----------------------------------------------------------------

typedef struct _X86_PDE_4M // page-directory entry (4-MB page)
    {
    union
        {
        struct
            {
            DWORD dValue;            // packed value
            };
        struct
            {
            unsigned P         :  1; // present (1 = present)
            unsigned RW        :  1; // read/write
            unsigned US        :  1; // user/supervisor
            unsigned PWT       :  1; // page-level write-through
            unsigned PCD       :  1; // page-level cache disabled
            unsigned A         :  1; // accessed
            unsigned D         :  1; // dirty
            unsigned PS        :  1; // page size (1 = 4-MB page)
            unsigned G         :  1; // global page
            unsigned Available :  3; // available to programmer
            unsigned Reserved  : 10;
            unsigned PFN       : 10; // page-frame number
            };
        };
    }
    X86_PDE_4M, *PX86_PDE_4M, **PPX86_PDE_4M;

#define X86_PDE_4M_ sizeof (X86_PDE_4M)

// -----------------------------------------------------------------

typedef struct _X86_PDE_4K // page-directory entry (4-KB page)
    {
    union
        {
        struct
            {
            DWORD dValue;            // packed value
            };
        struct
            {
            unsigned P         :  1; // present (1 = present)
            unsigned RW        :  1; // read/write
            unsigned US        :  1; // user/supervisor
            unsigned PWT       :  1; // page-level write-through
            unsigned PCD       :  1; // page-level cache disabled
            unsigned A         :  1; // accessed
            unsigned Reserved  :  1; // dirty
            unsigned PS        :  1; // page size (0 = 4-KB page)
            unsigned G         :  1; // global page
            unsigned Available :  3; // available to programmer
            unsigned PFN       : 20; // page-frame number
            };
        };
    }
    X86_PDE_4K, *PX86_PDE_4K, **PPX86_PDE_4K;

#define X86_PDE_4K_ sizeof (X86_PDE_4K)

// -----------------------------------------------------------------

typedef struct _X86_PTE_4K // page-table entry (4-KB page)
    {
    union
        {
        struct
            {
            DWORD dValue;            // packed value
            };
        struct
            {
            unsigned P         :  1; // present (1 = present)
            unsigned RW        :  1; // read/write
            unsigned US        :  1; // user/supervisor
            unsigned PWT       :  1; // page-level write-through
            unsigned PCD       :  1; // page-level cache disabled
            unsigned A         :  1; // accessed
            unsigned D         :  1; // dirty
            unsigned Reserved  :  1;
            unsigned G         :  1; // global page
            unsigned Available :  3; // available to programmer
            unsigned PFN       : 20; // page-frame number
            };
        };
    }
    X86_PTE_4K, *PX86_PTE_4K, **PPX86_PTE_4K;

#define X86_PTE_4K_ sizeof (X86_PTE_4K)

// -----------------------------------------------------------------

typedef struct _X86_PNPE // page not present entry
    {
    union
        {
        struct
            {
            DWORD dValue;            // packed value
            };
        struct
            {
            unsigned P         :  1; // present (0 = not present)
            unsigned Reserved1 :  9;
            unsigned PageFile  :  1; // page swapped to pagefile
            unsigned Reserved2 : 21;
            };
        };
    }
    X86_PNPE, *PX86_PNPE, **PPX86_PNPE;

#define X86_PNPE_ sizeof (X86_PNPE)

// -----------------------------------------------------------------

typedef struct _X86_PE // general page entry
    {
    union
        {
        DWORD      dValue; // packed value
        X86_PDBR   pdbr;   // page-directory Base Register
        X86_PDE_4M pde4M;  // page-directory entry (4-MB page)
        X86_PDE_4K pde4K;  // page-directory entry (4-KB page)
        X86_PTE_4K pte4K;  // page-table entry (4-KB page)
        X86_PNPE   pnpe;   // page not present entry
        };
    }
    X86_PE, *PX86_PE, **PPX86_PE;

#define X86_PE_ sizeof (X86_PE)

// =================================================================
// INTEL X86 STRUCTURES, PART 3 OF 3
// =================================================================

typedef struct _X86_LINEAR_4M // linear address (4-MB page)
    {
    union
        {
        struct
            {
            PVOID pAddress;       // packed address
            };
        struct
            {
            unsigned Offset : 22; // offset into page
            unsigned PDI    : 10; // page-directory index
            };
        };
    }
    X86_LINEAR_4M, *PX86_LINEAR_4M, **PPX86_LINEAR_4M;

#define X86_LINEAR_4M_ sizeof (X86_LINEAR_4M)

// -----------------------------------------------------------------

typedef struct _X86_LINEAR_4K // linear address (4-KB page)
    {
    union
        {
        struct
            {
            PVOID pAddress;       // packed address
            };
        struct
            {
            unsigned Offset : 12; // offset into page
            unsigned PTI    : 10; // page-table index
            unsigned PDI    : 10; // page-directory index
            };
        };
    }
    X86_LINEAR_4K, *PX86_LINEAR_4K, **PPX86_LINEAR_4K;

#define X86_LINEAR_4K_ sizeof (X86_LINEAR_4K)

// -----------------------------------------------------------------

typedef struct _X86_LINEAR // general linear address
    {
    union
        {
        PVOID         pAddress; // packed address
        X86_LINEAR_4M linear4M; // linear address (4-MB page)
        X86_LINEAR_4K linear4K; // linear address (4-KB page)
        };
    }
    X86_LINEAR, *PX86_LINEAR, **PPX86_LINEAR;

#define X86_LINEAR_ sizeof (X86_LINEAR)

// =================================================================
// INTEL X86 MACROS & CONSTANTS
// =================================================================

#define X86_PAGE_MASK (0 - (1 << PAGE_SHIFT))
#define X86_PAGE(_p)  (((DWORD) (_p) & X86_PAGE_MASK) >> PAGE_SHIFT)

#define X86_PDI_MASK  (0 - (1 << PDI_SHIFT))
#define X86_PDI(_p)   (((DWORD) (_p) & X86_PDI_MASK) >> PDI_SHIFT)

#define X86_PTI_MASK  ((0 - (1 << PTI_SHIFT)) & ~X86_PDI_MASK)
#define X86_PTI(_p)   (((DWORD) (_p) & X86_PTI_MASK) >> PTI_SHIFT)

#define X86_OFFSET(_p,_m) ((DWORD_PTR) (_p) & ~(_m))
#define X86_OFFSET_4M(_p) X86_OFFSET (_p, X86_PDI_MASK)
#define X86_OFFSET_4K(_p) X86_OFFSET (_p, X86_PDI_MASK|X86_PTI_MASK)

#define X86_PAGE_4M   (1 << PDI_SHIFT)
#define X86_PAGE_4K   (1 << PTI_SHIFT)

#define X86_PAGES_4M  (1 << (32 - PDI_SHIFT))
#define X86_PAGES_4K  (1 << (32 - PTI_SHIFT))

// -----------------------------------------------------------------

#define X86_PAGES         0xC0000000
#define X86_PTE_ARRAY     ((PX86_PE) X86_PAGES)
#define X86_PDE_ARRAY     (X86_PTE_ARRAY + (X86_PAGES >> PTI_SHIFT))

// -----------------------------------------------------------------

#define X86_SEGMENT_OTHER           0
#define X86_SEGMENT_CS              1
#define X86_SEGMENT_DS              2
#define X86_SEGMENT_ES              3
#define X86_SEGMENT_FS              4
#define X86_SEGMENT_GS              5
#define X86_SEGMENT_SS              6
#define X86_SEGMENT_TSS             7

// -----------------------------------------------------------------

#define X86_SELECTOR_RPL            0x0003
#define X86_SELECTOR_TI             0x0004
#define X86_SELECTOR_INDEX          0xFFF8
#define X86_SELECTOR_SHIFT          3

#define X86_SELECTOR_LIMIT          (X86_SELECTOR_INDEX >> \
                                     X86_SELECTOR_SHIFT)

// -----------------------------------------------------------------

#define X86_DESCRIPTOR_SYS_TSS16A       0x1
#define X86_DESCRIPTOR_SYS_LDT          0x2
#define X86_DESCRIPTOR_SYS_TSS16B       0x3
#define X86_DESCRIPTOR_SYS_CALL16       0x4
#define X86_DESCRIPTOR_SYS_TASK         0x5
#define X86_DESCRIPTOR_SYS_INT16        0x6
#define X86_DESCRIPTOR_SYS_TRAP16       0x7
#define X86_DESCRIPTOR_SYS_TSS32A       0x9
#define X86_DESCRIPTOR_SYS_TSS32B       0xB
#define X86_DESCRIPTOR_SYS_CALL32       0xC
#define X86_DESCRIPTOR_SYS_INT32        0xE
#define X86_DESCRIPTOR_SYS_TRAP32       0xF

// -----------------------------------------------------------------

#define X86_DESCRIPTOR_APP_ACCESSED     0x1
#define X86_DESCRIPTOR_APP_READ_WRITE   0x2
#define X86_DESCRIPTOR_APP_EXECUTE_READ 0x2
#define X86_DESCRIPTOR_APP_EXPAND_DOWN  0x4
#define X86_DESCRIPTOR_APP_CONFORMING   0x4
#define X86_DESCRIPTOR_APP_CODE         0x8

// =================================================================
// SPY STRUCTURES
// =================================================================

typedef struct _SPY_VERSION_INFO
    {
    DWORD dVersion;
    WORD  awName [SPY_NAME];
    }
    SPY_VERSION_INFO, *PSPY_VERSION_INFO, **PPSPY_VERSION_INFO;

#define SPY_VERSION_INFO_ sizeof (SPY_VERSION_INFO)

// -----------------------------------------------------------------

typedef struct _SPY_OS_INFO
    {
    DWORD   dPageSize;
    DWORD   dPageShift;
    DWORD   dPtiShift;
    DWORD   dPdiShift;
    DWORD   dPageMask;
    DWORD   dPtiMask;
    DWORD   dPdiMask;
    PX86_PE PteArray;
    PX86_PE PdeArray;
    PVOID   pLowestUserAddress;
    PVOID   pThreadEnvironmentBlock;
    PVOID   pHighestUserAddress;
    PVOID   pUserProbeAddress;
    PVOID   pSystemRangeStart;
    PVOID   pLowestSystemAddress;
    PVOID   pSharedUserData;
    PVOID   pProcessorControlRegion;
    PVOID   pProcessorControlBlock;
    DWORD   dGlobalFlag;
    DWORD   dI386MachineType;
    DWORD   dNumberProcessors;
    DWORD   dProductType;
    DWORD   dBuildNumber;
    DWORD   dNtMajorVersion;
    DWORD   dNtMinorVersion;
    WORD    awNtSystemRoot [MAX_PATH];
    }
    SPY_OS_INFO, *PSPY_OS_INFO, **PPSPY_OS_INFO;

#define SPY_OS_INFO_ sizeof (SPY_OS_INFO)

// -----------------------------------------------------------------

typedef struct _SPY_SEGMENT
    {
    X86_SELECTOR   Selector;
    X86_DESCRIPTOR Descriptor;
    PVOID          pBase;
    DWORD          dLimit;
    BOOL           fOk;
    }
    SPY_SEGMENT, *PSPY_SEGMENT, **PPSPY_SEGMENT;

#define SPY_SEGMENT_ sizeof (SPY_SEGMENT)

// -----------------------------------------------------------------

typedef struct _SPY_INTERRUPT
    {
    X86_SELECTOR Selector;
    X86_GATE     Gate;
    SPY_SEGMENT  Segment;
    PVOID        pOffset;
    BOOL         fOk;
    }
    SPY_INTERRUPT, *PSPY_INTERRUPT, **PPSPY_INTERRUPT;

#define SPY_INTERRUPT_ sizeof (SPY_INTERRUPT)

// -----------------------------------------------------------------

typedef struct _SPY_CPU_INFO
    {
    X86_REGISTER cr0;
    X86_REGISTER cr2;
    X86_REGISTER cr3;
    SPY_SEGMENT  cs;
    SPY_SEGMENT  ds;
    SPY_SEGMENT  es;
    SPY_SEGMENT  fs;
    SPY_SEGMENT  gs;
    SPY_SEGMENT  ss;
    SPY_SEGMENT  tss;
    X86_TABLE    idt;
    X86_TABLE    gdt;
    X86_SELECTOR ldt;
    }
    SPY_CPU_INFO, *PSPY_CPU_INFO, **PPSPY_CPU_INFO;

#define SPY_CPU_INFO_ sizeof (SPY_CPU_INFO)

// -----------------------------------------------------------------

typedef struct _SPY_PDE_ARRAY
    {
    X86_PE apde [X86_PAGES_4M];
    }
    SPY_PDE_ARRAY, *PSPY_PDE_ARRAY, **PPSPY_PDE_ARRAY;

#define SPY_PDE_ARRAY_ sizeof (SPY_PDE_ARRAY)

// -----------------------------------------------------------------

typedef struct _SPY_PAGE_ENTRY
    {
    X86_PE pe;
    DWORD  dSize;
    BOOL   fPresent;
    }
    SPY_PAGE_ENTRY, *PSPY_PAGE_ENTRY, **PPSPY_PAGE_ENTRY;

#define SPY_PAGE_ENTRY_ sizeof (SPY_PAGE_ENTRY)

// -----------------------------------------------------------------

typedef struct _SPY_MEMORY_BLOCK
    {
    union
        {
        PBYTE pbAddress;
        PVOID pAddress;
        };
    DWORD dBytes;
    }
    SPY_MEMORY_BLOCK, *PSPY_MEMORY_BLOCK, **PPSPY_MEMORY_BLOCK;

#define SPY_MEMORY_BLOCK_ sizeof (SPY_MEMORY_BLOCK)

// -----------------------------------------------------------------

#define SPY_MEMORY_DATA_N(_n) \
        struct _SPY_MEMORY_DATA_##_n \
            { \
            SPY_MEMORY_BLOCK smb; \
            WORD             awData [_n]; \
            }

typedef SPY_MEMORY_DATA_N (0)
        SPY_MEMORY_DATA, *PSPY_MEMORY_DATA, **PPSPY_MEMORY_DATA;

#define SPY_MEMORY_DATA_ sizeof (SPY_MEMORY_DATA)
#define SPY_MEMORY_DATA__(_n) (SPY_MEMORY_DATA_ + ((_n) * WORD_))

#define SPY_MEMORY_DATA_BYTE  0x00FF
#define SPY_MEMORY_DATA_VALID 0x0100

#define SPY_MEMORY_DATA_VALUE(_b,_v) \
        ((WORD) (((_b) & SPY_MEMORY_DATA_BYTE     ) | \
                 ((_v) ? SPY_MEMORY_DATA_VALID : 0)))

// -----------------------------------------------------------------

typedef struct _SPY_HANDLE_INFO
    {
    PVOID pObjectBody;
    DWORD dHandleAttributes;
    }
    SPY_HANDLE_INFO, *PSPY_HANDLE_INFO, **PPSPY_HANDLE_INFO;

#define SPY_HANDLE_INFO_ sizeof (SPY_HANDLE_INFO)

// -----------------------------------------------------------------

typedef struct _SPY_HOOK_ENTRY
    {
    NTPROC Handler;
    PBYTE  pbFormat;
    }
    SPY_HOOK_ENTRY, *PSPY_HOOK_ENTRY, **PPSPY_HOOK_ENTRY;

#define SPY_HOOK_ENTRY_ sizeof (SPY_HOOK_ENTRY)

// -----------------------------------------------------------------

typedef struct _SPY_CALL
    {
    BOOL            fInUse;               // set if used entry
    HANDLE          hThread;              // id of calling thread
    PSPY_HOOK_ENTRY pshe;                 // associated hook entry
    PVOID           pCaller;              // caller's return address
    DWORD           dParameters;          // number of parameters
    DWORD           adParameters [1+256]; // result and parameters
    }
    SPY_CALL, *PSPY_CALL, **PPSPY_CALL;

#define SPY_CALL_ sizeof (SPY_CALL)

// -----------------------------------------------------------------

typedef struct _SPY_HEADER
    {
    LARGE_INTEGER liStart;  // start time
    DWORD         dRead;    // read data index
    DWORD         dWrite;   // write data index
    DWORD         dCalls;   // api usage count
    DWORD         dHandles; // handle count
    DWORD         dName;    // object name index
    }
    SPY_HEADER, *PSPY_HEADER, **PPSPY_HEADER;

#define SPY_HEADER_ sizeof (SPY_HEADER)

// -----------------------------------------------------------------

typedef struct _SPY_PROTOCOL
    {
    SPY_HEADER    sh;                            // protocol header
    HANDLE        ahProcesses [SPY_HANDLES];     // process id array
    HANDLE        ahObjects   [SPY_HANDLES];     // handle array
    DWORD         adNames     [SPY_HANDLES];     // name offsets
    WORD          awNames     [SPY_NAME_BUFFER]; // name strings
    BYTE          abData      [SPY_DATA_BUFFER]; // protocol data
    }
    SPY_PROTOCOL, *PSPY_PROTOCOL, **PPSPY_PROTOCOL;

#define SPY_PROTOCOL_ sizeof (SPY_PROTOCOL)

// -----------------------------------------------------------------

typedef struct _SPY_HOOK_INFO
    {
    SPY_HEADER                sh;
    PSPY_CALL                 psc;
    PSPY_PROTOCOL             psp;
    PSERVICE_DESCRIPTOR_TABLE psdt;
    SERVICE_DESCRIPTOR_TABLE  sdt;
    DWORD                     ServiceLimit;
    NTPROC                    ServiceTable  [SDT_SYMBOLS_MAX];
    BYTE                      ArgumentTable [SDT_SYMBOLS_MAX];
    SPY_HOOK_ENTRY            SpyHooks      [SDT_SYMBOLS_MAX];
    }
    SPY_HOOK_INFO, *PSPY_HOOK_INFO, **PPSPY_HOOK_INFO;

#define SPY_HOOK_INFO_ sizeof (SPY_HOOK_INFO)

// -----------------------------------------------------------------

typedef struct _SPY_MODULE_INFO
    {
    PVOID pBase;
    DWORD dSize;
    DWORD dFlags;
    DWORD dIndex;
    DWORD dLoadCount;
    DWORD dNameOffset;
    BYTE  abPath [MAXIMUM_FILENAME_LENGTH];
    }
    SPY_MODULE_INFO, *PSPY_MODULE_INFO, **PPSPY_MODULE_INFO;

#define SPY_MODULE_INFO_ sizeof (SPY_MODULE_INFO)

// -----------------------------------------------------------------

typedef struct _SPY_CALL_INPUT
    {
    BOOL  fFastCall;
    DWORD dArgumentBytes;
    PVOID pArguments;
    PBYTE pbSymbol;
    PVOID pEntryPoint;
    }
    SPY_CALL_INPUT, *PSPY_CALL_INPUT, **PPSPY_CALL_INPUT;

#define SPY_CALL_INPUT_ sizeof (SPY_CALL_INPUT)

// -----------------------------------------------------------------

typedef struct _SPY_CALL_OUTPUT
    {
    ULARGE_INTEGER uliResult;
    }
    SPY_CALL_OUTPUT, *PSPY_CALL_OUTPUT, **PPSPY_CALL_OUTPUT;

#define SPY_CALL_OUTPUT_ sizeof (SPY_CALL_OUTPUT)

// -----------------------------------------------------------------

typedef struct _SPY_SEARCH
    {
    QWORD aqFlags [256];
    QWORD qMask;
    QWORD qTest;
    DWORD dNext;
    DWORD dHit;
    DWORD dBytes;
    }
    SPY_SEARCH, *PSPY_SEARCH, **PPSPY_SEARCH;

#define SPY_SEARCH_ sizeof (SPY_SEARCH)

// =================================================================
// DEVICE CONTEXT
// =================================================================

#ifdef _W2K_SPY_SYS_

typedef struct _DEVICE_CONTEXT
    {
    PDRIVER_OBJECT  pDriverObject;        // driver object ptr
    PDEVICE_OBJECT  pDeviceObject;        // device object ptr
    KMUTEX          kmDispatch;           // ioctl dispatch mutex
    KMUTEX          kmProtocol;           // protocol access mutex
    DWORD           dLevel;               // nesting level
    DWORD           dMisses;              // number of misses
    SPY_CALL        SpyCalls [SPY_CALLS]; // api call contexts
    SPY_PROTOCOL    SpyProtocol;          // protocol control block
    }
    DEVICE_CONTEXT, *PDEVICE_CONTEXT, **PPDEVICE_CONTEXT;

#define DEVICE_CONTEXT_ sizeof (DEVICE_CONTEXT)

#endif // #ifdef _W2K_SPY_SYS_

////////////////////////////////////////////////////////////////////
#endif // #ifndef _RC_PASS_
////////////////////////////////////////////////////////////////////

// =================================================================
// END OF FILE
// =================================================================
