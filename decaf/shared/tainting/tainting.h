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
 * tainting.h
 *
 *  Created on: Oct 17, 2012
 *      Author: lok
 */

//This file needs to be removed --Heng
//uncomment this
#ifndef TAINTING_H_
#define TAINTING_H_

#include "shared/DECAF_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

  //LOK: This used to be in taintcheck.h - moved it here now for consolidation
// AWH - This used to be in taintcheck_types.h
/*! \typedef taint_operand_t
 This structure contains taint information for an operand (memory or register).
 @ingroup taintcheck
*/
typedef struct {
#define OPERAND_REG 0
#define OPERAND_MEM 1
#define OPERAND_NIC 2
#define OPERAND_DISK 3
  char type;
  char size;
  uint8_t taint;
  uint8_t offset;
  uint32_t addr;
  uint8_t *records;
} taint_operand_t;

typedef struct _tainting_struct_t
{
  /// \brief size of taint record for each tainted byte.
  /// TEMU sees taint record as untyped buffer, so it only cares about the
  /// size of taint record
  int taint_record_size;

#define PROP_MODE_MOVE  0
#define PROP_MODE_XFORM 1

  /// \brief This callback customizes its own policy for taint propagation
  ///
  /// TEMU asks plugin how to propagate tainted data. If the plugin does not
  /// want to customize the propagation policy, it can simply specify
  /// default_taint_propagate().
  ///
  /// @param nr_src number of source operands
  /// @param src_oprnds array of source operands
  /// @param dst_oprnd destination operand
  /// @param mode mode of propagation (either direct move or transformation)
  void (*taint_propagate) (int nr_src, taint_operand_t *src_oprnds,
                taint_operand_t *dst_oprnd, int mode);
  void (*taint_disk) (uint64_t addr, uint8_t * record, void *opaque);
  void (*read_disk_taint)(uint64_t addr, uint8_t * record, void *opaque);
  void (*eip_tainted) (uint8_t * record);
}tainting_struct_t;

extern tainting_struct_t* taint_config;

void tainting_init(void);
void tainting_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* TAINTING_H_ */
