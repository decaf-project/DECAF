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
/*
 * DECAF_callback.h
 *
 *  Created on: Apr 10, 2012
 *      Author: heyin@syr.edu
 */

#ifndef DECAF_CALLBACK_H_
#define DECAF_CALLBACK_H_

//LOK: for CPUState
// #include "cpu.h" //Not needed - included in DECAF_callback_common.h
// #include "shared/DECAF_types.h" // not needed either
#include "shared/DECAF_callback_common.h"

#ifdef __cplusplus
extern "C"
{
#endif

/// \brief Register a callback function
///
/// @param cb_type the event type
/// @param cb_func the callback function
/// @param cb_cond the boolean condition provided by the caller. Only
/// if this condition is true, the callback can be activated. This condition
/// can be NULL, so that callback is always activated.
/// @return handle, which is needed to unregister this callback later.
extern DECAF_Handle DECAF_register_callback(
		DECAF_callback_type_t cb_type,
		DECAF_callback_func_t cb_func,
		int *cb_cond
                );

extern int DECAF_unregister_callback(DECAF_callback_type_t cb_type, DECAF_Handle handle);

extern DECAF_Handle DECAF_registerOpcodeRangeCallbacks (
		DECAF_callback_func_t handler,
		OpcodeRangeCallbackConditions *condition,
		uint16_t start_opcode,
		uint16_t end_opcode);

extern DECAF_Handle DECAF_registerOptimizedBlockBeginCallback(
    DECAF_callback_func_t cb_func,
    int *cb_cond,
    gva_t addr,
    OCB_t type);

extern DECAF_Handle DECAF_registerOptimizedBlockEndCallback(
    DECAF_callback_func_t cb_func,
    int *cb_cond,
    gva_t from,
    gva_t to);

extern int DECAF_unregisterOptimizedBlockBeginCallback(DECAF_Handle handle);

extern int DECAF_unregisterOptimizedBlockEndCallback(DECAF_Handle handle);

extern DECAF_errno_t DECAF_unregisterOpcodeRangeCallbacks(DECAF_Handle handle);
extern void helper_DECAF_invoke_block_begin_callback(CPUState* env, TranslationBlock* tb);
extern void helper_DECAF_invoke_block_end_callback(CPUState* env, TranslationBlock* tb, gva_t from);
extern void helper_DECAF_invoke_insn_begin_callback(CPUState* env);
extern void helper_DECAF_invoke_insn_end_callback(CPUState* env);
extern void helper_DECAF_invoke_eip_check_callback(gva_t source_eip, gva_t target_eip, gva_t target_eip_taint);
extern void helper_DECAF_invoke_opcode_range_callback(
  CPUState *env,
  target_ulong eip,
  target_ulong next_eip,
  uint32_t op);
extern void DECAF_callback_init(void);

#ifdef __cplusplus
}
#endif // __cplusplus


#endif /* DECAF_CALLBACK_H_ */
