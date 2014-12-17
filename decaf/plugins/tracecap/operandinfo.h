/* 
   Tracecap is owned and copyright (C) BitBlaze, 2007-2010.
   All rights reserved.
   Do not copy, disclose, or distribute without explicit written
   permission. 

   Author: Juan Caballero <jcaballero@cmu.edu>
           Zhenkai Liang <liangzk@comp.nus.edu.sg>
*/
#ifndef _OPERANDINFO_H_
#define _OPERANDINFO_H_
#include "trace.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

inline void set_operand_taint(OperandVal *op);
extern void set_operand_data(OperandVal *op, int is_begin);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // _OPERANDINFO_H_
