/*
 * args.h - Instruction argument header
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

#ifndef _LIBDISARM_ARGS_H
#define _LIBDISARM_ARGS_H

#include "libdisarm_macros.h"
#include "libdisarm_types.h"

#define DA_ARG(instr,shift,mask)  (((instr)->data >> (shift)) & (mask))
#define DA_ARG_BOOL(instr,shift)  DA_ARG(instr,shift,1)
#define DA_ARG_COND(instr,shift)  DA_ARG(instr,shift,DA_COND_MASK)
#define DA_ARG_SHIFT(instr,shift)  DA_ARG(instr,shift,DA_SHIFT_MASK)
#define DA_ARG_DATA_OP(instr,shift)  DA_ARG(instr,shift,DA_DATA_OP_MASK)
#define DA_ARG_REG(instr,shift)  DA_ARG(instr,shift,DA_REG_MASK)
#define DA_ARG_CPREG(instr,shift)  DA_ARG(instr,shift,DA_CPREG_MASK)

DA_BEGIN_DECLS

typedef struct {
	da_cond_t cond;
	da_uint_t imm;
} da_args_bkpt_t;

typedef struct {
	da_cond_t cond;
	da_uint_t link;
	da_uint_t off;
} da_args_bl_t;

typedef struct {
	da_cond_t cond;
	da_uint_t link;
	da_reg_t rm;
} da_args_blx_reg_t;

typedef struct {
	da_uint_t h;
	da_uint_t off;
} da_args_blx_imm_t;

typedef struct {
	da_cond_t cond;
	da_reg_t rd;
	da_reg_t rm;
} da_args_clz_t;

typedef struct {
	da_cond_t cond;
	da_uint_t op_1;
	da_cpreg_t crn;
	da_cpreg_t crd;
	da_uint_t cp_num;
	da_uint_t op_2;
	da_cpreg_t crm;
} da_args_cp_data_t;

typedef struct {
	da_cond_t cond;
	da_uint_t p;
	da_uint_t sign;
	da_uint_t n;
	da_uint_t write;
	da_uint_t load;
	da_reg_t rn;
	da_cpreg_t crd;
	da_uint_t cp_num;
	da_uint_t imm;
} da_args_cp_ls_t;

typedef struct {
	da_cond_t cond;
	da_uint_t op_1;
	da_uint_t load;
	da_cpreg_t crn;
	da_reg_t rd;
	da_uint_t cp_num;
	da_uint_t op_2;
	da_cpreg_t crm;
} da_args_cp_reg_t;

typedef struct {
	da_cond_t cond;
	da_data_op_t op;
	da_uint_t flags;
	da_reg_t rn;
	da_reg_t rd;
	da_uint_t imm;
} da_args_data_imm_t;

typedef struct {
	da_cond_t cond;
	da_data_op_t op;
	da_uint_t flags;
	da_reg_t rn;
	da_reg_t rd;
	da_uint_t sha;
	da_shift_t sh;
	da_reg_t rm;
} da_args_data_imm_sh_t;

typedef struct {
	da_cond_t cond;
	da_data_op_t op;
	da_uint_t flags;
	da_reg_t rn;
	da_reg_t rd;
	da_reg_t rs;
	da_shift_t sh;
	da_reg_t rm;
} da_args_data_reg_sh_t;

typedef struct {
	da_cond_t cond;
	da_uint_t op;
	da_reg_t rn;
	da_reg_t rd;
	da_reg_t rm;
} da_args_dsp_add_sub_t;

typedef struct {
	da_cond_t cond;
	da_uint_t op;
	da_reg_t rd;
	da_reg_t rn;
	da_reg_t rs;
	da_uint_t y;
	da_uint_t x;
	da_reg_t rm;
} da_args_dsp_mul_t;

typedef struct {
	da_cond_t cond;
	da_uint_t p;
	da_uint_t write;
	da_reg_t rn;
	da_reg_t rd;
	da_uint_t hword;
	int off;
} da_args_l_sign_imm_t;

typedef struct {
	da_cond_t cond;
	da_uint_t p;
	da_uint_t sign;
	da_uint_t write;
	da_reg_t rn;
	da_reg_t rd;
	da_uint_t hword;
	da_reg_t rm;
} da_args_l_sign_reg_t;

typedef struct {
	da_cond_t cond;
	da_uint_t p;
	da_uint_t write;
	da_uint_t load;
	da_reg_t rn;
	da_reg_t rd;
	int off;
} da_args_ls_hw_imm_t;

typedef struct {
	da_cond_t cond;
	da_uint_t p;
	da_uint_t sign;
	da_uint_t write;
	da_uint_t load;
	da_reg_t rn;
	da_reg_t rd;
	da_reg_t rm;
} da_args_ls_hw_reg_t;

typedef struct {
	da_cond_t cond;
	da_uint_t p;
	da_uint_t byte;
	da_uint_t w;
	da_uint_t load;
	da_reg_t rn;
	da_reg_t rd;
	int off;
} da_args_ls_imm_t;

typedef struct {
	da_cond_t cond;
	da_uint_t p;
	da_uint_t u;
	da_uint_t s;
	da_uint_t write;
	da_uint_t load;
	da_reg_t rn;
	da_uint_t reglist;
} da_args_ls_multi_t;

typedef struct {
	da_cond_t cond;
	da_uint_t p;
	da_uint_t sign;
	da_uint_t byte;
	da_uint_t write;
	da_uint_t load;
	da_reg_t rn;
	da_reg_t rd;
	da_uint_t sha;
	da_shift_t sh;
	da_reg_t rm;
} da_args_ls_reg_t;

typedef struct {
	da_cond_t cond;
	da_uint_t p;
	da_uint_t write;
	da_reg_t rn;
	da_reg_t rd;
	da_uint_t store;
	int off;
} da_args_ls_two_imm_t;

typedef struct {
	da_cond_t cond;
	da_uint_t p;
	da_uint_t sign;
	da_uint_t write;
	da_reg_t rn;
	da_reg_t rd;
	da_uint_t store;
	da_reg_t rm;
} da_args_ls_two_reg_t;

typedef struct {
	da_cond_t cond;
	da_uint_t r;
	da_reg_t rd;
} da_args_mrs_t;

typedef struct {
	da_cond_t cond;
	da_uint_t r;
	da_uint_t mask;
	da_reg_t rm;
} da_args_msr_t;

typedef struct {
	da_cond_t cond;
	da_uint_t r;
	da_uint_t mask;
	da_uint_t imm;
} da_args_msr_imm_t;

typedef struct {
	da_cond_t cond;
	da_uint_t acc;
	da_uint_t flags;
	da_reg_t rd;
	da_reg_t rn;
	da_reg_t rs;
	da_reg_t rm;
} da_args_mul_t;

typedef struct {
	da_cond_t cond;
	da_uint_t sign;
	da_uint_t acc;
	da_uint_t flags;
	da_reg_t rd_hi;
	da_reg_t rd_lo;
	da_reg_t rs;
	da_reg_t rm;
} da_args_mull_t;

typedef struct {
	da_cond_t cond;
	da_uint_t imm;
} da_args_swi_t;

typedef struct {
	da_cond_t cond;
	da_uint_t byte;
	da_reg_t rn;
	da_reg_t rd;
	da_reg_t rm;
} da_args_swp_t;


typedef struct {
	union {
		da_args_bkpt_t bkpt;
		da_args_bl_t bl;
		da_args_blx_imm_t blx_imm;
		da_args_blx_reg_t blx_reg;
		da_args_clz_t clz;
		da_args_cp_data_t cp_data;
		da_args_cp_ls_t cp_ls;
		da_args_cp_reg_t cp_reg;
		da_args_data_imm_t data_imm;
		da_args_data_imm_sh_t data_imm_sh;
		da_args_data_reg_sh_t data_reg_sh;
		da_args_dsp_add_sub_t dsp_add_sub;
		da_args_dsp_mul_t dsp_mul;
		da_args_l_sign_imm_t l_sign_imm;
		da_args_l_sign_reg_t l_sign_reg;
		da_args_ls_hw_imm_t ls_hw_imm;
		da_args_ls_hw_reg_t ls_hw_reg;
		da_args_ls_imm_t ls_imm;
		da_args_ls_multi_t ls_multi;
		da_args_ls_reg_t ls_reg;
		da_args_ls_two_imm_t ls_two_imm;
		da_args_ls_two_reg_t ls_two_reg;
		da_args_mrs_t mrs;
		da_args_msr_t msr;
		da_args_msr_imm_t msr_imm;
		da_args_mul_t mul;
		da_args_mull_t mull;
		da_args_swi_t swi;
		da_args_swp_t swp;
	};
} da_instr_args_t;


da_cond_t da_instr_get_cond(const da_instr_t *instr);
da_addr_t da_instr_branch_target(da_uint_t off, da_addr_t addr);

void da_instr_parse_args(da_instr_args_t *args, const da_instr_t *instr);

DA_END_DECLS

#endif /* ! _LIBDISARM_ARGS_H */
