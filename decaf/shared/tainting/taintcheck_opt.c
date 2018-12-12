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
*/

#include "config.h"
#include <dlfcn.h>
#include <assert.h>
#include <sys/queue.h>
#include "hw/hw.h"
#include "qemu-common.h"
#include "sysemu.h"
#include "hw/hw.h" /* {de,}register_savevm */
#include "cpu.h"
#include "DECAF_main.h"
#include "DECAF_main_internal.h"
#include "shared/tainting/taintcheck_opt.h"
//#include "shared/tainting/taintcheck.h"
#include "shared/DECAF_vm_compress.h"
#include "shared/tainting/taint_memory.h"
#include "tcg.h" // tcg_abort()

#ifdef CONFIG_TCG_TAINT

uint8_t nic_bitmap[1024 * 32 ]; //!<bitmap for nic

#ifndef min
#define min(X,Y) ((X) < (Y) ? (X) : (Y))
#endif


//each disk sector is 512 bytes
typedef struct disk_record {
  const void *bs;
  uint64_t index;
  uint8_t bitmap[512];
  LIST_ENTRY(disk_record) entry;
} disk_record_t;

#define DISK_HTAB_SIZE (1024)
static LIST_HEAD(disk_record_list_head, disk_record)
        disk_record_heads[DISK_HTAB_SIZE];
static uint8_t zero_mem[512];

static int taintcheck_taint_disk(const uint8_t *taint, const uint64_t index,
    const int offset, const int size, const void *bs)
{
    struct disk_record_list_head *head =
        &disk_record_heads[index & (DISK_HTAB_SIZE - 1)];
    disk_record_t *drec,  *new_drec;
    int found = 0;

    assert(offset + size <= 512);

    int is_tainted = (memcmp(taint, zero_mem, size) != 0);

    LIST_FOREACH(drec, head, entry) {
        if (drec->index == index && drec->bs == bs) {
            found = 1;
            break;
        }
        if (drec->index > index)
            break;
    }
    if (!found) {
        if (!is_tainted)
            return 0;

        if (!(new_drec = g_malloc0((size_t)sizeof(disk_record_t))))
            return -1;

        new_drec->index = index;
        new_drec->bs = bs;
        memcpy(&new_drec->bitmap[offset], taint, size);
        LIST_INSERT_HEAD(head, new_drec, entry);
    } else {
        memcpy(&drec->bitmap[offset], taint, size);
        if (!is_tainted && !memcmp(drec->bitmap, zero_mem, sizeof(drec->bitmap))) {
            LIST_REMOVE(drec, entry);
            g_free(drec);
        }
    }
    return 0;
}

static void taintcheck_disk_check(uint8_t *taint, const uint64_t index, const int offset,
                               const int size, const void *bs)
{
    struct disk_record_list_head *head =
        &disk_record_heads[index & (DISK_HTAB_SIZE - 1)];
    disk_record_t *drec;
    int found = 0;

    assert(offset + size <= 512);

    LIST_FOREACH(drec, head, entry) {
        if (drec->index == index && drec->bs == bs) {
            found = 1;
            break;
        }
        if (drec->index > index)
            break;
    }

    if (!found) {
        bzero(taint, size);
        return;
    }

    memcpy(taint, &drec->bitmap[offset], size);
}

int taintcheck_init(void)
{
    int i;
    for (i = 0; i < DISK_HTAB_SIZE; i++)
        LIST_INIT(&disk_record_heads[i]);

    bzero(zero_mem, sizeof(zero_mem));
    return 0;
}

void taintcheck_cleanup(void)
{
  //clean nic buffer
  bzero(nic_bitmap, sizeof(nic_bitmap));
  //clean disk
  //TODO:
  // AWH - deregister_savevm(), first parm NULL
  unregister_savevm(NULL, "taintcheck", 0);
}

void taintcheck_chk_hdout(const int size, const int64_t sect_num,
  const uint32_t offset, const void *s)
{
     taintcheck_taint_disk((uint8_t *)&cpu_single_env->tempidx, sect_num, offset, size, s);
}

void taintcheck_chk_hdin(const int size, const int64_t sect_num,
  const uint32_t offset, const void *s)
{
    taintcheck_disk_check((uint8_t *)&cpu_single_env->tempidx, sect_num,
            offset, size, s);
}

void taintcheck_chk_hdwrite(const ram_addr_t paddr, const int size, const int64_t sect_num, const void *s)
{
    //We assume size is multiple of 512, because this function is used in DMA, and paddr is also aligned.
    int i;
    uint8_t taint[512];
	if (!taint_tracking_enabled) {
		return ;
	}
    for (i = 0; i < size; i += 512) {
        taint_mem_check(paddr+i, 512, taint);
        taintcheck_taint_disk(taint, sect_num + i/512, 0, 512, s);
    }
}


void taintcheck_chk_hdread(const ram_addr_t paddr, const int size, const int64_t sect_num, const void *s) {
    //We assume size is multiple of 512, because this function is used in DMA, and paddr is also aligned.
	int i;
    uint8_t taint[512];
	if (!taint_tracking_enabled) {
		return ;
	}
	for (i = 0; i < size; i += 512) {
        taintcheck_disk_check(taint, sect_num + i/512, 0, size, s);
        taint_mem(paddr+i, 512, taint);
	}
}

/// \brief check the taint of a memory buffer given the start virtual address.
///
/// \param vaddr the virtual address of the memory buffer
/// \param size  the memory buffer size
/// \param taint the output taint array, it must hold at least [size] bytes
///  \return 0 means success, -1 means failure
int  taintcheck_check_virtmem(gva_t vaddr, uint32_t size, uint8_t * taint)
{
	gpa_t paddr = 0, offset;
	uint32_t size1, size2;
	// uint8_t taint=0;
	CPUState *env;
	env = cpu_single_env ? cpu_single_env : first_cpu;

	// AWH - If tainting is disabled, return no taint
	if (!taint_tracking_enabled) {
		bzero(taint, size);
		return 0;
	}

	paddr = DECAF_get_phys_addr(env,vaddr);
	if(paddr == -1) return -1;

	offset = vaddr& ~TARGET_PAGE_MASK;
	if(offset+size > TARGET_PAGE_SIZE) {
		size1 = TARGET_PAGE_SIZE-offset;
		size2 = size -size1;
	} else
		size1 = size, size2 = 0;

	taint_mem_check(paddr, size1, taint);
	if(size2) {
		paddr = DECAF_get_phys_addr(env, (vaddr&TARGET_PAGE_MASK) + TARGET_PAGE_SIZE);
		if(paddr == -1)
			return -1;

		taint_mem_check(paddr, size2, (uint8_t*)(taint+size1));
	}

	return 0;
}


/// \brief set taint for a memory buffer given the start virtual address.
///
/// \param vaddr the virtual address of the memory buffer
/// \param size  the memory buffer size
/// \param taint the taint array, it must hold at least [size] bytes
/// \return 0 means success, -1 means failure
int  taintcheck_taint_virtmem(gva_t vaddr, uint32_t size, uint8_t * taint)
{
	gpa_t paddr = 0, offset;
	uint32_t size1, size2;
	// uint8_t taint=0;
	CPUState *env;
	env = cpu_single_env ? cpu_single_env : first_cpu;

	// AWH - If tainting is disabled, return no taint
	if (!taint_tracking_enabled) {
		return 0;
	}

	paddr = DECAF_get_phys_addr(env,vaddr);
	if(paddr == -1) return -1;

	offset = vaddr& ~TARGET_PAGE_MASK;
	if(offset+size > TARGET_PAGE_SIZE) {
		size1 = TARGET_PAGE_SIZE-offset;
		size2 = size -size1;
	} else
		size1 = size, size2 = 0;

	taint_mem(paddr, size1, taint);
	if(size2) {
		paddr = DECAF_get_phys_addr(env, (vaddr&TARGET_PAGE_MASK) + TARGET_PAGE_SIZE);
		if(paddr == -1)
			return -1;

		taint_mem(paddr, size2, (uint8_t*)(taint+size1));
	}

	return 0;
}



void taintcheck_nic_writebuf(const uint32_t addr, const int size, const uint8_t * taint)
{
	memcpy(&nic_bitmap[addr], taint, size);
}

void taintcheck_nic_readbuf(const uint32_t addr, const int size, uint8_t *taint)
{
  memcpy(taint, &nic_bitmap[addr], size);
}

void taintcheck_nic_cleanbuf(const uint32_t addr, const int size)
{
	memset(&nic_bitmap[addr], 0, size);
}

#endif
