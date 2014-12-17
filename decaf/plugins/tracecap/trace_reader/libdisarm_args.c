/*
 * args.c - Instruction argument functions
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>

#include "libdisarm_args.h"
#include "libdisarm_macros.h"
#include "libdisarm_types.h"


/* Return true if instruction has a cond field. */
static int
da_instr_has_cond(const da_instr_t *instr)
{
	switch (instr->group) {
	case DA_GROUP_UNDEF_1:
	case DA_GROUP_UNDEF_2:
	case DA_GROUP_UNDEF_3:
	case DA_GROUP_UNDEF_4:
	case DA_GROUP_UNDEF_5:
	case DA_GROUP_BLX_IMM:
		return 0;
	default:
		return 1;
	}
}

DA_API da_cond_t
da_instr_get_cond(const da_instr_t *instr)
{
	if (da_instr_has_cond(instr)) return DA_ARG_COND(instr, 28);
	else return DA_COND_AL;
}

/* Return target address for branch instruction offset. */
DA_API da_addr_t
da_instr_branch_target(da_uint_t offset, da_addr_t addr)
{
	/* Sign-extend offset */
	struct { signed int ext : 24; } off;
	off.ext = offset;
	
	return (off.ext << 2) + addr + 8;
}

/* Parse instruction arguments. */
DA_API void
da_instr_parse_args(da_instr_args_t *args, const da_instr_t *instr)
{
	switch (instr->group) {
	case DA_GROUP_BKPT:
		args->bkpt.cond = DA_ARG_COND(instr, 28);
		args->bkpt.imm = (DA_ARG(instr, 8, 0xfff) << 4) |
			DA_ARG(instr, 0, 0xf);
		break;
	case DA_GROUP_BL:
		args->bl.cond = DA_ARG_COND(instr, 28);
		args->bl.link = DA_ARG_BOOL(instr, 24);
		args->bl.off = DA_ARG(instr, 0, 0xffffff);
		break;
	case DA_GROUP_BLX_IMM:
		args->blx_imm.h = DA_ARG_BOOL(instr, 24);
		args->blx_imm.off = DA_ARG(instr, 0, 0xffffff);
		break;
	case DA_GROUP_BLX_REG:
		args->blx_reg.cond = DA_ARG_COND(instr, 28);
		args->blx_reg.link = DA_ARG_BOOL(instr, 5);
		args->blx_reg.rm = DA_ARG_REG(instr, 0);
		break;
	case DA_GROUP_CLZ:
		args->clz.cond = DA_ARG_COND(instr, 28);
		args->clz.rd = DA_ARG_REG(instr, 12);
		args->clz.rm = DA_ARG_REG(instr, 0);
		break;
	case DA_GROUP_CP_DATA:
		args->cp_data.cond = DA_ARG_COND(instr, 28);
		args->cp_data.op_1 = DA_ARG(instr, 20, 0xf);
		args->cp_data.crn = DA_ARG_CPREG(instr, 16);
		args->cp_data.crd = DA_ARG_CPREG(instr, 12);
		args->cp_data.cp_num = DA_ARG(instr, 8, 0xf);
		args->cp_data.op_2 = DA_ARG(instr, 5, 0x7);
		args->cp_data.crm = DA_ARG_CPREG(instr, 0);
		break;
	case DA_GROUP_CP_LS:
		args->cp_ls.cond = DA_ARG_COND(instr, 28);
		args->cp_ls.p = DA_ARG_BOOL(instr, 24);
		args->cp_ls.sign = DA_ARG_BOOL(instr, 23);
		args->cp_ls.n = DA_ARG_BOOL(instr, 22);
		args->cp_ls.write = DA_ARG_BOOL(instr, 21);
		args->cp_ls.load = DA_ARG_BOOL(instr, 20);
		args->cp_ls.rn = DA_ARG_REG(instr, 16);
		args->cp_ls.crd = DA_ARG_CPREG(instr, 12);
		args->cp_ls.cp_num = DA_ARG(instr, 8, 0xf);
		args->cp_ls.imm = DA_ARG(instr, 0, 0xff);
		break;
	case DA_GROUP_CP_REG:
		args->cp_reg.cond = DA_ARG_COND(instr, 28);
		args->cp_reg.op_1 = DA_ARG(instr, 21, 0x7);
		args->cp_reg.load = DA_ARG_BOOL(instr, 20);
		args->cp_reg.crn = DA_ARG_CPREG(instr, 16);
		args->cp_reg.rd = DA_ARG_REG(instr, 12);
		args->cp_reg.cp_num = DA_ARG(instr, 8, 0xf);
		args->cp_reg.op_2 = DA_ARG(instr, 5, 0x7);
		args->cp_reg.crm = DA_ARG_CPREG(instr, 0);
		break;
	case DA_GROUP_DATA_IMM:
		args->data_imm.cond = DA_ARG_COND(instr, 28);
		args->data_imm.op = DA_ARG_DATA_OP(instr, 21);
		args->data_imm.flags = DA_ARG_BOOL(instr, 20);
		args->data_imm.rn = DA_ARG_REG(instr, 16);
		args->data_imm.rd = DA_ARG_REG(instr, 12);
		args->data_imm.imm = (DA_ARG(instr, 0, 0xff) >>
				      (DA_ARG(instr, 8, 0xf) << 1)) |
			(DA_ARG(instr, 0, 0xff) <<
			 (32 - (DA_ARG(instr, 8, 0xf) << 1)));
		break;
	case DA_GROUP_DATA_IMM_SH:
		args->data_imm_sh.cond = DA_ARG_COND(instr, 28);
		args->data_imm_sh.op = DA_ARG_DATA_OP(instr, 21);
		args->data_imm_sh.flags = DA_ARG_BOOL(instr, 20);
		args->data_imm_sh.rn = DA_ARG_REG(instr, 16);
		args->data_imm_sh.rd = DA_ARG_REG(instr, 12);
		args->data_imm_sh.sha = DA_ARG(instr, 7, 0x1f);
		args->data_imm_sh.sh = DA_ARG_SHIFT(instr, 5);
		args->data_imm_sh.rm = DA_ARG_REG(instr, 0);
		break;
	case DA_GROUP_DATA_REG_SH:
		args->data_reg_sh.cond = DA_ARG_COND(instr, 28);
		args->data_reg_sh.op = DA_ARG_DATA_OP(instr, 21);
		args->data_reg_sh.flags = DA_ARG_BOOL(instr, 20);
		args->data_reg_sh.rn = DA_ARG_REG(instr, 16);
		args->data_reg_sh.rd = DA_ARG_REG(instr, 12);
		args->data_reg_sh.rs = DA_ARG_REG(instr, 8);
		args->data_reg_sh.sh = DA_ARG_SHIFT(instr, 5);
		args->data_reg_sh.rm = DA_ARG_REG(instr, 0);
		break;
	case DA_GROUP_DSP_ADD_SUB:
		args->dsp_add_sub.cond = DA_ARG_COND(instr, 28);
		args->dsp_add_sub.op = DA_ARG(instr, 21, 0x3);
		args->dsp_add_sub.rn = DA_ARG_REG(instr, 16);
		args->dsp_add_sub.rd = DA_ARG_REG(instr, 12);
		args->dsp_add_sub.rm = DA_ARG_REG(instr, 0);
		break;
	case DA_GROUP_DSP_MUL:
		args->dsp_mul.cond = DA_ARG_COND(instr, 28);
		args->dsp_mul.op = DA_ARG(instr, 21, 0x3);
		args->dsp_mul.rd = DA_ARG_REG(instr, 16);
		args->dsp_mul.rn = DA_ARG_REG(instr, 12);
		args->dsp_mul.rs = DA_ARG_REG(instr, 12);
		args->dsp_mul.y = DA_ARG_BOOL(instr, 6);
		args->dsp_mul.x = DA_ARG_BOOL(instr, 5);
		args->dsp_mul.rm = DA_ARG_REG(instr, 0);
		break;
	case DA_GROUP_L_SIGN_IMM:
		args->l_sign_imm.cond = DA_ARG_COND(instr, 28);
		args->l_sign_imm.p = DA_ARG_BOOL(instr, 24);
		args->l_sign_imm.write = DA_ARG_BOOL(instr, 21);
		args->l_sign_imm.rn = DA_ARG_REG(instr, 16);
		args->l_sign_imm.rd = DA_ARG_REG(instr, 12);
		args->l_sign_imm.hword = DA_ARG_BOOL(instr, 5);
		args->l_sign_imm.off = (DA_ARG_BOOL(instr, 23) ? 1 : -1) *
			((DA_ARG(instr, 8, 0xf) << 4) | DA_ARG(instr, 0, 0xf));
		break;
	case DA_GROUP_L_SIGN_REG:
		args->l_sign_reg.cond = DA_ARG_COND(instr, 28);
		args->l_sign_reg.p = DA_ARG_BOOL(instr, 24);
		args->l_sign_reg.sign = DA_ARG_BOOL(instr, 23);
		args->l_sign_reg.write = DA_ARG_BOOL(instr, 21);
		args->l_sign_reg.rn = DA_ARG_REG(instr, 16);
		args->l_sign_reg.rd = DA_ARG_REG(instr, 12);
		args->l_sign_reg.hword = DA_ARG_BOOL(instr, 5);
		args->l_sign_reg.rm = DA_ARG_REG(instr, 0);
		break;
	case DA_GROUP_LS_HW_IMM:
		args->ls_hw_imm.cond = DA_ARG_COND(instr, 28);
		args->ls_hw_imm.p = DA_ARG_BOOL(instr, 24);
		args->ls_hw_imm.write = DA_ARG_BOOL(instr, 21);
		args->ls_hw_imm.load = DA_ARG_BOOL(instr, 20);
		args->ls_hw_imm.rn = DA_ARG_REG(instr, 16);
		args->ls_hw_imm.rd = DA_ARG_REG(instr, 12);
		args->ls_hw_imm.off = (DA_ARG_BOOL(instr, 23) ? 1 : -1) *
			((DA_ARG(instr, 8, 0xf) << 4) | DA_ARG(instr, 0, 0xf));
		break;
	case DA_GROUP_LS_HW_REG:
		args->ls_hw_reg.cond = DA_ARG_COND(instr, 28);
		args->ls_hw_reg.p = DA_ARG_BOOL(instr, 24);
		args->ls_hw_reg.sign = DA_ARG_BOOL(instr, 23);
		args->ls_hw_reg.write = DA_ARG_BOOL(instr, 21);
		args->ls_hw_reg.load = DA_ARG_BOOL(instr, 20);
		args->ls_hw_reg.rn = DA_ARG_REG(instr, 16);
		args->ls_hw_reg.rd = DA_ARG_REG(instr, 12);
		args->ls_hw_reg.rm = DA_ARG_REG(instr,  0);
		break;
	case DA_GROUP_LS_IMM:
		args->ls_imm.cond = DA_ARG_COND(instr, 28);
		args->ls_imm.p = DA_ARG_BOOL(instr, 24);
		args->ls_imm.byte = DA_ARG_BOOL(instr, 22);
		args->ls_imm.w = DA_ARG_BOOL(instr, 21);
		args->ls_imm.load = DA_ARG_BOOL(instr, 20);
		args->ls_imm.rn = DA_ARG_REG(instr, 16);
		args->ls_imm.rd = DA_ARG_REG(instr, 12);
		args->ls_imm.off = (DA_ARG_BOOL(instr, 23) ? 1 : -1) *
			DA_ARG(instr, 0, 0xfff);
		break;
	case DA_GROUP_LS_MULTI:
		args->ls_multi.cond = DA_ARG_COND(instr, 28);
		args->ls_multi.p = DA_ARG_BOOL(instr, 24);
		args->ls_multi.u = DA_ARG_BOOL(instr, 23);
		args->ls_multi.s = DA_ARG_BOOL(instr, 22);
		args->ls_multi.write = DA_ARG_BOOL(instr, 21);
		args->ls_multi.load = DA_ARG_BOOL(instr, 20);
		args->ls_multi.rn = DA_ARG_REG(instr, 16);
		args->ls_multi.reglist = DA_ARG(instr, 0, 0xffff);
		break;
	case DA_GROUP_LS_REG:
		args->ls_reg.cond = DA_ARG_COND(instr, 28);
		args->ls_reg.p = DA_ARG_BOOL(instr, 24);
		args->ls_reg.sign = DA_ARG_BOOL(instr, 23);
		args->ls_reg.byte = DA_ARG_BOOL(instr, 22);
		args->ls_reg.write = DA_ARG_BOOL(instr, 21);
		args->ls_reg.load = DA_ARG_BOOL(instr, 20);
		args->ls_reg.rn = DA_ARG_REG(instr, 16);
		args->ls_reg.rd = DA_ARG_REG(instr, 12);
		args->ls_reg.sha = DA_ARG(instr, 7, 0x1f);
		args->ls_reg.sh = DA_ARG_SHIFT(instr, 5);
		args->ls_reg.rm = DA_ARG_REG(instr, 0);
		break;
	case DA_GROUP_LS_TWO_IMM:
		args->ls_two_imm.cond = DA_ARG_COND(instr, 28);
		args->ls_two_imm.p = DA_ARG_BOOL(instr, 24);
		args->ls_two_imm.write = DA_ARG_BOOL(instr, 21);
		args->ls_two_imm.rn = DA_ARG_REG(instr, 16);
		args->ls_two_imm.rd = DA_ARG_REG(instr, 12);
		args->ls_two_imm.store = DA_ARG_BOOL(instr, 5);
		args->ls_two_imm.off = (DA_ARG_BOOL(instr, 23) ? 1 : -1) *
			((DA_ARG(instr, 8, 0xf) << 4) | DA_ARG(instr, 0, 0xf));
		break;
	case DA_GROUP_LS_TWO_REG:
		args->ls_two_reg.cond = DA_ARG_COND(instr, 28);
		args->ls_two_reg.p = DA_ARG_BOOL(instr, 24);
		args->ls_two_reg.sign = DA_ARG_BOOL(instr, 23);
		args->ls_two_reg.write = DA_ARG_BOOL(instr, 21);
		args->ls_two_reg.rn = DA_ARG_REG(instr, 16);
		args->ls_two_reg.rd = DA_ARG_REG(instr, 12);
		args->ls_two_reg.store = DA_ARG_BOOL(instr, 5);
		args->ls_two_reg.rm = DA_ARG_REG(instr, 0);
		break;
	case DA_GROUP_MRS:
		args->mrs.cond = DA_ARG_COND(instr, 28);
		args->mrs.r = DA_ARG_BOOL(instr, 22);
		args->mrs.rd = DA_ARG_REG(instr, 12);
		break;
	case DA_GROUP_MSR:
		args->msr.cond = DA_ARG_COND(instr, 28);
		args->msr.r = DA_ARG_BOOL(instr, 22);
		args->msr.mask = DA_ARG(instr, 16, 0xf);
		args->msr.rm = DA_ARG_REG(instr, 0);
		break;
	case DA_GROUP_MSR_IMM:
		args->msr_imm.cond = DA_ARG_COND(instr, 28);
		args->msr_imm.r = DA_ARG_BOOL(instr, 22);
		args->msr_imm.mask = DA_ARG(instr, 16, 0xf);
		args->msr_imm.imm = (DA_ARG(instr, 0, 0xff) >>
				     (DA_ARG(instr, 8, 0xf) << 1)) |
			(DA_ARG(instr, 0, 0xff) <<
			 (32 - (DA_ARG(instr, 8, 0xf) << 1)));
		break;
	case DA_GROUP_MUL:
		args->mul.cond = DA_ARG_COND(instr, 28);
		args->mul.acc = DA_ARG_BOOL(instr, 21);
		args->mul.flags = DA_ARG_BOOL(instr, 20);
		args->mul.rd = DA_ARG_REG(instr, 16);
		args->mul.rn = DA_ARG_REG(instr, 12);
		args->mul.rs = DA_ARG_REG(instr, 8);
		args->mul.rm = DA_ARG_REG(instr, 0);
		break;
	case DA_GROUP_MULL:
		args->mull.cond = DA_ARG_COND(instr, 28);
		args->mull.sign = DA_ARG_BOOL(instr, 22);
		args->mull.acc = DA_ARG_BOOL(instr, 21);
		args->mull.flags = DA_ARG_BOOL(instr, 20);
		args->mull.rd_hi = DA_ARG_REG(instr, 16);
		args->mull.rd_lo = DA_ARG_REG(instr, 12);
		args->mull.rs = DA_ARG_REG(instr, 8);
		args->mull.rm = DA_ARG_REG(instr, 0);
		break;
	case DA_GROUP_SWI:
		args->swi.cond = DA_ARG_COND(instr, 28);
		args->swi.imm = DA_ARG(instr, 0, 0xffffff);
		break;
	case DA_GROUP_SWP:
		args->swp.cond = DA_ARG_COND(instr, 28);
		args->swp.byte = DA_ARG_BOOL(instr, 22);
		args->swp.rn = DA_ARG_REG(instr, 16);
		args->swp.rd = DA_ARG_REG(instr, 12);
		args->swp.rm = DA_ARG_REG(instr, 10);
		break;
	case DA_GROUP_UNDEF_1:
	case DA_GROUP_UNDEF_2:
	case DA_GROUP_UNDEF_3:
	case DA_GROUP_UNDEF_4:
	case DA_GROUP_UNDEF_5:
		break;
	}
}
