#ifndef __DECAF_REGISTER_MAP_H__
#define __DECAF_REGISTER_MAP_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#if defined(TARGET_ARM)
typedef enum {
  ARM_REG_INVALID,
  ARM_REG_R0,
  ARM_REG_R1,
  ARM_REG_R2,
  ARM_REG_R3,
  ARM_REG_R4,
  ARM_REG_R5,
  ARM_REG_R6,
  ARM_REG_R7,
  ARM_REG_R8,
  ARM_REG_R9,
  ARM_REG_R10,
  ARM_REG_R11,
  ARM_REG_R12,
  ARM_REG_R13,
  ARM_REG_R14,
  ARM_REG_PC,
  ARM_REG_LAST
} reg_enum_t;
#else
#include "xed2/xed2-ia32/include/xed-reg-enum.h"
typedef xed_reg_enum_t reg_enum_t;
#endif /* defined TARGET check */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __DECAF_REGISTER_MAP_H__ */

