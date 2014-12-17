
/*
Copyright (C) <2012> <Syracuse System Security (Sycure) Lab>

DECAF is based on QEMU, a whole-system emulator. You can redistribute
and modify it under the terms of the GNU GPL, version 3 or later,
but it is made available WITHOUT ANY WARRANTY. See the top-level
README file for more details.

For more information about DECAF and other softwares, see our
web site at:
http://sycurelab.ecs.syr.edu/

If you have any questions about DECAF,please post it on
http://code.google.com/p/decaf-platform/


 * windows_vmi.h
 *
 *  Created on: Aug 1, 2012
 *      Author: haoru
 */

//#ifdef CONFIG_VMI_ENABLE

#ifndef WINDOWS_VMI_H_
#define WINDOWS_VMI_H_
#include <inttypes.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>

#include <sys/time.h>
#include <math.h>
#include <glib.h>

#include "vmi.h"

#ifdef __cplusplus
extern "C" {
#endif
//#include "helper.h"
//#include "cpu.h"
#include "config.h"
#include "DECAF_callback_common.h" // AWH

#define MAX_NAME_LENGTH 128
#define MAX_UNICODE_LENGTH 2*MAX_NAME_LENGTH

#ifdef __x86_64__
#define BYTE uint8_t
#define WORD uint16_t
#define DWORD uint32_t
#define LONG uint32_t
#else
#define BYTE     unsigned char
#define WORD     unsigned short
#define DWORD    unsigned int
#define LONG     unsigned int
#endif

#define KPCR_OFFSET 0x1c // base on rc3 at 0xffdff000
#define KDVB_OFFSET 0x34 // base on KPCR
#define PSLM_OFFSET 0x70
#define PSAPH_OFFSET 0x78 // base on KDVB

#define NAMESIZE 16
////////////////////////////
#define PROCESSINFO 0x10
#define QUOTAINFO   0x08
#define HANDLEINFO  0x04
#define NAMEINFO    0x02
#define CREATORINFO 0x01

#define MODULES 0
#define PROC 1
#define THRD 2
//#define FILE 3

#define IMAGE_SIZEOF_SHORT_NAME 8
#define IMAGE_NUMBEROF_DIRECTORY_ENTRIES   16
#define IMAGE_NT_SIGNATURE 0x00004550

#define IMAGE_DIRECTORY_ENTRY_EXPORT 0
#define IMAGE_DIRECTORY_ENTRY_IMPORT 1
#define IMAGE_DIRECTORY_ENTRY_RESOURCE 2
#define IMAGE_DIRECTORY_ENTRY_EXCEPTION 3
#define IMAGE_DIRECTORY_ENTRY_SECURITY 4
#define IMAGE_DIRECTORY_ENTRY_BASERELOC 5
#define IMAGE_DIRECTORY_ENTRY_DEBUG 6
#define IMAGE_DIRECTORY_ENTRY_ARCHITECTURE 7
#define IMAGE_DIRECTORY_ENTRY_GLOBALPTR 8
#define IMAGE_DIRECTORY_ENTRY_TLS 9
#define IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG 10
#define IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT 11
#define IMAGE_DIRECTORY_ENTRY_IAT 12
#define IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT 13
#define IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR 14


int clear_list(int type);

typedef struct _IMAGE_FILE_HEADER {
  WORD  Machine;
  WORD  NumberOfSections;
  DWORD TimeDateStamp;
  DWORD PointerToSymbolTable;
  DWORD NumberOfSymbols;
  WORD  SizeOfOptionalHeader;
  WORD  Characteristics;
} IMAGE_FILE_HEADER, *PIMAGE_FILE_HEADER;

typedef struct _IMAGE_DATA_DIRECTORY {
  DWORD VirtualAddress;
  DWORD Size;
} IMAGE_DATA_DIRECTORY, *PIMAGE_DATA_DIRECTORY;

typedef struct _IMAGE_OPTIONAL_HEADER {
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
  IMAGE_DATA_DIRECTORY DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES];
} IMAGE_OPTIONAL_HEADER, *PIMAGE_OPTIONAL_HEADER;

typedef struct _IMAGE_SECTION_HEADER {
  BYTE  Name[IMAGE_SIZEOF_SHORT_NAME];
  union {
    DWORD PhysicalAddress;
    DWORD VirtualSize;
  } Misc;
  DWORD VirtualAddress;
  DWORD SizeOfRawData;
  DWORD PointerToRawData;
  DWORD PointerToRelocations;
  DWORD PointerToLinenumbers;
  WORD  NumberOfRelocations;
  WORD  NumberOfLinenumbers;
  DWORD Characteristics;
} IMAGE_SECTION_HEADER, *PIMAGE_SECTION_HEADER;

typedef struct _IMAGE_DOS_HEADER
{
     WORD e_magic;
     WORD e_cblp;
     WORD e_cp;
     WORD e_crlc;
     WORD e_cparhdr;
     WORD e_minalloc;
     WORD e_maxalloc;
     WORD e_ss;
     WORD e_sp;
     WORD e_csum;
     WORD e_ip;
     WORD e_cs;
     WORD e_lfarlc;
     WORD e_ovno;
     WORD e_res[4];
     WORD e_oemid;
     WORD e_oeminfo;
     WORD e_res2[10];
     LONG e_lfanew;
} IMAGE_DOS_HEADER, *PIMAGE_DOS_HEADER;


typedef struct _IMAGE_NT_HEADERS {
  DWORD                 Signature;
  IMAGE_FILE_HEADER     FileHeader;
  IMAGE_OPTIONAL_HEADER OptionalHeader;
} IMAGE_NT_HEADERS, *PIMAGE_NT_HEADERS;

typedef struct _IMAGE_EXPORT_DIRECTORY
{
	DWORD Characteristics;
	DWORD TimeDateStamp;
	WORD MajorVersion;
	WORD MinorVersion;
	DWORD Name;
	DWORD Base;
	DWORD NumberOfFunctions;
	DWORD NumberOfNames;
	DWORD AddressOfFunctions;     // RVA from base of image
	DWORD AddressOfNames;     // RVA from base of image
	DWORD AddressOfNameOrdinals;  // RVA from base of image
}IMAGE_EXPORT_DIRECTORY, *PIMAGE_EXPORT_DIRECTORY;

typedef enum {
	WINXP_SP2 = 0, WINXP_SP3, WIN7_SP0, WIN7_SP1
} GUEST_OS;

typedef enum{
	Type = 2, Directory = 3, SymbolicLink, Token, Job, Process, Thread, UserApcReserve, IoCompletionReserve, DebugObject,
	Event, EventPair, Mutant, Callback, Semaphore, Timer, Profile, KeyedEvent, WindowStation, Desktop, TpWorkerFactory,
	Adapter, Controller, Device, Driver,IoCompletion, File, TmTm, TmTx, TmRm, TmEn, Section, Session, Key, ALPCPort, PowerRequest,
	WmiGuid, EtwRegistration, EtwConsumer, FilterConnectionPort, FilterCommunicationPort, PcwObject
}W7TYPE_TABLE;

struct api_entry{
	char name[64];
	uint32_t base;
	QLIST_ENTRY (api_entry) loadedlist_entry;
};

struct pe_entry{
	char name[64];
	char fullname[128];
	//uint32_t function_num;
	uint32_t base;
	uint32_t size;
	QLIST_HEAD (apilist_head, api_entry) apilist_head;
	QLIST_ENTRY (pe_entry) loadedlist_entry;
};

struct process_entry {
	char name[NAMESIZE];
	uint32_t EPROC_base_addr; //Base address of the EPROCESS structure _Most imp_
	uint32_t ppid; //Parent process id
	//uint32_t time_created[2]; //Time created
	uint32_t number_of_threads; //Number of active threads
	uint32_t number_of_handles;
	uint32_t table_code;
	uint32_t process_id;
	uint32_t cr3;
	QLIST_HEAD (modlist_head, pe_entry) modlist_head;
	QLIST_ENTRY (process_entry) loadedlist_entry;
};

struct tcb {
	uint32_t _EAX;
	uint32_t _EBX;
	uint32_t _ECX;
	uint32_t _EDX;
};

struct thread_entry {
	//struct process_entry *owning_process;
	uint32_t thread_id;
	uint32_t owning_process_id;
	uint32_t ETHREAD_base_addr; //_Most imp_
	struct tcb* tcb;
	//uint32_t time_created[2];
	QLIST_ENTRY (thread_entry) loadedlist_entry;
};

struct service_entry {
	char name[64];
	uint32_t base;
	uint32_t size;
	QLIST_ENTRY (service_entry) loadedlist_entry;
};

struct file_entry {

	char type[32];
	char filename[64];
	uint32_t file_object_base;
	//uint32_t num_ptr;
	//uint32_t num_hnd;
	QLIST_ENTRY (file_entry) loadedlist_entry;
};

typedef struct {
	uint32_t DLLBASE_OFFSET;
	uint32_t SIZE_OFFSET;
	uint32_t DLLNAME_OFFSET;
	uint32_t PSAPL_OFFSET; // base on struct _EPROCESS
	uint32_t PSAPID_OFFSET;
	uint32_t PSAPNAME_OFFSET;
	uint32_t PSAPPID_OFFSET;
	uint32_t PSAPTHREADS_OFFSET;
	uint32_t PSAPHANDLES_OFFSET;
	uint32_t HANDLE_COUNT_OFFSET;
	uint32_t THREADLH_OFFSET;
	uint32_t THREADCID_OFFSET;
	uint32_t THREADENTRY_OFFSET;
	uint32_t TRAPFRAME_OFFSET;
	uint32_t PEXIT_TIME;
}off_set;

typedef struct os_handle {
	GUEST_OS os_info;
	off_set* offset;
	void (*update_modulelist)();
	int (*update_processlist)();
	void (*update_threadlist)();
	void (*update_filelist)();
	void (*update_symbollist)();
} os_handle;


int find_win7sp0(CPUState *env,uintptr_t insn_handle);
int find_win7sp1(CPUState *env,uintptr_t insn_handle);
int find_winxpsp2(CPUState *env,uintptr_t insn_handle);
int find_winxpsp3(CPUState *env,uintptr_t insn_handle);

void win_vmi_init();


#ifdef __cplusplus
};
#endif
#endif /* RECON_H_ */

//#endif /*CONFIG_VMI_ENABLE*/

