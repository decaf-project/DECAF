/*
 * print.c - Instruction printer functions
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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "libdisarm_args.h"
#include "libdisarm_macros.h"
#include "libdisarm_print.h"
#include "libdisarm_types.h"


static const char *const da_cond_map[] = {
	"eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc",
	"hi", "ls", "ge", "lt", "gt", "le", "", "nv"
};

static const char *const da_shift_map[] = {
	"lsl", "lsr", "asr", "ror"
};

static const char *const da_data_op_map[] = {
	"and", "eor", "sub", "rsb", "add", "adc", "sbc", "rsc",
	"tst", "teq", "cmp", "cmn", "orr", "mov", "bic", "mvn"
};


static void
da_reglist_fprint(FILE *f, da_uint_t reglist)
{
	int comma = 0;
	int range_start = -1;
	int i = 0;
	for (i = 0; reglist; i++) {
		if (reglist & 1 && range_start == -1) {
			range_start = i;
		}

		reglist >>= 1;

		if (!(reglist & 1)) {
			if (range_start == i) {
				if (comma) fprintf(f, ",");
				fprintf(f, " r%d", i);
				comma = 1;
			} else if (i > 0 && range_start == i-1) {
				if (comma) fprintf(f, ",");
				fprintf(f, " r%d, r%d",
					range_start, i);
				comma = 1;
			} else if (range_start >= 0) {
				if (comma) fprintf(f, ",");
				fprintf(f, " r%d-r%d", range_start, i);
				comma = 1;
			}
			range_start = -1;
		}
	}
}


static void
da_instr_fprint_bkpt(FILE *f, const da_instr_t *instr,
		     const da_args_bkpt_t *args, da_addr_t addr)
{
	fprintf(f, "bkpt%s\t0x%x", da_cond_map[args->cond], args->imm);
}

static void
da_instr_fprint_bl(FILE *f, const da_instr_t *instr,
		   const da_args_bl_t *args, da_addr_t addr)
{
	da_uint_t target = da_instr_branch_target(args->off, addr);
	fprintf(f, "b%s%s\t0x%x", (args->link ? "l" : ""),
		da_cond_map[args->cond], target);
}

static void
da_instr_fprint_blx_imm(FILE *f, const da_instr_t *instr,
			const da_args_blx_imm_t *args, da_addr_t addr)
{
	da_uint_t target = da_instr_branch_target(args->off, addr);
	fprintf(f, "blx\t0x%x", target | args->h);
}

static void
da_instr_fprint_blx_reg(FILE *f, const da_instr_t *instr,
			const da_args_blx_reg_t *args, da_addr_t addr)
{
	fprintf(f, "b%sx%s\tr%d", (args->link ? "l" : ""),
		da_cond_map[args->cond], args->rm);
}

static void
da_instr_fprint_clz(FILE *f, const da_instr_t *instr,
		    const da_args_clz_t *args, da_addr_t addr)
{
	fprintf(f, "clz%s\tr%d, r%d", da_cond_map[args->cond],
		args->rd, args->rm);
}

static void
da_instr_fprint_cp_data(FILE *f, const da_instr_t *instr,
			const da_args_cp_data_t *args, da_addr_t addr)
{
	fprintf(f, "cdp%s\tp%d, %d, cr%d, cr%d, cr%d, %d",
		(args->cond != DA_COND_NV ? da_cond_map[args->cond] : "2"),
		args->cp_num, args->op_1, args->crd, args->crn, args->crm,
		args->op_2);
}

static void
da_instr_fprint_cp_ls(FILE *f, const da_instr_t *instr,
		      const da_args_cp_ls_t *args, da_addr_t addr)
{
	fprintf(f, "%sc%s%s\tp%d, cr%d, [r%d", (args->load ? "ld" : "st"),
		(args->cond != DA_COND_NV ? da_cond_map[args->cond] : "2"),
		(args->n ? "l" : ""), args->cp_num, args->crd, args->rn);

	if (!args->p) fprintf(f, "]");

	if (!(args->sign || args->p)) {
		fprintf(f, ", {%d}", args->imm);
	} else if (args->imm > 0) {
		fprintf(f, ", #%s0x%x", (args->sign ? "" : "-"),
			(args->imm << 2));
	}

	if (args->p) fprintf(f, "]%s", (args->write ? "!" : ""));
}

static void
da_instr_fprint_cp_reg(FILE *f, const da_instr_t *instr,
		       const da_args_cp_reg_t *args, da_addr_t addr)
{
	fprintf(f, "m%s%s\tp%d, %d, r%d, cr%d, cr%d, %d",
		(args->load ? "rc" : "cr"),
		(args->cond != DA_COND_NV ? da_cond_map[args->cond] : "2"),
		args->cp_num, args->op_1, args->rd, args->crn, args->crm,
		args->op_2);
}

static void
da_instr_fprint_data_imm(FILE *f, const da_instr_t *instr,
			 const da_args_data_imm_t *args, da_addr_t addr)
{
	fprintf(f, "%s%s%s\t", da_data_op_map[args->op],
		da_cond_map[args->cond],
		((args->flags && (args->op < DA_DATA_OP_TST ||
				  args->op > DA_DATA_OP_CMN)) ? "s" : ""));

	if (args->op >= DA_DATA_OP_TST && args->op <= DA_DATA_OP_CMN) {
		fprintf(f, "r%d", args->rn);
	} else if (args->op == DA_DATA_OP_MOV || args->op == DA_DATA_OP_MVN) {
		fprintf(f, "r%d", args->rd);
	} else {
		fprintf(f, "r%d, r%d", args->rd, args->rn);
	}

	fprintf(f, ", #0x%x", args->imm);

	if (args->rn == DA_REG_R15) {
		if (args->op == DA_DATA_OP_ADD) {
			fprintf(f, "\t; 0x%x", addr + 8 + args->imm);
		} else if (args->op == DA_DATA_OP_SUB) {
			fprintf(f, "\t; 0x%x", addr + 8 - args->imm);
		}
	}
}

static void
da_instr_fprint_data_imm_sh(FILE *f, const da_instr_t *instr,
			    const da_args_data_imm_sh_t *args, da_addr_t addr)
{
	fprintf(f, "%s%s%s\t", da_data_op_map[args->op],
		da_cond_map[args->cond],
		((args->flags &&
		  (args->op < DA_DATA_OP_TST ||
		   args->op > DA_DATA_OP_CMN)) ? "s" : ""));

	if (args->op >= DA_DATA_OP_TST && args->op <= DA_DATA_OP_CMN) {
		fprintf(f, "r%d, r%d", args->rn, args->rm);
	} else if (args->op == DA_DATA_OP_MOV || args->op == DA_DATA_OP_MVN) {
		fprintf(f, "r%d, r%d", args->rd, args->rm);
	} else {
		fprintf(f, "r%d, r%d, r%d", args->rd, args->rn, args->rm);
	}

	da_uint_t sha = args->sha;
	
	if (args->sh == DA_SHIFT_LSR || args->sh == DA_SHIFT_ASR) {
		sha = ((sha > 0) ? sha : 32);
	}

	if (sha > 0) {
		fprintf(f, ", %s #0x%x", da_shift_map[args->sh], sha);
	} else if (args->sh == DA_SHIFT_ROR) {
		fprintf(f, ", rrx");
	}
}

static void
da_instr_fprint_data_reg_sh(FILE *f, const da_instr_t *instr,
			    const da_args_data_reg_sh_t *args, da_addr_t addr)
{
	fprintf(f, "%s%s%s\t", da_data_op_map[args->op],
		da_cond_map[args->cond],
		((args->flags &&
		  (args->op < DA_DATA_OP_TST ||
		   args->op > DA_DATA_OP_CMN)) ? "s" : ""));

	if (args->op >= DA_DATA_OP_TST && args->op <= DA_DATA_OP_CMN) {
		fprintf(f, "r%d, r%d", args->rn, args->rm);
	} else if (args->op == DA_DATA_OP_MOV || args->op == DA_DATA_OP_MVN) {
		fprintf(f, "r%d, r%d", args->rd, args->rm);
	} else {
		fprintf(f, "r%d, r%d, r%d", args->rd, args->rn, args->rm);
	}

	fprintf(f, ", %s r%d", da_shift_map[args->sh], args->rs);
}

static void
da_instr_fprint_dsp_add_sub(FILE *f, const da_instr_t *instr,
			    const da_args_dsp_add_sub_t *args, da_addr_t addr)
{
	fprintf(f, "q%s%s%s\tr%d, r%d, r%d", ((args->op & 2) ? "d" : ""),
		((args->op & 1) ? "sub" : "add"), da_cond_map[args->cond],
		args->rd, args->rm, args->rn);
}

static void
da_instr_fprint_dsp_mul(FILE *f, const da_instr_t *instr,
			const da_args_dsp_mul_t *args, da_addr_t addr)
{
	switch (args->op) {
	case 0:
		fprintf(f, "smla%s%s%s\tr%d, r%d, r%d, r%d",
			(args->x ? "t" : "b"), (args->y ? "t" : "b"),
			da_cond_map[args->cond], args->rd, args->rm, args->rs,
			args->rn);
		break;
	case 1:
		fprintf(f, "s%sw%s%s\tr%d, r%d, r%d",
			(args->x ? "mul" : "mla"), (args->y ? "t" : "b"),
			da_cond_map[args->cond], args->rd, args->rm, args->rs);
		if (!args->x) fprintf(f, ", r%d", args->rn);
		break;
	case 2:
		fprintf(f, "smlal%s%s%s\tr%d, r%d, r%d, r%d",
			(args->x ? "t" : "b"), (args->y ? "t" : "b"),
			da_cond_map[args->cond], args->rn, args->rd, args->rm,
			args->rs);
		break;
	case 3:
		fprintf(f, "smul%s%s%s\tr%d, r%d, r%d",
			(args->x ? "t" : "b"), (args->y ? "t" : "b"),
			da_cond_map[args->cond], args->rd, args->rm, args->rs);
		break;
	}
}

static void
da_instr_fprint_l_sign_imm(FILE *f, const da_instr_t *instr,
			   const da_args_l_sign_imm_t *args, da_addr_t addr)
{
	fprintf(f, "ldr%ss%s\tr%d, [r%d", da_cond_map[args->cond],
		(args->hword ? "h" : "b"), args->rd, args->rn);

	if (!args->p) fprintf(f, "]");

	if (args->off != 0) {
		fprintf(f, ", #%s0x%x", (args->off < 0 ? "-" : ""),
			abs(args->off));
	}

	if (args->p) fprintf(f, "]%s", (args->write ? "!" : ""));

	if (args->rn == DA_REG_R15) {
		fprintf(f, "\t; 0x%x", addr + 8 + args->off);
	}
}

static void
da_instr_fprint_l_sign_reg(FILE *f, const da_instr_t *instr,
			   const da_args_l_sign_reg_t *args, da_addr_t addr)
{
	fprintf(f, "ldr%ss%s\tr%d, [r%d", da_cond_map[args->cond],
		(args->hword ? "h" : "b"), args->rd, args->rn);

	if (!args->p) fprintf(f, "]");

	fprintf(f, ", %sr%d", (args->sign ? "" : "-"), args->rm);

	if (args->p) fprintf(f, "]%s", (args->write ? "!" : ""));
}

static void
da_instr_fprint_ls_hw_imm(FILE *f, const da_instr_t *instr,
			  const da_args_ls_hw_imm_t *args, da_addr_t addr)
{
	fprintf(f, "%sr%sh\tr%d, [r%d", (args->load ? "ld" : "st"),
		da_cond_map[args->cond], args->rd, args->rn);

	if (!args->p) fprintf(f, "]");

	if (args->off != 0) {
		fprintf(f, ", #%s0x%x", (args->off < 0 ? "-" : ""),
			abs(args->off));
	}

	if (args->p) fprintf(f, "]%s", (args->write ? "!" : ""));

	if (args->rn == DA_REG_R15) {
		fprintf(f, "\t; 0x%x", addr + 8 + args->off);
	}
}

static void
da_instr_fprint_ls_hw_reg(FILE *f, const da_instr_t *instr,
			  const da_args_ls_hw_reg_t *args, da_addr_t addr)
{
	fprintf(f, "%sr%sh\tr%d, [r%d", (args->load ? "ld" : "st"),
		da_cond_map[args->cond], args->rd, args->rn);

	if (!args->p) fprintf(f, "]");

	fprintf(f, ", %sr%d", (args->sign ? "" : "-"), args->rm);

	if (args->p) fprintf(f, "]%s", (args->write ? "!" : ""));
}

static void
da_instr_fprint_ls_imm(FILE *f, const da_instr_t *instr,
		       const da_args_ls_imm_t *args, da_addr_t addr)
{
	fprintf(f, "%sr%s%s%s\tr%d, [r%d", (args->load ? "ld" : "st"),
		da_cond_map[args->cond], (args->byte ? "b" : ""),
		((!args->p && args->w) ? "t" : ""), args->rd, args->rn);

	if (!args->p) fprintf(f, "]");

	if (args->off != 0) {
		fprintf(f, ", #%s0x%x", (args->off < 0 ? "-" : ""),
			abs(args->off));
	}

	if (args->p) fprintf(f, "]%s", (args->w ? "!" : ""));

	if (args->rn == DA_REG_R15) {
		fprintf(f, "\t; 0x%x", addr + 8 + args->off);
	}
}

static void
da_instr_fprint_ls_multi(FILE *f, const da_instr_t *instr,
			 const da_args_ls_multi_t *args, da_addr_t addr)
{
	fprintf(f, "%sm%s%s%s\tr%d%s, {", (args->load ? "ld" : "st"),
		da_cond_map[args->cond], (args->u ? "i" : "d"),
		(args->p ? "b" : "a"), args->rn, (args->write ? "!" : ""));
      
	da_reglist_fprint(f, args->reglist);

	fprintf(f, " }%s", (args->s ? "^" : ""));
}

static void
da_instr_fprint_ls_reg(FILE *f, const da_instr_t *instr,
		       const da_args_ls_reg_t *args, da_addr_t addr)
{
	fprintf(f, "%sr%s%s%s\tr%d, [r%d", (args->load ? "ld" : "st"),
		da_cond_map[args->cond], (args->byte ? "b" : ""),
		((!args->p && args->write) ? "t" : ""), args->rd, args->rn);

	if (!args->p) fprintf(f, "]");

	fprintf(f, ", %sr%d", (args->sign ? "" : "-"), args->rm);

	da_uint_t sha = args->sha;
	
	if (args->sh == DA_SHIFT_LSR || args->sh == DA_SHIFT_ASR) {
		sha = (sha ? sha : 32);
	}

	if (sha > 0) {
		fprintf(f, ", %s #0x%x", da_shift_map[args->sh], sha);
	} else if (args->sh == DA_SHIFT_ROR) {
		fprintf(f, ", rrx");
	}

	if (args->p) fprintf(f, "]%s", (args->write ? "!" : ""));
}

static void
da_instr_fprint_ls_two_imm(FILE *f, const da_instr_t *instr,
			   const da_args_ls_two_imm_t *args, da_addr_t addr)
{
	fprintf(f, "%sr%sd\tr%d, [r%d", (args->store ? "st" : "ld"),
		da_cond_map[args->cond], args->rd, args->rn);

	if (!args->p) fprintf(f, "]");

	if (args->off != 0) {
		fprintf(f, ", #%s0x%x", (args->off < 0 ? "-" : ""),
			abs(args->off));
	}

	if (args->p) fprintf(f, "]%s", (args->write ? "!" : ""));

	if (args->rn == DA_REG_R15) {
		fprintf(f, "\t; 0x%x", addr + 8 + args->off);
	}
}

static void
da_instr_fprint_ls_two_reg(FILE *f, const da_instr_t *instr,
			   const da_args_ls_two_reg_t *args, da_addr_t addr)
{
	fprintf(f, "%sr%sd\tr%d, [r%d", (args->store ? "st" : "ld"),
		da_cond_map[args->cond], args->rd, args->rn);

	if (!args->p) fprintf(f, "]");

	fprintf(f, ", %sr%d", (args->sign ? "" : "-"), args->rm);

	if (args->p) fprintf(f, "]%s", (args->write ? "!" : ""));
}

static void
da_instr_fprint_mrs(FILE *f, const da_instr_t *instr,
		    const da_args_mrs_t *args, da_addr_t addr)
{
	fprintf(f, "mrs%s\tr%d, %s", da_cond_map[args->cond],
		args->rd, (args->r ? "SPSR" : "CPSR"));
}

static void
da_instr_fprint_msr(FILE *f, const da_instr_t *instr,
		    const da_args_msr_t *args, da_addr_t addr)
{
	fprintf(f, "msr%s\t%s_%s%s%s%s", da_cond_map[args->cond],
		(args->r ? "SPSR" : "CPSR"), ((args->mask & 1) ? "c" : ""),
		((args->mask & 2) ? "x" : ""), ((args->mask & 4) ? "s" : ""),
		((args->mask & 8) ? "f" : ""));

	fprintf(f, ", r%d", args->rm);
}

static void
da_instr_fprint_msr_imm(FILE *f, const da_instr_t *instr,
			const da_args_msr_imm_t *args, da_addr_t addr)
{
	fprintf(f, "msr%s\t%s_%s%s%s%s, #0x%x", da_cond_map[args->cond],
		(args->r ? "SPSR" : "CPSR"), ((args->mask & 1) ? "c" : ""),
		((args->mask & 2) ? "x" : ""), ((args->mask & 4) ? "s" : ""),
		((args->mask & 8) ? "f" : ""), args->imm);
}

static void
da_instr_fprint_mul(FILE *f, const da_instr_t *instr,
		    const da_args_mul_t *args, da_addr_t addr)
{
	fprintf(f, "m%s%s%s\tr%d, r%d, r%d", (args->acc ? "la" : "ul"),
		da_cond_map[args->cond], (args->flags ? "s" : ""),
		args->rd, args->rm, args->rs);

	if (args->acc) fprintf(f, ", r%d", args->rn);
}

static void
da_instr_fprint_mull(FILE *f, const da_instr_t *instr,
		     const da_args_mull_t *args, da_addr_t addr)
{
	fprintf(f, "%sm%sl%s%s\tr%d, r%d, r%d, r%d",
		(args->sign ? "s" : "u"), (args->acc ? "la" : "ul"),
		da_cond_map[args->cond], (args->flags ? "s" : ""),
		args->rd_lo, args->rd_hi, args->rm, args->rs);
}

static void
da_instr_fprint_swi(FILE *f, const da_instr_t *instr,
		    const da_args_swi_t *args, da_addr_t addr)
{
	fprintf(f, "swi%s\t0x%x", da_cond_map[args->cond], args->imm);
}

static void
da_instr_fprint_swp(FILE *f, const da_instr_t *instr,
		    const da_args_swp_t *args, da_addr_t addr)
{
	fprintf(f, "swp%s%s\tr%d, r%d, [r%d]", da_cond_map[args->cond],
		(args->byte ? "b" : ""), args->rd, args->rm, args->rn);
}

DA_API void
da_instr_fprint(FILE *f, const da_instr_t *instr, const da_instr_args_t *args,
		da_addr_t addr)
{
	switch (instr->group) {
	case DA_GROUP_BKPT:
		da_instr_fprint_bkpt(f, instr, &args->bkpt, addr);
		break;
	case DA_GROUP_BL:
		da_instr_fprint_bl(f, instr, &args->bl, addr);
		break;
	case DA_GROUP_BLX_IMM:
		da_instr_fprint_blx_imm(f, instr, &args->blx_imm, addr);
		break;
	case DA_GROUP_BLX_REG:
		da_instr_fprint_blx_reg(f, instr, &args->blx_reg, addr);
		break;
	case DA_GROUP_CLZ:
		da_instr_fprint_clz(f, instr, &args->clz, addr);
		break;
	case DA_GROUP_CP_DATA:
		da_instr_fprint_cp_data(f, instr, &args->cp_data, addr);
		break;
	case DA_GROUP_CP_LS:
		da_instr_fprint_cp_ls(f, instr, &args->cp_ls, addr);
		break;
	case DA_GROUP_CP_REG:
		da_instr_fprint_cp_reg(f, instr, &args->cp_reg, addr);
		break;
	case DA_GROUP_DATA_IMM:
		da_instr_fprint_data_imm(f, instr, &args->data_imm, addr);
		break;
	case DA_GROUP_DATA_IMM_SH:
		da_instr_fprint_data_imm_sh(f, instr, &args->data_imm_sh,
					    addr);
		break;
	case DA_GROUP_DATA_REG_SH:
		da_instr_fprint_data_reg_sh(f, instr, &args->data_reg_sh,
					    addr);
		break;
	case DA_GROUP_DSP_ADD_SUB:
		da_instr_fprint_dsp_add_sub(f, instr, &args->dsp_add_sub,
					    addr);
		break;
	case DA_GROUP_DSP_MUL:
		da_instr_fprint_dsp_mul(f, instr, &args->dsp_mul, addr);
		break;
	case DA_GROUP_L_SIGN_IMM:
		da_instr_fprint_l_sign_imm(f, instr, &args->l_sign_imm, addr);
		break;
	case DA_GROUP_L_SIGN_REG:
		da_instr_fprint_l_sign_reg(f, instr, &args->l_sign_reg, addr);
		break;
	case DA_GROUP_LS_HW_IMM:
		da_instr_fprint_ls_hw_imm(f, instr, &args->ls_hw_imm, addr);
		break;
	case DA_GROUP_LS_HW_REG:
		da_instr_fprint_ls_hw_reg(f, instr, &args->ls_hw_reg, addr);
		break;
	case DA_GROUP_LS_IMM:
		da_instr_fprint_ls_imm(f, instr, &args->ls_imm, addr);
		break;
	case DA_GROUP_LS_MULTI:
		da_instr_fprint_ls_multi(f, instr, &args->ls_multi, addr);
		break;
	case DA_GROUP_LS_REG:
		da_instr_fprint_ls_reg(f, instr, &args->ls_reg, addr);
		break;
	case DA_GROUP_LS_TWO_IMM:
		da_instr_fprint_ls_two_imm(f, instr, &args->ls_two_imm, addr);
		break;
	case DA_GROUP_LS_TWO_REG:
		da_instr_fprint_ls_two_reg(f, instr, &args->ls_two_reg, addr);
		break;
	case DA_GROUP_MRS:
		da_instr_fprint_mrs(f, instr, &args->mrs, addr);
		break;
	case DA_GROUP_MSR:
		da_instr_fprint_msr(f, instr, &args->msr, addr);
		break;
	case DA_GROUP_MSR_IMM:
		da_instr_fprint_msr_imm(f, instr, &args->msr_imm, addr);
		break;
	case DA_GROUP_MUL:
		da_instr_fprint_mul(f, instr, &args->mul, addr);
		break;
	case DA_GROUP_MULL:
		da_instr_fprint_mull(f, instr, &args->mull, addr);
		break;
	case DA_GROUP_SWI:
		da_instr_fprint_swi(f, instr, &args->swi, addr);
		break;
	case DA_GROUP_SWP:
		da_instr_fprint_swp(f, instr, &args->swp, addr);
		break;
	case DA_GROUP_UNDEF_1:
	case DA_GROUP_UNDEF_2:
	case DA_GROUP_UNDEF_3:
	case DA_GROUP_UNDEF_4:
	case DA_GROUP_UNDEF_5:
		fprintf(f, "undefined");
		break;
	}
}
