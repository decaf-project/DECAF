/*
 * types.h - Types header
 *
 * Copyright (C) 2007  Jon Lund Steffensen <jonlst@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _LIBDISARM_TYPES_H
#define _LIBDISARM_TYPES_H

#include <stdint.h>


typedef uint32_t da_word_t;
typedef da_word_t da_addr_t;
typedef unsigned int da_uint_t;


typedef enum {
	/* Software breakpoint */
	DA_GROUP_BKPT = 0,

	/* Branch and branch with link */
	DA_GROUP_BL,
	/* Branch and branch with link and possible change to Thumb */
	DA_GROUP_BLX_IMM,
	/* Branch and link/exchange instruction set */
	DA_GROUP_BLX_REG,

	/* Count leading zeros */
	DA_GROUP_CLZ,

	/* Coprocessor data processing */
	DA_GROUP_CP_DATA,
	/* Coprocessor load/store and double register transfers */
	DA_GROUP_CP_LS,
	/* Coprocessor register transfers */
	DA_GROUP_CP_REG,

	/* Data processing immediate */
	DA_GROUP_DATA_IMM,
	/* Data processing immediate shift */
	DA_GROUP_DATA_IMM_SH,
	/* Data processing register shift */
	DA_GROUP_DATA_REG_SH,

	/* Enhanced DSP add/subtracts */
	DA_GROUP_DSP_ADD_SUB,
	/* Enhanced DSP multiplies */
	DA_GROUP_DSP_MUL,

	/* Load signed halfword/byte immediate offset */
	DA_GROUP_L_SIGN_IMM,
	/* Load signed halfword/byte register offset */
	DA_GROUP_L_SIGN_REG,
	/* Load/store halfword immediate offset */
	DA_GROUP_LS_HW_IMM,
	/* Load/store halfword register offset */
	DA_GROUP_LS_HW_REG,
	/* Load/store immediate offset */
	DA_GROUP_LS_IMM,
	/* Load/store multiple */
	DA_GROUP_LS_MULTI,
	/* Load/store register offset */
	DA_GROUP_LS_REG,
	/* Load/store two words immediate offset */
	DA_GROUP_LS_TWO_IMM,
	/* Load/store two words register offset */
	DA_GROUP_LS_TWO_REG,

	/* Move status register to register */
	DA_GROUP_MRS,
	/* Move register to status register */
	DA_GROUP_MSR,
	/* Move immediate to status register */
	DA_GROUP_MSR_IMM,

	/* Multiply (accumulate) */
	DA_GROUP_MUL,
	/* Multiply (accumulate) long */
	DA_GROUP_MULL,

	/* Software interrupt */
	DA_GROUP_SWI,

	/* Swap/swap byte */
	DA_GROUP_SWP,

	/* Undefined instruction group #1 */
	DA_GROUP_UNDEF_1,
	/* Undefined instruction group #2 */
	DA_GROUP_UNDEF_2,
	/* Undefined instruction group #3 */
	DA_GROUP_UNDEF_3,
	/* Undefined instruction group #4 */
	DA_GROUP_UNDEF_4,
	/* Undefined instruction group #5 */
	DA_GROUP_UNDEF_5,

	DA_GROUP_MAX
} da_group_t;


typedef enum {
	DA_COND_EQ = 0, DA_COND_NE,
	DA_COND_CS,     DA_COND_CC,
	DA_COND_MI,     DA_COND_PL,
	DA_COND_VS,     DA_COND_VC,
	DA_COND_HI,     DA_COND_LS,
	DA_COND_GE,     DA_COND_LT,
	DA_COND_GT,     DA_COND_LE,
	DA_COND_AL,     DA_COND_NV,
	DA_COND_MAX
} da_cond_t;

#define DA_COND_MASK  (DA_COND_MAX - 1)


typedef enum {
	DA_SHIFT_LSL = 0, DA_SHIFT_LSR,
	DA_SHIFT_ASR,     DA_SHIFT_ROR,
	DA_SHIFT_MAX
} da_shift_t;

#define DA_SHIFT_MASK  (DA_SHIFT_MAX - 1)

typedef enum {
	DA_DATA_OP_AND = 0, DA_DATA_OP_EOR,
	DA_DATA_OP_SUB,     DA_DATA_OP_RSB,
	DA_DATA_OP_ADD,     DA_DATA_OP_ADC,
	DA_DATA_OP_SBC,     DA_DATA_OP_RSC,
	DA_DATA_OP_TST,     DA_DATA_OP_TEQ,
	DA_DATA_OP_CMP,     DA_DATA_OP_CMN,
	DA_DATA_OP_ORR,     DA_DATA_OP_MOV,
	DA_DATA_OP_BIC,     DA_DATA_OP_MVN,
	DA_DATA_OP_MAX
} da_data_op_t;

#define DA_DATA_OP_MASK  (DA_DATA_OP_MAX - 1)


typedef enum {
	DA_REG_R0 = 0, DA_REG_R1,
	DA_REG_R2,     DA_REG_R3,
	DA_REG_R4,     DA_REG_R5,
	DA_REG_R6,     DA_REG_R7,
	DA_REG_R8,     DA_REG_R9,
	DA_REG_R10,    DA_REG_R11,
	DA_REG_R12,    DA_REG_R13,
	DA_REG_R14,    DA_REG_R15,
	DA_REG_MAX
} da_reg_t;

#define DA_REG_SP  DA_REG_13
#define DA_REG_LR  DA_REG_14
#define DA_REG_PC  DA_REG_15

#define DA_REG_MASK  (DA_REG_MAX - 1)


typedef enum {
	DA_CPREG_CR0 = 0, DA_CPREG_CR1,
	DA_CPREG_CR2,     DA_CPREG_CR3,
	DA_CPREG_CR4,     DA_CPREG_CR5,
	DA_CPREG_CR6,     DA_CPREG_CR7,
	DA_CPREG_CR8,     DA_CPREG_CR9,
	DA_CPREG_CR10,    DA_CPREG_CR11,
	DA_CPREG_CR12,    DA_CPREG_CR13,
	DA_CPREG_CR14,    DA_CPREG_CR15,
	DA_CPREG_MAX
} da_cpreg_t;

#define DA_CPREG_MASK  (DA_CPREG_MAX - 1)


typedef enum {
	DA_FLAG_N = 0, DA_FLAG_Z,
	DA_FLAG_C,     DA_FLAG_V,
	DA_FLAG_MAX
} da_flag_t;

#define DA_FLAG_MASK  (DA_FLAG_MAX - 1)


typedef struct {
	da_word_t data;
	da_group_t group;
} da_instr_t;


#endif /* ! _LIBDISARM_TYPES_H */
