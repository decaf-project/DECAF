/* This file must be included from target-arm/translate.c */

/*****
 *****
 *****
 *****  C O N F I G _ M E M C H E C K
 *****
 *****
 *****/

#ifdef CONFIG_MEMCHECK

/*
 * Memchecker addition in this module is intended to inject qemu callback into
 * translated code for each BL/BLX, as well as BL/BLX returns. These callbacks
 * are used to build calling stack of the thread in order to provide better
 * reporting on memory access violations. Although this may seem as something
 * that may gratly impact the performance, in reality it doesn't. Overhead that
 * is added by setting up callbacks and by callbacks themselves is neglectable.
 * On the other hand, maintaining calling stack can indeed add some perf.
 * overhead (TODO: provide solid numbers here).
 * One of the things to watch out with regards to injecting callbacks, is
 * consistency between intermediate code generated for execution, and for guest
 * PC address calculation. If code doesn't match, a segmentation fault is
 * guaranteed.
 */

#include "memcheck/memcheck_proc_management.h"
#include "memcheck/memcheck_api.h"

/* Array of return addresses detected in gen_intermediate_code_internal. */
AddrArray   ret_addresses = { 0 };

/* Checks if call stack collection is enabled for the given context.
 * We collect call stack only for the user mode (both, code and CPU), and on
 * condition that memory checking, and call collection are enabled. It also
 * seems that collecting stack for the linker code is excessive, as it doesn't
 * provide much useful info for the memory checker.
 * Return:
 *  boolean: 1 if stack collection is enabled for the given context, or 0 if
 *  it's not enabled.
 */
static inline int
watch_call_stack(DisasContext *s)
{
    if (!memcheck_enabled || !memcheck_watch_call_stack) {
        return 0;
    }

#ifndef CONFIG_USER_ONLY
    if (!s->user) {
        /* We're not interested in kernel mode CPU stack. */
        return 0;
    }
#endif  // CONFIG_USER_ONLY

    /* We're not interested in kernel code stack (pc >= 0xC0000000).
     * Android specific: We're also not interested in android linker stack
     * (0xB0000000 - 0xB00FFFFF) */
    if (s->pc >= 0xC0000000 || (0xB0000000 <= s->pc && s->pc <= 0xB00FFFFF)) {
        return 0;
    }
    return 1;
}

/* Checks if given ARM instruction is BL, or BLX.
 * Return:
 *  boolean: 1 if ARM instruction is BL/BLX, or 0 if it's not.
 */
static inline int
is_arm_bl_or_blx(uint32_t insn)
{
    /* ARM BL  (immediate): xxxx 1011 xxxx xxxx xxxx xxxx xxxx xxxx
     * ARM BLX (immediate): 1111 101x xxxx xxxx xxxx xxxx xxxx xxxx
     * ARM BLX (register):  xxxx 0001 0010 xxxx xxxx xxxx 0011 xxxx
     */
    if ((insn & 0x0F000000) == 0x0B000000 ||    // ARM BL (imm)
        (insn & 0xFE000000) == 0xFA000000 ||    // ARM BLX (imm)
        (insn & 0x0FF000F0) == 0x12000030) {    // ARM BLX (reg)
        return 1;
    }
    return 0;
}

/* Checks if given THUMB instruction is BL, or BLX.
 * Param:
 *  insn - THUMB instruction to check.
 *  pc - Emulated PC address for the instruction.
 *  ret_off - If insn is BL, or BLX, upon return ret_off contains
 *      instruction's byte size. If instruction is not BL, or BLX, content of
 *      this parameter is undefined on return.
 * Return:
 *  boolean: 1 if THUMB instruction is BL/BLX, or 0 if it's not.
 */
static inline int
is_thumb_bl_or_blx(uint16_t insn, target_ulong pc, target_ulong* ret_off)
{
    /* THUMB BLX(register):      0100 0111 1xxx xxxx
     * THUMB BL(1-stimmediate):  1111 0xxx xxxx xxxx
     * THUMB BLX(1-stimmediate): 1111 0xxx xxxx xxxx
     */
    if ((insn & 0xFF80) == 0x4780) {            // THUMB BLX(reg)
        *ret_off = 2;
        return 1;
    } else if ((insn & 0xF800) == 0xF000) {     // THUMB BL(X)(imm)
        // This is a 32-bit THUMB. Get the second half of the instuction.
        insn = lduw_code(pc + 2);
        if ((insn & 0xC000) == 0xC000) {
            *ret_off = 4;
            return 1;
        }
    }
    return 0;
}

/* Registers a return address detected in gen_intermediate_code_internal.
 * NOTE: If return address has been registered as new in this routine, this will
 * cause invalidation of all existing TBs that contain translated code for that
 * address.
 * NOTE: Before storing PC address in the array, we convert it from emulated
 * address to a physical address. This way we deal with emulated addresses
 * overlapping for different processes.
 * Param:
 *  env - CPU state environment.
 *  addr - Return address to register.
 * Return:
 *  1  - Address has been registered in this routine.
 *  -1 - Address has been already registered before.
 *  0  - Insufficient memory.
 */
static int
register_ret_address(CPUState* env, target_ulong addr)
{
    int ret;
    if ((0x90000000 <= addr && addr <= 0xBFFFFFFF)) {
        /* Address belongs to a module that always loads at this fixed address.
         * So, we can keep this address in the global array. */
        ret = addrarray_add(&ret_addresses, get_phys_addr_code(env, addr));
    } else {
        ret = addrarray_add(&ret_addresses, get_phys_addr_code(env, addr));
    }
    assert(ret != 0);

    if (ret == 1) {
        /* If this ret address has been added to the array, we need to make sure
         * that all TBs that contain translated code for that address are
         * invalidated. This will force retranslation of that code, which will
         * make sure that our ret callback is set. This is also important part
         * in keeping consistency between translated code, and intermediate code
         * generated for guest PC calculation. If we don't invalidate TBs, and
         * PC calculation code is generated, there will be inconsistency due to
         * the fact that TB code doesn't contain ret callback, while PC calc
         * code contains it. This inconsistency will lead to an immanent
         * segmentation fault.*/
        TranslationBlock* tb;
        const target_ulong phys_pc = get_phys_addr_code(env, addr);
        const target_ulong phys_page1 = phys_pc & TARGET_PAGE_MASK;

        for(tb = tb_phys_hash[tb_phys_hash_func(phys_pc)]; tb != NULL;
            tb = tb->phys_hash_next) {
            if (tb->pc == addr && tb->page_addr[0] == phys_page1) {
                tb_phys_invalidate(tb, -1);
            }
        }
    }
    return ret;
}

/* Checks if given address is recognized as a return address.
 * Return:
 *  boolean: 1 if if given address is recognized as a return address,
 *  or 0 if it's not.
 */
static inline int
is_ret_address(CPUState* env, target_ulong addr)
{
    if ((0x90000000 <= addr && addr <= 0xBFFFFFFF)) {
        return addrarray_check(&ret_addresses, get_phys_addr_code(env, addr));
    } else {
        return addrarray_check(&ret_addresses, get_phys_addr_code(env, addr));
    }
}

/* Adds "on_call" callback into generated intermediate code. */
static inline void
set_on_call(target_ulong pc, target_ulong ret)
{
    TCGv_ptr tmp_pc = tcg_const_ptr(pc & ~1);
    TCGv_ptr tmp_ret = tcg_const_ptr(ret & ~1);

    gen_helper_on_call(tmp_pc, tmp_ret);

    tcg_temp_free_ptr(tmp_ret);
    tcg_temp_free_ptr(tmp_pc);
}

/* Adds "on_ret" callback into generated intermediate code. */
static inline void
set_on_ret(target_ulong ret)
{
    TCGv_ptr tmp_ret = tcg_const_ptr(ret & ~1);

    gen_helper_on_ret(tmp_ret);

    tcg_temp_free_ptr(tmp_ret);
}


#  define ANDROID_WATCH_CALLSTACK_ARM(s) \
    if (watch_call_stack(s)) { \
        if (is_ret_address(env, s->pc)) { \
            set_on_ret(s->pc); \
        } \
        if (is_arm_bl_or_blx(insn)) { \
            set_on_call(s->pc, s->pc + 4); \
            if (!s->search_pc) { \
                register_ret_address(env, s->pc + 4); \
            } \
        } \
    }

#  define ANDROID_WATCH_CALLSTACK_THUMB(s) \
    if (watch_call_stack(s)) { \
        target_ulong ret_off; \
        if (is_ret_address(env, s->pc)) { \
            set_on_ret(s->pc); \
        } \
        if (is_thumb_bl_or_blx(insn, s->pc, &ret_off)) { \
            set_on_call(s->pc, s->pc + ret_off); \
            if (!s->search_pc) { \
                register_ret_address(env, s->pc + ret_off); \
            } \
        } \
    }

#  define ANDROID_DISAS_CONTEXT_FIELDS \
    int search_pc;

#  define ANDROID_START_CODEGEN(search_pc) \
    dc->search_pc = search_pc

        /* When memchecker is enabled, we need to keep a match between
         * translated PC and guest PCs, so memchecker can quickly covert
         * one to another. Note that we do that only for user mode. */
#  define ANDROID_CHECK_CODEGEN_PC(search_pc) \
        ((search_pc) || (memcheck_enabled && dc->user))

#  define ANDROID_END_CODEGEN() \
    do { \
        if (memcheck_enabled && dc->user) { \
            j = gen_opc_ptr - gen_opc_buf; \
            lj++; \
            while (lj <= j) \
                gen_opc_instr_start[lj++] = 0; \
        } \
    } while (0)

#else /* !CONFIG_MEMCHECK */

#  define ANDROID_WATCH_CALLSTACK_ARM     ((void)0)
#  define ANDROID_WATCH_CALLSTACK_THUMB   ((void)0)
#  define ANDROID_DISAS_CONTEXT_FIELDS     /* nothing */
#  define ANDROID_START_CODEGEN(s)         ((void)(s))
#  define ANDROID_CHECK_CODEGEN_PC(s)      (s)
#  define ANDROID_END_CODEGEN()            ((void)0)

#endif  /* !CONFIG_MEMCHECK */


/*****
 *****
 *****
 *****  C O N F I G _ T R A C E
 *****
 *****
 *****/

#ifdef CONFIG_TRACE

#include "android-trace.h"
#define  gen_traceInsn()   gen_helper_traceInsn()

static void
gen_traceTicks( int  count )
{
    TCGv  tmp = tcg_temp_new_i32();
    tcg_gen_movi_i32(tmp, count);
    gen_helper_traceTicks(tmp);
    tcg_temp_free_i32(tmp);
}

static void
gen_traceBB( uint64_t  bbNum, void* tb )
{
#if HOST_LONG_BITS == 32
    TCGv_i64  tmpNum = tcg_temp_new_i64();
    TCGv_i32  tmpTb  = tcg_temp_new_i32();

    tcg_gen_movi_i64(tmpNum, (int64_t)bbNum);
    tcg_gen_movi_i32(tmpTb,  (int32_t)tb);
    gen_helper_traceBB32(tmpNum, tmpTb);
    tcg_temp_free_i32(tmpTb);
    tcg_temp_free_i64(tmpNum);
#elif HOST_LONG_BITS == 64
    TCGv_i64  tmpNum = tcg_temp_new_i64();
    TCGv_i64  tmpTb  = tcg_temp_new_i64();

    tcg_gen_movi_i64(tmpNum, (int64_t)bbNum);
    tcg_gen_movi_i64(tmpTb,  (int64_t)tb);
    gen_helper_traceBB64(tmpNum, tmpTb);
    tcg_temp_free_i64(tmpTb);
    tcg_temp_free_i64(tmpNum);
#endif
}

#  define ANDROID_TRACE_DECLS   int ticks = 0;

#  define ANDROID_TRACE_START_ARM() \
    do { \
        if (tracing) { \
            trace_add_insn(insn, 0); \
            ticks = get_insn_ticks_arm(insn); \
            gen_traceInsn(); \
        } \
    } while (0)

#  define ANDROID_TRACE_START_THUMB() \
    do { \
        if (tracing) { \
            int  ticks = get_insn_ticks_thumb(insn); \
            trace_add_insn( insn_wrap_thumb(insn), 1 ); \
            gen_traceInsn(); \
            gen_traceTicks(ticks); \
        } \
    } while (0)

#  define ANDROID_TRACE_GEN_TICKS() \
    do { \
        if (tracing) { \
        } \
    } while (0)

#  define ANDROID_TRACE_GEN_SINGLE_TICK() \
    do { \
        if (tracing) { \
            gen_traceTicks(1); \
            ticks -= 1; \
        } \
    } while (0)

# define ANDROID_TRACE_GEN_OTHER_TICKS() \
    do { \
        if (tracing && ticks > 0) { \
            gen_traceTicks(ticks); \
        } \
    } while (0)

#  define ANDROID_TRACE_START_BB() \
    do { \
        if (tracing) { \
            gen_traceBB(trace_static_bb_num(), tb); \
            trace_bb_start(dc->pc); \
        } \
    } while (0)

#  define ANDROID_TRACE_END_BB() \
    do { \
        if (tracing) { \
            trace_bb_end(); \
        } \
    } while (0)

#else /* !CONFIG_TRACE */

#  define ANDROID_TRACE_DECLS         /* nothing */
#  define ANDROID_TRACE_START_ARM()   ((void)0)
#  define ANDROID_TRACE_START_THUMB() ((void)0)

#  define ANDROID_TRACE_GEN_TICKS()        ((void)0)
#  define ANDROID_TRACE_GEN_SINGLE_TICK()  ((void)0)
#  define ANDROID_TRACE_GEN_OTHER_TICKS()  ((void)0)

#  define ANDROID_TRACE_START_BB()         ((void)0)
#  define ANDROID_TRACE_END_BB()           ((void)0)

#endif /* !CONFIG_TRACE */

