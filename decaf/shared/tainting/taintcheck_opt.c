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
#include "shared/tainting/tainting.h"
#include "shared/tainting/taintcheck_opt.h"
//#include "shared/tainting/taintcheck.h"
#include "shared/DECAF_vm_compress.h"
#include "shared/tainting/taint_memory.h"
#include "tcg.h" // tcg_abort()

/*uint64_t*/uint8_t nic_bitmap[1024 * 32 /*/ 64*/]; //!<bitmap for nic

#ifndef min
#define min(X,Y) ((X) < (Y) ? (X) : (Y))
#endif

typedef struct disk_record{
  void *bs;
  uint64_t index;
  uint64_t bitmap;
  LIST_ENTRY(disk_record) entry;
  uint8_t records[0];
} disk_record_t;

#define DISK_HTAB_SIZE (1024)
static LIST_HEAD(disk_record_list_head, disk_record)
        disk_record_heads[DISK_HTAB_SIZE];

int taintcheck_taint_disk(const uint64_t index, const uint32_t taint, 
                          const int offset, const int size, const void *bs)
{
  struct disk_record_list_head *head =
      &disk_record_heads[index & (DISK_HTAB_SIZE - 1)];
  disk_record_t *drec,  *new_drec;
  int found = 0;
  // AWH int size2 = 0;
  uint64_t taint2 = 0;

  if (taint & 0x000000FF) taint2 |= 1;
  if (taint & 0x0000FF00) taint2 |= 2;
  if (taint & 0x00FF0000) taint2 |= 4;
  if (taint & 0xFF000000) taint2 |= 8;

  //if (taint)
  //  fprintf(stderr, "taintcheck_taint_disk() taint -> 0x%08x\n", taint);

#if 0 // AWH
  if (offset + size > 64) {
    size = 64 - offset, taint &= size_to_mask(size);
    size2 = offset + size - 64;
    taint2 = taint >> offset;
  }
#endif // AWH
  LIST_FOREACH(drec, head, entry) {
    if (drec->index == index && drec->bs == bs) {
      found = 1;
      break;
    }
    if (drec->index > index)
      break;
  }
  if (!found) {
    if (!taint)
      return 0;

//fprintf(stderr, "taintcheck_taint_disk() -> Not found w/ taint\n");
    if (!(new_drec = g_malloc0((size_t)sizeof(disk_record_t) /*+
                              64 * temu_plugin->taint_record_size*/)))
      return 0;

    new_drec->index = index;
    new_drec->bs = bs;
    new_drec->bitmap = taint2 << offset;
    LIST_INSERT_HEAD(head, new_drec, entry);
//fprintf(stderr, "taintcheck_taint_disk() -> Adding new taint record\n");
  }
  else {
//fprintf(stderr, "taintcheck_taint_disk() -> Changing taint record\n");
    drec->bitmap &= ~(size_to_mask(size) << offset);
    if (taint) {
      drec->bitmap |= taint2 << offset;
      /*memcpy(drec->records + offset * temu_plugin->taint_record_size,
             record, size * temu_plugin->taint_record_size);*/
    }
    else if (!drec->bitmap) {
      LIST_REMOVE(drec, entry);
      g_free(drec);
    }
  }
#if 0 // AWH
  if (size2)
    taintcheck_taint_disk(index + 1, taint2, 0, size2,
                          /*record + size * temu_plugin->taint_record_size,*/
                          bs);
#endif // AWH
  return 0;
}

uint32_t taintcheck_disk_check(const uint64_t index, const int offset, 
                               const int size, const void *bs)
{
  //if(!TEMU_emulation_started) return 0;

  struct disk_record_list_head *head =
      &disk_record_heads[index & (DISK_HTAB_SIZE - 1)];
  disk_record_t *drec;
  int found = 0;
  uint64_t taint;
  uint32_t retval = 0;
  uint32_t ourSize = size;
  if (offset + size > 64)
    ourSize = 64 - offset, taint &= size_to_mask(size);   //fixme:ignore the unalignment

  LIST_FOREACH(drec, head, entry) {
    if (drec->index == index && drec->bs == bs) {
      found = 1;
      break;
    }
    if (drec->index > index)
      break;
  }

  if (!found)
    return 0;

  taint = (drec->bitmap >> offset) & size_to_mask(ourSize);
  if (taint & 1) retval |= 0x000000FF;
  if (taint & 2) retval |= 0x0000FF00;
  if (taint & 4) retval |= 0x00FF0000;
  if (taint & 8) retval |= 0xFF000000;
  //fprintf(stderr, "taintcheck_disk_check() -> taint 0x%08x\n", retval);
    //memcpy(record, drec->records + offset * temu_plugin->taint_record_size,
    //       size * temu_plugin->taint_record_size);
  return retval;
}

int taintcheck_init(void)
{
  int i;
  for (i = 0; i < DISK_HTAB_SIZE; i++)
    LIST_INIT(&disk_record_heads[i]);

  // AWH assert(tpage_table == NULL); //make sure it is not double created
  // AWH tpage_table = (tpage_entry_t **) qemu_malloc((ram_size/64) * sizeof(void*));

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

int taintcheck_chk_hdout(const int size, const int64_t sect_num,
  const uint32_t offset, const void *s)
{
#ifdef CONFIG_TCG_TAINT
  //uint8_t taint_rec;
  int taint = cpu_single_env->tempidx;
  if (size > 4) tcg_abort();

  //taint_rec = taint_reg_check_slow(reg, 0, size);
  taintcheck_taint_disk(sect_num * 8 + offset / 64, taint, offset & 63,
                        size,
                        /*regs_records +
                        reg * temu_plugin->taint_record_size,*/ s);
#endif /* CONFIG_TCG_TAINT */
  return 0;
}

int taintcheck_chk_hdin(const int size, const int64_t sect_num,
  const uint32_t offset, const void *s)
{
#ifdef CONFIG_TCG_TAINT
  /*taint_rec*/ cpu_single_env->tempidx =
      taintcheck_disk_check(sect_num * 8 + offset / 64, offset & 63, size,
                            /*records,*/ s);
#endif /*CONFIG_TCG_TAINT*/
  return 0;
}

int taintcheck_chk_hdwrite(const ram_addr_t paddr,unsigned long vaddr, const int size,
  const int64_t sect_num, const void *s)
{
#ifdef CONFIG_TCG_TAINT
  uint32_t i;

  if ((paddr & 63))
    return 0;

  for (i = paddr; i < paddr + size; i += 4) {
    __taint_ldl_raw_paddr(i, vaddr+i-paddr);
    //if (cpu_single_env->tempidx) fprintf(stderr, "taintcheck_chk_hdwrite() -> Writing taint 0x%08x to disk\n", cpu_single_env->tempidx);
    taintcheck_taint_disk(sect_num * 8 + (i - paddr) / 64,
                          /*(entry) ? entry->bitmap[((paddr & 63) >> 2)] : 0*/cpu_single_env->tempidx, 0, 4/*size*/,
                          /*(entry) ? entry->records : NULL,*/ s);
  } // end for
#endif /* CONFIG_TCG_TAINT */
  return 0;
}

int taintcheck_chk_hdread(const ram_addr_t paddr,unsigned long vaddr, const int size,
		const int64_t sect_num, const void *s) {
#ifdef CONFIG_TCG_TAINT
	unsigned long i;
	for (i = paddr; i < paddr + size; i += 4) {
		cpu_single_env->tempidx = taintcheck_disk_check(
				sect_num * 8 + (i - paddr) / 64, 0, 4, s);
		__taint_stl_raw_paddr(i, vaddr+i-paddr);
	}
#endif /* CONFIG_TCG_TAINT */
	return 0;
}

#ifdef CONFIG_TCG_TAINT

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

#endif //CONFIG_TCG_TAINT
