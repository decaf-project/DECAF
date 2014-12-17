#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "config.h"
#include "shared/tainting/analysis_log.h"
#include "tcg.h"
#include "helper.h" // AWH

/* The buffered internal analysis log */
#define MAX_LOG_ENTRIES 512
static uint8_t temp_liveness[TCG_MAX_TEMPS];
static analysis_log_entry_t analysisLog[MAX_LOG_ENTRIES];
static uint32_t logSize = 0;

/* These are our concrete global registers */
#if defined(TARGET_X86_64)
static uint64_t cpu_regs[16];
#elif defined(TARGET_I386)
static uint32_t cpu_regs[8]; 
#elif defined(TARGET_ARM)
static uint32_t cpu_regs[16];
#elif defined(TARGET_MIPS)
static uint32_t cpu_regs[32];
#else
#error Unknown target
#endif

void helper_taint_event1_32(uint32_t op, uint32_t arg0) {}
void helper_taint_event1_64(uint32_t op, uint64_t arg0) {}

void helper_taint_event2_32(uint32_t op, uint32_t arg0, uint32_t arg1) 
{
  switch (op) {
    case INDEX_op_movi_i32:
      fprintf(stderr, "INDEX_op_movi_i32: %u (%u)\n", arg0, arg1); break;
    default:
      fprintf(stderr, "Bad op: %d\n", op);
      tcg_abort();
  } 
}

void helper_taint_event2_64(uint32_t op, uint64_t arg0, uint64_t arg1) {}

void helper_taint_event3_32(uint32_t op, uint32_t arg0, uint32_t arg1, uint32_t arg2) {}
void helper_taint_event3_64(uint32_t op, uint64_t arg0, uint64_t arg1, uint64_t arg2) {}

void helper_taint_event4_32(uint32_t op, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3) 
{
  switch(op) {
    case INDEX_op_mov_i32:
      break; //fprintf(stderr, "INDEX_op_mov_i32: %u, %u (%u, %u)\n", arg0, arg1, arg2, arg3); break;
    default:
      tcg_abort();
  }
}

void helper_taint_event4_64(uint32_t op, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3) {}

void helper_taint_event5_32(uint32_t op, uint32_t arg0, uint32_t arg1,
  uint32_t arg2, uint32_t arg3, uint32_t arg4) {
  assert(logSize < MAX_LOG_ENTRIES);
  analysisLog[logSize].op = op;
  analysisLog[logSize].arg[0] = arg0;
  analysisLog[logSize].arg[1] = arg1;
  analysisLog[logSize].arg[2] = arg2;
  analysisLog[logSize].arg[3] = arg3;
  analysisLog[logSize].arg[4] = arg4;
}

void helper_taint_event5_64(uint32_t op, uint64_t arg0, uint64_t arg1,
  uint64_t arg2, uint64_t arg3, uint64_t arg4) {
  assert(logSize < MAX_LOG_ENTRIES);
  analysisLog[logSize].op = op;
  analysisLog[logSize].arg[0] = arg0;
  analysisLog[logSize].arg[1] = arg1;
  analysisLog[logSize].arg[2] = arg2;
  analysisLog[logSize].arg[3] = arg3;
  analysisLog[logSize].arg[4] = arg4;
}

void DECAF_block_begin_for_analysis(void) {
  int32_t currentIndex;
  if (logSize == 0) return;
  /* Each temp is alive or dead.  Alive temps are a concrete
    register, a temp that contributes to a value in a concrete
    register, or a temp that contributes to a value in a memory op. */
  memset(temp_liveness, 0, TCG_MAX_TEMPS);

  currentIndex = logSize - 1;
  while (currentIndex >= 0) {
    switch(analysisLog[currentIndex].op) {
      
    }
    currentIndex--;
  }
}
void helper_taint_log_pointer(uint32_t pointer,uint32_t taint_value)
{
	if(taint_value!=0)
		fprintf(stderr,"0x%08x   0x%08x \n",pointer,taint_value);
}

void helper_taint_log_deposit_i32(uint32_t reg1, uint32_t reg2, uint32_t reg3, uint32_t reg4, uint32_t reg5, uint32_t taint1, uint32_t taint2, uint32_t taint3) {
 fprintf(stderr, "log: deposit_i32()");
}

void helper_taint_log_setcond2_i32(uint32_t reg1, uint32_t reg2, uint32_t reg3, uint32_t reg4, uint32_t reg5, uint32_t reg6, uint32_t taint1, uint32_t taint2, uint32_t taint3, uint32_t taint4, uint32_t taint5) {
 fprintf(stderr, "log: setcond2_i32()");
}

void helper_taint_log_movi_i32(uint32_t reg1, uint32_t taint1) {
 fprintf(stderr, "log: movi_i32(%d:%d, 0)\n", reg1, taint1);
}

void helper_taint_log_mov_i32(uint32_t reg1, uint32_t reg2, uint32_t taint1, uint32_t taint2) {
  fprintf(stderr, "log: mov_i32(%d:%d, %d:%d)\n", reg1, taint1, reg2, taint2);
}

void helper_taint_log_qemu_ld(uint32_t reg1, uint32_t reg2, uint32_t reg3, uint32_t taint1, uint32_t taint2, uint32_t id) {
 fprintf(stderr, "log: qemu_ld()");
}

void helper_taint_log_qemu_st(uint32_t reg1, uint32_t reg2, uint32_t reg3, uint32_t taint1, uint32_t taint2, uint32_t id) {
 fprintf(stderr, "log: qemu_st()");
}

void helper_taint_log_setcond_i32(uint32_t reg1, uint32_t reg2, uint32_t reg3, uint32_t reg4, uint32_t taint1, uint32_t taint2, uint32_t taint3) {
 fprintf(stderr, "log: setcond_i32()");
}

void helper_taint_log_shl_i32(uint32_t reg1, uint32_t reg2, uint32_t reg3, uint32_t taint1, uint32_t taint2, uint32_t taint3) {
 fprintf(stderr, "log: shl_i32()");
}

void helper_taint_log_shr_i32(uint32_t reg1, uint32_t reg2, uint32_t reg3, uint32_t taint1, uint32_t taint2, uint32_t taint3) {
 fprintf(stderr, "log: shr_i32()");
}

void helper_taint_log_sar_i32(uint32_t reg1, uint32_t reg2, uint32_t reg3, uint32_t taint1, uint32_t taint2, uint32_t taint3) {
 fprintf(stderr, "log: sar_i32()");
}

#if TCG_TARGET_HAS_rot_i32
void helper_taint_log_rotl_i32(uint32_t reg1, uint32_t reg2, uint32_t reg3, uint32_t taint1, uint32_t taint2, uint32_t taint3) {
 fprintf(stderr, "log: rotl_i32()");
}

void helper_taint_log_rotr_i32(uint32_t reg1, uint32_t reg2, uint32_t reg3, uint32_t taint1, uint32_t taint2, uint32_t taint3) {
 fprintf(stderr, "log: rotr_i32()");
}
#endif /* TCG_TARGET_HAS_rot_i32 */

void helper_taint_log_add_i32(uint32_t reg1, uint32_t reg2, uint32_t reg3, uint32_t taint1, uint32_t taint2, uint32_t taint3) {
 fprintf(stderr, "log: add_i32()");
}

void helper_taint_log_sub_i32(uint32_t reg1, uint32_t reg2, uint32_t reg3, uint32_t taint1, uint32_t taint2, uint32_t taint3) {
 fprintf(stderr, "log: sub_i32()");
}

void helper_taint_log_mul_i32(uint32_t reg1, uint32_t reg2, uint32_t reg3, uint32_t taint1, uint32_t taint2, uint32_t taint3) {
 fprintf(stderr, "log: mul_i32()");
}

void helper_taint_log_and_i32(uint32_t reg1, uint32_t reg2, uint32_t reg3, uint32_t taint1, uint32_t taint2, uint32_t taint3) {
 fprintf(stderr, "log: and_i32()");
}

void helper_taint_log_or_i32(uint32_t reg1, uint32_t reg2, uint32_t reg3, uint32_t taint1, uint32_t taint2, uint32_t taint3) {
 fprintf(stderr, "log: or_i32()");
}

void helper_taint_log_xor_i32(uint32_t reg1, uint32_t reg2, uint32_t reg3, uint32_t taint1, uint32_t taint2, uint32_t taint3) {
 fprintf(stderr, "log: xor_i32()");
}

#if TCG_TARGET_HAS_div_i32
void helper_taint_log_div_i32(uint32_t reg1, uint32_t reg2, uint32_t reg3, uint32_t taint1, uint32_t taint2, uint32_t taint3) {
 fprintf(stderr, "log: div_i32()");
}

void helper_taint_log_divu_i32(uint32_t reg1, uint32_t reg2, uint32_t reg3, uint32_t taint1, uint32_t taint2, uint32_t taint3) {
 fprintf(stderr, "log: divu_i32()");
}

void helper_taint_log_rem_i32(uint32_t reg1, uint32_t reg2, uint32_t reg3, uint32_t taint1, uint32_t taint2, uint32_t taint3) {
 fprintf(stderr, "log: rem_i32()");
}

void helper_taint_log_remu_i32(uint32_t reg1, uint32_t reg2, uint32_t reg3, uint32_t taint1, uint32_t taint2, uint32_t taint3) {
 fprintf(stderr, "log: remu_i32()");
}
#elif TCG_TARGET_HAS_div2_i32
void helper_taint_log_div2_i32(uint32_t reg1, uint32_t reg2, uint32_t reg3, uint32_t reg4, uint32_t reg5, uint32_t taint1, uint32_t taint2, uint32_t taint3, uint32_t taint4, uint32_t taint5) {
 fprintf(stderr, "log: div2_i32()");
}

void helper_taint_log_divu2_i32(uint32_t reg1, uint32_t reg2, uint32_t reg3, uint32_t reg4, uint32_t reg5, uint32_t taint1, uint32_t taint2, uint32_t taint3, uint32_t taint4, uint32_t taint5) {
 fprintf(stderr, "log: divu2_i32()");
}
#endif /* TCG_TARGET_HAS_div2_i32 */

void helper_taint_log_mulu2_i32(uint32_t reg1, uint32_t reg2, uint32_t reg3, uint32_t reg4, uint32_t taint1, uint32_t taint2, uint32_t taint3, uint32_t taint4) {
 fprintf(stderr, "log: mulu2_i32()");
}

void helper_taint_log_add2_i32(uint32_t reg1, uint32_t reg2, uint32_t reg3, uint32_t reg4, uint32_t reg5, uint32_t reg6, uint32_t taint1, uint32_t taint2, uint32_t taint3, uint32_t taint4, uint32_t taint5, uint32_t taint6) {
 fprintf(stderr, "log: add2_i32()");
}

void helper_taint_log_sub2_i32(uint32_t reg1, uint32_t reg2, uint32_t reg3, uint32_t reg4, uint32_t reg5, uint32_t reg6, uint32_t taint1, uint32_t taint2, uint32_t taint3, uint32_t taint4, uint32_t taint5, uint32_t taint6) {
 fprintf(stderr, "log: sub2_i32()");
}

#if TCG_TARGET_HAS_ext8s_i32
void helper_taint_log_ext8s_i32(uint32_t reg1, uint32_t reg2, uint32_t taint1, uint32_t taint2) {
 fprintf(stderr, "log: ext8s_i32()");
}
#endif /* TCG_TARGET_HAS_ext8s_i32 */
#if TCG_TARGET_HAS_ext16s_i32
void helper_taint_log_ext16s_i32(uint32_t reg1, uint32_t reg2, uint32_t taint1, uint32_t taint2) {
 fprintf(stderr, "log: ext16s_i32()");
}
#endif /* TCG_TARGET_HAS_ext16s_i32 */
#if TCG_TARGET_HAS_ext8u_i32
void helper_taint_log_ext8u_i32(uint32_t reg1, uint32_t reg2, uint32_t taint1, uint32_t taint2) {
 fprintf(stderr, "log: ext8u_i32()");
}
#endif /* TCG_TARGET_HAS_ext8u_i32 */
#if TCG_TARGET_HAS_ext16u_i32
void helper_taint_log_ext16u_i32(uint32_t reg1, uint32_t reg2, uint32_t taint1, uint32_t taint2) {
 fprintf(stderr, "log: ext16u_i32()");
}
#endif /* TCG_TARGET_HAS_ext16u_i32 */
#if TCG_TARGET_HAS_bswap16_i32
void helper_taint_log_bswap16_i32(uint32_t reg1, uint32_t reg2, uint32_t taint1, uint32_t taint2) {
 fprintf(stderr, "log: bswap16_i32()");
}
#endif /* TCG_TARGET_HAS_bswap16_i32 */
#if TCG_TARGET_HAS_bswap32_i32
void helper_taint_log_bswap32_i32(uint32_t reg1, uint32_t reg2, uint32_t taint1, uint32_t taint2) {
 fprintf(stderr, "log: bswap32_i32()");
}
#endif /* TCG_TARGET_HAS_bswap32_i32 */
#if TCG_TARGET_HAS_not_i32
void helper_taint_log_not_i32(uint32_t reg1, uint32_t reg2, uint32_t taint1, uint32_t taint2) {
 fprintf(stderr, "log: not_i32()");
}
#endif /* TCG_TARGET_HAS_not_i32 */
#if TCG_TARGET_HAS_neg_i32
void helper_taint_log_neg_i32(uint32_t reg1, uint32_t reg2, uint32_t taint1, uint32_t taint2) {
 fprintf(stderr, "log: neg_i32()");
}
#endif /* TCG_TARGET_HAS_neg_i32 */

