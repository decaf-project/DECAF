/* AWH - This is included by the target directory's helper.h.  
   Don't include this in your code */

#ifdef CONFIG_TCG_TAINT
/* void taint_eventX_32/64(opcode, args0-4) */
DEF_HELPER_2(taint_event1_32, void, i32, i32)
DEF_HELPER_2(taint_event1_64, void, i32, i64)
DEF_HELPER_3(taint_event2_32, void, i32, i32, i32)
DEF_HELPER_3(taint_event2_64, void, i32, i64, i64)
DEF_HELPER_4(taint_event3_32, void, i32, i32, i32, i32)
DEF_HELPER_4(taint_event3_64, void, i32, i64, i64, i64)
DEF_HELPER_5(taint_event4_32, void, i32, i32, i32, i32, i32)
DEF_HELPER_5(taint_event4_64, void, i32, i64, i64, i64, i64)
DEF_HELPER_6(taint_event5_32, void, i32, i32, i32, i32, i32, i32)
DEF_HELPER_6(taint_event5_64, void, i32, i64, i64, i64, i64, i64)

/* AWH - Each helper function passes either a Value (V) or a 
  Concrete (C) for each opcode argument that the helper logs.  Each 
  value is either a register number or a constant.  Each concrete 
  is the taint associated with a register.  For some TCG opcodes, 
  we'll need both a value and concrete (VC) for an argument.  
  Other times, we'll need just the value (V). For each helper 
  function, the values (V) are the first arguments to the helper 
  and the concretes (C) are the last arguments to the helper.  

  As an example, take mov_i32.  This opcode takes two arguments: a 
  source register and a destination register.  In order to capture 
  the taint-changing properties of this opcode, we need to capture 
  both the values of each opcode argument (i.e. the numbers of the 
  registers used as the src and dest) and the concrete taint for 
  each of the registers (i.e. the values held in each of the 
  registers that shadow the src and dest registers).  We refer to
  this as (VC, VC), where the V and C of both parms are needed.
  As another quick example, the deposit_i32 opcode has a form of
  (VC, VC, VC, V, V).  In this case, the value and concrete are 
  needed for the first three parms, but only the value is needed for
  the last two parms. 

  For mov_i32, we'll need to define a helper that takes 4 arguments 
  (DEF_HELPER_4()).  This helper will return "void" (its result is
  not used by the TCG opcodes following the helper function call).
  Since values are the first arguments, the (VC, VC) form of the 
  opcode results in a helper function call of (V, V, C, C). 
*/
#if 0 // AWH - Duplication of DECAF_callback_to_QEMU.h
DEF_HELPER_2(DECAF_invoke_log_pointer_write, void, i32, i32)
DEF_HELPER_2(DECAF_invoke_log_pointer_read, void, i32, i32)
#endif // AWH
DEF_HELPER_2(taint_log_pointer, void, i32, i32)
/* 5 parms: (VC, VC, VC, V, V) */
DEF_HELPER_8(taint_log_deposit_i32, void, i32, i32, i32, i32, i32, i32, i32, i32)
/* 6 parms: (VC, VC, VC, VC, VC, V) */
DEF_HELPER_11(taint_log_setcond2_i32, void, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32)
/* 1 parm: (VC) */
DEF_HELPER_2(taint_log_movi_i32, void, i32, i32)
/* 2 parms: (VC, VC) */
DEF_HELPER_4(taint_log_mov_i32, void, i32, i32, i32, i32)
/* 4 parms: (VC, VC, V, ID) - Sixth parm is the opcode ID (follows concrete) */
DEF_HELPER_6(taint_log_qemu_ld, void, i32, i32, i32, i32, i32, i32)
DEF_HELPER_6(taint_log_qemu_st, void, i32, i32, i32, i32, i32, i32)
#if 0 // AWH
DEF_HELPER_?(taint_log_qemu_ld64, void, ?)
DEF_HELPER_?(taint_log_qemu_st64, void, ?)
#endif // AWH
/* 4 parms: (VC, VC, VC, V) */
DEF_HELPER_7(taint_log_setcond_i32, void, i32, i32, i32, i32, i32, i32, i32)
/* 3 parms: (VC, VC, VC) */
DEF_HELPER_6(taint_log_shl_i32, void, i32, i32, i32, i32, i32, i32)
DEF_HELPER_6(taint_log_shr_i32, void, i32, i32, i32, i32, i32, i32)
DEF_HELPER_6(taint_log_sar_i32, void, i32, i32, i32, i32, i32, i32)
#if TCG_TARGET_HAS_rot_i32
DEF_HELPER_6(taint_log_rotl_i32, void, i32, i32, i32, i32, i32, i32)
DEF_HELPER_6(taint_log_rotr_i32, void, i32, i32, i32, i32, i32, i32)
#endif /* TCG_TARGET_HAS_rot_i32 */
DEF_HELPER_6(taint_log_add_i32, void, i32, i32, i32, i32, i32, i32)
DEF_HELPER_6(taint_log_sub_i32, void, i32, i32, i32, i32, i32, i32)
DEF_HELPER_6(taint_log_mul_i32, void, i32, i32, i32, i32, i32, i32)
DEF_HELPER_6(taint_log_and_i32, void, i32, i32, i32, i32, i32, i32)
DEF_HELPER_6(taint_log_or_i32, void, i32, i32, i32, i32, i32, i32)
DEF_HELPER_6(taint_log_xor_i32, void, i32, i32, i32, i32, i32, i32)
#if TCG_TARGET_HAS_div_i32
DEF_HELPER_6(taint_log_div_i32, void, i32, i32, i32, i32, i32, i32)
DEF_HELPER_6(taint_log_divu_i32, void, i32, i32, i32, i32, i32, i32)
DEF_HELPER_6(taint_log_rem_i32, void, i32, i32, i32, i32, i32, i32)
DEF_HELPER_6(taint_log_remu_i32, void, i32, i32, i32, i32, i32, i32)
#elif TCG_TARGET_HAS_div2_i32
/* 5 parms: (VC, VC, VC, VC, VC) */
DEF_HELPER_10(taint_log_div2_i32, void, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32)
DEF_HELPER_10(taint_log_divu2_i32, void, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32)
#endif /* TCG_TARGET_HAS_div_i32 */
/* 4 parms: (VC, VC, VC, VC) */
DEF_HELPER_8(taint_log_mulu2_i32, void, i32, i32, i32, i32, i32, i32, i32, i32)
/* 6 parms: (VC, VC, VC, VC, VC, VC) */
DEF_HELPER_12(taint_log_add2_i32, void, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32)
DEF_HELPER_12(taint_log_sub2_i32, void, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32)
/* 2 parms: (VC, VC) */
#if TCG_TARGET_HAS_ext8s_i32
DEF_HELPER_4(taint_log_ext8s_i32, void, i32, i32, i32, i32)
#endif /* TCG_TARGET_HAS_ext8s_i32 */
#if TCG_TARGET_HAS_ext16s_i32
DEF_HELPER_4(taint_log_ext16s_i32, void, i32, i32, i32, i32)
#endif /* TCG_TARGET_HAS_ext16s_i32 */
#if TCG_TARGET_HAS_ext8u_i32
DEF_HELPER_4(taint_log_ext8u_i32, void, i32, i32, i32, i32)
#endif /* TCG_TARGET_HAS_ext8u_i32 */
#if TCG_TARGET_HAS_ext16u_i32
DEF_HELPER_4(taint_log_ext16u_i32, void, i32, i32, i32, i32)
#endif /* TCG_TARGET_HAS_ext16u_i32 */
#if TCG_TARGET_HAS_bswap16_i32
DEF_HELPER_4(taint_log_bswap16_i32, void, i32, i32, i32, i32)
#endif /* TCG_TARGET_HAS_bswap16_i32 */
#if TCG_TARGET_HAS_bswap32_i32
DEF_HELPER_4(taint_log_bswap32_i32, void, i32, i32, i32, i32)
#endif /* TCG_TARGET_HAS_bswap32_i32 */
#if TCG_TARGET_HAS_not_i32
DEF_HELPER_4(taint_log_not_i32, void, i32, i32, i32, i32)
#endif /* TCG_TARGET_HAS_not_i32 */
#if TCG_TARGET_HAS_neg_i32
DEF_HELPER_4(taint_log_neg_i32, void, i32, i32, i32, i32)
#endif /* TCG_TARGET_HAS_neg_i32 */

/* TODO: All of the 64-bit opcodes */

#endif /* CONFIG_TCG_TAINT */

