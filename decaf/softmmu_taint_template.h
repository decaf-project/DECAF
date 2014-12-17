/*
 *  Software MMU support
 *
 * Generate helpers used by TCG for qemu_ld/st ops and code load
 * functions.
 *
 * Included from target op helpers and exec.c.
 *
 *  Copyright (c) 2003 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */
/* AWH - Piggyback off of softmmu-template.h for a lot of the size stuff */
#define DATA_SIZE (1 << SHIFT)

/* Get rid of some implicit function declaration warnings */
#include "tainting/taint_memory.h"

static DATA_TYPE glue(glue(taint_slow_ld, SUFFIX), MMUSUFFIX)(target_ulong addr,
                                                        int mmu_idx,
                                                        void *retaddr);
static inline DATA_TYPE glue(taint_io_read, SUFFIX)(target_phys_addr_t physaddr,
                                              target_ulong addr,
                                              void *retaddr)
{
    DATA_TYPE res;
    int index;
    index = (physaddr >> IO_MEM_SHIFT) & (IO_MEM_NB_ENTRIES - 1);
    physaddr = (physaddr & TARGET_PAGE_MASK) + addr;
    env->mem_io_pc = (unsigned long)retaddr;
    if (index > (IO_MEM_NOTDIRTY >> IO_MEM_SHIFT)
            && !can_do_io(env)) {
        cpu_io_recompile(env, retaddr);
    }
    cpu_single_env->tempidx = 0;

    env->mem_io_vaddr = addr;
#if SHIFT <= 2
    res = io_mem_read[index][SHIFT](io_mem_opaque[index], physaddr);
#else
#ifdef TARGET_WORDS_BIGENDIAN
    res = (uint64_t)io_mem_read[index][2](io_mem_opaque[index], physaddr) << 32;
    res |= io_mem_read[index][2](io_mem_opaque[index], physaddr + 4);
#else
    res = io_mem_read[index][2](io_mem_opaque[index], physaddr);
    res |= (uint64_t)io_mem_read[index][2](io_mem_opaque[index], physaddr + 4) << 32;
#endif
#endif /* SHIFT > 2 */
    //res.taint = cpu_single_env->tempidx;
#if 0 // AWH
    if (cpu_single_env->tempidx/*res.taint*/) {
      fprintf(stderr, "MMAP IO %s() -> physaddr: 0x%08x, taint: %u\n", "__taint_io_read", physaddr, cpu_single_env->tempidx);
      //__asm__ ("int $3");
    }
#endif // AWH
    return res;
}

/* handle all cases except unaligned access which span two pages */
DATA_TYPE REGPARM glue(glue(__taint_ld, SUFFIX), MMUSUFFIX)(target_ulong addr,
                                                      int mmu_idx)
{
    DATA_TYPE res;
    int index;
    target_ulong tlb_addr;
    target_phys_addr_t ioaddr;
    unsigned long addend;
    void *retaddr;

    //res.dummy = 0;
#if 0 // AWH
#if DATA_SIZE == 4
fprintf(stderr, "Entry %s() -> addr: 0x%08x mmu_idx: 0x%08x\n", "__taint_ldl", addr, mmu_idx);
#elif DATA_SIZE == 2
fprintf(stderr, "Entry %s() -> addr: 0x%08x mmu_idx: 0x%08x\n", "__taint_ldw", addr, mmu_idx);
#else
fprintf(stderr, "Entry %s() -> addr: 0x%08x mmu_idx: 0x%08x\n", "__taint_ldb", addr, mmu_idx);
#endif
#endif // AWH

    /* test if there is match for unaligned or IO access */
    /* XXX: could done more in memory macro in a non portable way */
    index = (addr >> TARGET_PAGE_BITS) & (CPU_TLB_SIZE - 1);
 redo:
    tlb_addr = env->tlb_table[mmu_idx][index].ADDR_READ;
    if ((addr & TARGET_PAGE_MASK) == (tlb_addr & (TARGET_PAGE_MASK | TLB_INVALID_MASK))) {
        if (tlb_addr & ~TARGET_PAGE_MASK) {
            /* IO access */
            if ((addr & (DATA_SIZE - 1)) != 0)
                goto do_unaligned_access;
            retaddr = GETPC();
            ioaddr = env->iotlb[mmu_idx][index];
            res = glue(taint_io_read, SUFFIX)(ioaddr, addr, retaddr);
        } else if (((addr & ~TARGET_PAGE_MASK) + DATA_SIZE - 1) >= TARGET_PAGE_SIZE) {
            /* slow unaligned access (it spans two pages or IO) */
        do_unaligned_access:
            retaddr = GETPC();
#ifdef ALIGNED_ONLY
            do_unaligned_access(addr, READ_ACCESS_TYPE, mmu_idx, retaddr);
#endif
            res = glue(glue(taint_slow_ld, SUFFIX), MMUSUFFIX)(addr,
                                                         mmu_idx, retaddr);
        } else {
            /* unaligned/aligned access in the same page */
#ifdef ALIGNED_ONLY
            if ((addr & (DATA_SIZE - 1)) != 0) {
                retaddr = GETPC();
                do_unaligned_access(addr, READ_ACCESS_TYPE, mmu_idx, retaddr);
            }
#endif
            addend = env->tlb_table[mmu_idx][index].addend;
            res = glue(glue(ld, USUFFIX), _raw)((uint8_t *)(long)(addr+addend));
            glue(glue(__taint_ld, SUFFIX), _raw)((unsigned long)(addr+addend),addr);
            //Hu-Mem read callback
#ifndef SOFTMMU_CODE_ACCESS
            if(DECAF_is_callback_needed(DECAF_MEM_READ_CB))// host vitual addr+addend
              helper_DECAF_invoke_mem_read_callback(addr,qemu_ram_addr_from_host_nofail((void *)(addr+addend)),DATA_SIZE);
#endif
            //end

        }
    } else {
        /* the page is not in the TLB : fill it */
        retaddr = GETPC();
#ifdef ALIGNED_ONLY
        if ((addr & (DATA_SIZE - 1)) != 0)
            do_unaligned_access(addr, READ_ACCESS_TYPE, mmu_idx, retaddr);
#endif
        tlb_fill(env, addr, READ_ACCESS_TYPE, mmu_idx, retaddr);
        goto redo;
    }
#if 0 // AWH
if (res.taint) {
#if DATA_SIZE == 4
fprintf(stderr, "Return %s() -> addr: 0x%08x, taint: 0x%08x\n", "__taint_ldl", addr, res.taint);
#elif DATA_SIZE == 2
fprintf(stderr, "Return %s() -> addr: 0x%08x, taint: 0x%04x\n", "__taint_ldw", addr, res.taint);
#else
fprintf(stderr, "Return %s() -> addr: 0x%08x, taint: 0x%02x\n", "__taint_ldb", addr, res.taint);
#endif
//__asm__("int $3");
}
#endif // AWH
    return res;
}

/* handle all unaligned cases */
static DATA_TYPE glue(glue(taint_slow_ld, SUFFIX), MMUSUFFIX)(target_ulong addr,
                                                        int mmu_idx,
                                                        void *retaddr)
{
    DATA_TYPE res, res1, res2, taint1, taint2;
    int index, shift;
    target_phys_addr_t ioaddr;
    unsigned long addend;
    target_ulong tlb_addr, addr1, addr2;

    index = (addr >> TARGET_PAGE_BITS) & (CPU_TLB_SIZE - 1);
 redo:
    tlb_addr = env->tlb_table[mmu_idx][index].ADDR_READ;
    if ((addr & TARGET_PAGE_MASK) == (tlb_addr & (TARGET_PAGE_MASK | TLB_INVALID_MASK))) {
        if (tlb_addr & ~TARGET_PAGE_MASK) {
            /* IO access */
            if ((addr & (DATA_SIZE - 1)) != 0)
                goto do_unaligned_access;
            ioaddr = env->iotlb[mmu_idx][index];
            res = glue(taint_io_read, SUFFIX)(ioaddr, addr, retaddr);
        } else if (((addr & ~TARGET_PAGE_MASK) + DATA_SIZE - 1) >= TARGET_PAGE_SIZE) {
        do_unaligned_access:
            /* slow unaligned access (it spans two pages) */
            addr1 = addr & ~(DATA_SIZE - 1);
            addr2 = addr1 + DATA_SIZE;
            res1 = glue(glue(taint_slow_ld, SUFFIX), MMUSUFFIX)(addr1,
                                                          mmu_idx, retaddr);
/* Special case for 32-bit host/guest and a 64-bit load */
#if ((TCG_TARGET_REG_BITS == 32) && (DATA_SIZE == 8))
            taint1 = cpu_single_env->tempidx2;
            taint1 = taint1 << 32;
            taint1 |= cpu_single_env->tempidx;
            //taint1 = cpu_single_env->tempidx | (cpu_single_env->tempidx2 << 32);
#else
            taint1 = cpu_single_env->tempidx;
#endif /* ((TCG_TARGET_REG_BITS == 32) && (DATA_SIZE == 8)) */
            res2 = glue(glue(taint_slow_ld, SUFFIX), MMUSUFFIX)(addr2,
                                                          mmu_idx, retaddr);
#if ((TCG_TARGET_REG_BITS == 32) && (DATA_SIZE == 8))
            taint2 = cpu_single_env->tempidx2;
            taint2 = taint2 << 32;
            taint2 |= cpu_single_env->tempidx;
            //taint2 = cpu_single_env->tempidx | (cpu_single_env->tempidx2 << 32);
#else
            taint2 = cpu_single_env->tempidx;
#endif /* ((TCG_TARGET_REG_BITS == 32) && (DATA_SIZE == 8)) */
            shift = (addr & (DATA_SIZE - 1)) * 8;
#ifdef TARGET_WORDS_BIGENDIAN
            res = (res1 << shift) | (res2 >> ((DATA_SIZE * 8) - shift));
#if ((TCG_TARGET_REG_BITS == 32) && (DATA_SIZE == 8))
            cpu_single_env->tempidx = ((taint1 << shift) | (taint2 >> ((DATA_SIZE * 8) - shift))) & 0xFFFFFFFF;
            cpu_single_env->tempidx2 = (((taint1 << shift) | (taint2 >> ((DATA_SIZE * 8) - shift))) >> 32) & 0xFFFFFFFF;
#else
            cpu_single_env->tempidx = (taint1 << shift) | (taint2 >> ((DATA_SIZE * 8) - shift));
#endif /* ((TCG_TARGET_REG_BITS == 32) && (DATA_SIZE == 8)) */
#else
            res = (res1 >> shift) | (res2 << ((DATA_SIZE * 8) - shift));
#if ((TCG_TARGET_REG_BITS == 32) && (DATA_SIZE == 8))
            cpu_single_env->tempidx = ((taint1 >> shift) | (taint2 << ((DATA_SIZE * 8) - shift))) & 0xFFFFFFFF;
            cpu_single_env->tempidx2 = (((taint1 >> shift) | (taint2 << ((DATA_SIZE * 8) - shift))) >> 32) & 0xFFFFFFFF;
#else
            cpu_single_env->tempidx = (taint1 >> shift) | (taint2 << ((DATA_SIZE * 8) - shift));
#endif /* ((TCG_TARGET_REG_BITS == 32) && (DATA_SIZE == 8)) */
#endif /* TARGET_WORDS_BIGENDIAN */
            res = (DATA_TYPE)(res);
            //cpu_single_env->tempidx = (DATA_TYPE)(cpu_single_env->tempidx);
        } else {
            /* unaligned/aligned access in the same page */
            addend = env->tlb_table[mmu_idx][index].addend;
            res = glue(glue(ld, USUFFIX), _raw)((uint8_t *)(long)(addr+addend));
            glue(glue(__taint_ld, SUFFIX), _raw)((unsigned long)(addr+addend),addr);
            //Hu-Mem read callback
#ifndef SOFTMMU_CODE_ACCESS
            if(DECAF_is_callback_needed(DECAF_MEM_READ_CB))
              helper_DECAF_invoke_mem_read_callback(addr,qemu_ram_addr_from_host_nofail((void *)(addr+addend)),DATA_SIZE);
#endif
            //end

        }
    } else {
        /* the page is not in the TLB : fill it */
        tlb_fill(env, addr, READ_ACCESS_TYPE, mmu_idx, retaddr);
        goto redo;
    }
#if 0 // AWH
#if DATA_SIZE == 4
fprintf(stderr, "Return %s() -> addr: 0x%08x, taint: %u\n", "__taint_slow_ldl", addr, res.taint);
#elif DATA_SIZE == 2
fprintf(stderr, "Return %s() -> addr: 0x%08x, taint: %u\n", "__taint_slow_ldw", addr, res.taint);
#else
fprintf(stderr, "Return %s() -> addr: 0x%08x, taint: %u\n", "__taint_slow_ldb", addr, res.taint);
#endif
#endif // AWH
    return res;
}

#ifndef SOFTMMU_CODE_ACCESS

static void glue(glue(taint_slow_st, SUFFIX), MMUSUFFIX)(target_ulong addr,
                                                   DATA_TYPE val,
                                                   int mmu_idx,
                                                   void *retaddr);

static inline void glue(taint_io_write, SUFFIX)(target_phys_addr_t physaddr,
                                          DATA_TYPE val,
                                          target_ulong addr,
                                          void *retaddr)
{
    int index;
    index = (physaddr >> IO_MEM_SHIFT) & (IO_MEM_NB_ENTRIES - 1);
    physaddr = (physaddr & TARGET_PAGE_MASK) + addr;
    if (index > (IO_MEM_NOTDIRTY >> IO_MEM_SHIFT)
            && !can_do_io(env)) {
        cpu_io_recompile(env, retaddr);
    }

    env->mem_io_vaddr = addr;
    env->mem_io_pc = (unsigned long)retaddr;
#if SHIFT <= 2
    io_mem_write[index][SHIFT](io_mem_opaque[index], physaddr, val);
    //Hu-for io mem not dirty
#ifndef SOFTMMU_CODE_ACCESS
    if((index == 3)&DECAF_is_callback_needed(DECAF_MEM_WRITE_CB)) { //IO_MEM_NOTDIRTY
        helper_DECAF_invoke_mem_write_callback(addr,physaddr,DATA_SIZE);
    }
#endif
    //end
#else
#ifdef TARGET_WORDS_BIGENDIAN
    io_mem_write[index][2](io_mem_opaque[index], physaddr, val >> 32);

    //Hu-for io mem not dirty
#ifndef SOFTMMU_CODE_ACCESS
    if((index == 3)&DECAF_is_callback_needed(DECAF_MEM_WRITE_CB)) { //IO_MEM_NOTDIRTY
        helper_DECAF_invoke_mem_write_callback(addr,physaddr,DATA_SIZE);
    }
#endif
    //end
    io_mem_write[index][2](io_mem_opaque[index], physaddr + 4, val);
    //Hu-for io mem not dirty
#ifndef SOFTMMU_CODE_ACCESS
    if((index == 3)&DECAF_is_callback_needed(DECAF_MEM_WRITE_CB)) { //IO_MEM_NOTDIRTY
        helper_DECAF_invoke_mem_write_callback(addr,physaddr,DATA_SIZE);
    }
#endif
    //end

#else
    io_mem_write[index][2](io_mem_opaque[index], physaddr, val);
    //Hu-for io mem not dirty
#ifndef SOFTMMU_CODE_ACCESS
    if((index == 3)&DECAF_is_callback_needed(DECAF_MEM_WRITE_CB)) { //IO_MEM_NOTDIRTY
        helper_DECAF_invoke_mem_write_callback(addr,physaddr,DATA_SIZE);
    }
#endif
    //end

    io_mem_write[index][2](io_mem_opaque[index], physaddr + 4, val >> 32);
    //Hu-for io mem not dirty
#ifndef SOFTMMU_CODE_ACCESS
    if((index == 3)&DECAF_is_callback_needed(DECAF_MEM_WRITE_CB)) { //IO_MEM_NOTDIRTY
        helper_DECAF_invoke_mem_write_callback(addr,physaddr,DATA_SIZE);
    }
#endif
    //end
#endif
#endif /* SHIFT > 2 */
    if (index == (IO_MEM_NOTDIRTY>>IO_MEM_SHIFT))
      glue(glue(__taint_st, SUFFIX), _raw_paddr)(physaddr,addr);
    
    /* Clean tempidx */  
    //cpu_single_env->tempidx = 0;
}

void REGPARM glue(glue(__taint_st, SUFFIX), MMUSUFFIX)(target_ulong addr,
                                                 DATA_TYPE val,
                                                 int mmu_idx)
{
    target_phys_addr_t ioaddr;
    unsigned long addend;
    target_ulong tlb_addr;
    void *retaddr;
    int index;

#if 0 // AWH
if (taint) {
#if DATA_SIZE == 4
fprintf(stderr, "Start  %s() -> addr: 0x%08x, data: 0x%08x, mmu_idx: %d, taint: 0x%08x\n", "__taint_stl", addr, val, mmu_idx, taint);
#elif DATA_SIZE == 2
fprintf(stderr, "Start  %s() -> addr: 0x%08x, data: 0x%08x, mmu_idx: %d, taint: 0x%08x\n", "__taint_stw", addr, val, mmu_idx, taint);
#else
fprintf(stderr, "Start  %s() -> addr: 0x%08x, data: 0x%08x, mmu_idx: %d, taint: 0x%08x\n", "__taint_stb", addr, val, mmu_idx, taint);
#endif
//__asm__("int $3");
}
#endif // AWH
    index = (addr >> TARGET_PAGE_BITS) & (CPU_TLB_SIZE - 1);
 redo:
    tlb_addr = env->tlb_table[mmu_idx][index].addr_write;
    if ((addr & TARGET_PAGE_MASK) == (tlb_addr & (TARGET_PAGE_MASK | TLB_INVALID_MASK))) {
        if (tlb_addr & ~TARGET_PAGE_MASK) {
            /* IO access */
            if ((addr & (DATA_SIZE - 1)) != 0)
                goto do_unaligned_access;
            retaddr = GETPC();
            ioaddr = env->iotlb[mmu_idx][index];
            glue(taint_io_write, SUFFIX)(ioaddr, val, addr, retaddr);
        } else if (((addr & ~TARGET_PAGE_MASK) + DATA_SIZE - 1) >= TARGET_PAGE_SIZE) {
        do_unaligned_access:
            retaddr = GETPC();
#ifdef ALIGNED_ONLY
            do_unaligned_access(addr, 1, mmu_idx, retaddr);
#endif
            glue(glue(taint_slow_st, SUFFIX), MMUSUFFIX)(addr, val, mmu_idx, retaddr);
        } else {
            /* aligned/unaligned access in the same page */
#ifdef ALIGNED_ONLY
            if ((addr & (DATA_SIZE - 1)) != 0) {
                retaddr = GETPC();
                do_unaligned_access(addr, 1, mmu_idx, retaddr);
            }
#endif
            addend = env->tlb_table[mmu_idx][index].addend;
            glue(glue(st, SUFFIX), _raw)((uint8_t *)(long)(addr+addend), val);
            glue(glue(__taint_st, SUFFIX), _raw)((unsigned long)(addr+addend),addr);
            //Hu-Mem write callback
#ifndef SOFTMMU_CODE_ACCESS
            if(DECAF_is_callback_needed(DECAF_MEM_WRITE_CB))
              helper_DECAF_invoke_mem_write_callback(addr,qemu_ram_addr_from_host_nofail((void *)(addr+addend)),DATA_SIZE);
#endif
            //end
        }
    } else {
        /* the page is not in the TLB : fill it */
        retaddr = GETPC();
#ifdef ALIGNED_ONLY
        if ((addr & (DATA_SIZE - 1)) != 0)
            do_unaligned_access(addr, 1, mmu_idx, retaddr);
#endif
        tlb_fill(env, addr, 1, mmu_idx, retaddr);
        goto redo;
    }
}

/* handles all unaligned cases */
static void glue(glue(taint_slow_st, SUFFIX), MMUSUFFIX)(target_ulong addr,
                                                   DATA_TYPE val,
                                                   int mmu_idx,
                                                   void *retaddr)
{
    target_phys_addr_t ioaddr;
    unsigned long addend;
    target_ulong tlb_addr;
    int index, i;
    DATA_TYPE backup_taint;

    index = (addr >> TARGET_PAGE_BITS) & (CPU_TLB_SIZE - 1);
 redo:
    tlb_addr = env->tlb_table[mmu_idx][index].addr_write;
    if ((addr & TARGET_PAGE_MASK) == (tlb_addr & (TARGET_PAGE_MASK | TLB_INVALID_MASK))) {
        if (tlb_addr & ~TARGET_PAGE_MASK) {
            /* IO access */
            if ((addr & (DATA_SIZE - 1)) != 0)
                goto do_unaligned_access;
            ioaddr = env->iotlb[mmu_idx][index];
            glue(taint_io_write, SUFFIX)(ioaddr, val, addr, retaddr);
        } else if (((addr & ~TARGET_PAGE_MASK) + DATA_SIZE - 1) >= TARGET_PAGE_SIZE) {
        do_unaligned_access:
            /* AWH - Backup the taint held in tempidx and tempidx2 and
               setup tempidx for each of these single-byte stores */
/* Special case for 32-bit host/guest and a 64-bit load */
#if ((TCG_TARGET_REG_BITS == 32) && (DATA_SIZE == 8))
            backup_taint = cpu_single_env->tempidx2;
            backup_taint = backup_taint << 32;
            backup_taint |= cpu_single_env->tempidx;
            //backup_taint = cpu_single_env->tempidx | (cpu_single_env->tempidx2 << 32);
#else
            backup_taint = cpu_single_env->tempidx;
#endif /* ((TCG_TARGET_REG_BITS == 32) && (DATA_SIZE == 8)) */

            /* XXX: not efficient, but simple */
            /* Note: relies on the fact that tlb_fill() does not remove the
             * previous page from the TLB cache.  */
            for(i = DATA_SIZE - 1; i >= 0; i--) {
#ifdef TARGET_WORDS_BIGENDIAN
                cpu_single_env->tempidx = backup_taint >> (((DATA_SIZE - 1) * 8) - (i * 8));
                glue(taint_slow_stb, MMUSUFFIX)(addr + i, val >> (((DATA_SIZE - 1) * 8) - (i * 8)),
                                          mmu_idx, retaddr);
#else
                cpu_single_env->tempidx = backup_taint >> (i * 8);
                glue(taint_slow_stb, MMUSUFFIX)(addr + i, val >> (i * 8), mmu_idx, retaddr);
#endif
            }
        } else {
            /* aligned/unaligned access in the same page */
            addend = env->tlb_table[mmu_idx][index].addend;
            glue(glue(st, SUFFIX), _raw)((uint8_t *)(long)(addr+addend), val);
            glue(glue(__taint_st, SUFFIX), _raw)((unsigned long)(addr+addend),addr);
            //Hu-Mem read callback
#if defined(ADD_MEM_CB)
            if(DECAF_is_callback_needed(DECAF_MEM_WRITE_CB))
              helper_DECAF_invoke_mem_write_callback(addr,qemu_ram_addr_from_host_nofail((void*)(addr+addend)),DATA_SIZE);
#endif
            //end

        }
    } else {
        /* the page is not in the TLB : fill it */
        tlb_fill(env, addr, 1, mmu_idx, retaddr);
        goto redo;
    }
}

#endif /* !defined(SOFTMMU_CODE_ACCESS) */

