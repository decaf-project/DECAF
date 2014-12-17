/**
 * @file This is a reimplementation of the print.c file so it uses ostream instead
 * This helps since I want to convert things to strings and not write them into a file
 */
#include <assert.h>
#include <stdlib.h>
#include "libdisarm_args.h"
#include "libdisarm_macros.h"
#include "libdisarm_types.h"

#include "libdisarm_print_cpp.h"

using namespace std;

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
da_reglist_ostream(ostream& os, da_uint_t reglist)
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
                                if (comma) os << ',';
                                os << " r" << i;
                                comma = 1;
                        } else if (i > 0 && range_start == i-1) {
                                if (comma) os << ',';
                                os << " r" << range_start << ", r" << i,
                                comma = 1;
                        } else if (range_start >= 0) {
                                if (comma) os << ',';
                                os << " r" << range_start << "-r" << i;
                                comma = 1;
                        }
                        range_start = -1;
                }
        }
}


static void
da_instr_ostream_bkpt(ostream& os, const da_instr_t *instr,
                     const da_args_bkpt_t *args, da_addr_t addr)
{
        os << "bkpt" << da_cond_map[args->cond] << "\t0x" << hex << args->imm << dec;
}

static void
da_instr_ostream_bl(ostream& os, const da_instr_t *instr,
                   const da_args_bl_t *args, da_addr_t addr)
{
        da_uint_t target = da_instr_branch_target(args->off, addr);
        os << 'b' << (args->link ? "l" : "") << da_cond_map[args->cond] << "\t0x" << hex << target << dec;
}

static void
da_instr_ostream_blx_imm(ostream& os, const da_instr_t *instr,
                        const da_args_blx_imm_t *args, da_addr_t addr)
{
        da_uint_t target = da_instr_branch_target(args->off, addr);
        os << "blx\t0x" << hex << (target | args->h) << dec;
}

static void
da_instr_ostream_blx_reg(ostream& os, const da_instr_t *instr,
                        const da_args_blx_reg_t *args, da_addr_t addr)
{
        os << 'b' << (args->link ? "l" : "") << 'x' << da_cond_map[args->cond] << "\tr" << args->rm;
}

static void
da_instr_ostream_clz(ostream& os, const da_instr_t *instr,
                    const da_args_clz_t *args, da_addr_t addr)
{
        os << "clz" << da_cond_map[args->cond] << "\tr" << args->rd << ", r" << args->rm;
}

static void
da_instr_ostream_cp_data(ostream& os, const da_instr_t *instr,
                        const da_args_cp_data_t *args, da_addr_t addr)
{
        os << "cdp" <<
                (args->cond != DA_COND_NV ? da_cond_map[args->cond] : "2")
                << "\tp" << args->cp_num << ", " << args->op_1 << ", cr" << args->crd << ", cr" << args->crn << ", cr" << args->crm
                << ", " <<args->op_2;
}

static void
da_instr_ostream_cp_ls(ostream& os, const da_instr_t *instr,
                      const da_args_cp_ls_t *args, da_addr_t addr)
{
        os << (args->load ? "ld" : "st") << 'c' <<
                (args->cond != DA_COND_NV ? da_cond_map[args->cond] : "2") <<
                (args->n ? "l" : "")<< "\tp" << args->cp_num << ", cr" << args->crd << ", [r" << args->rn;

        if (!args->p) os << ']';

        if (!(args->sign || args->p)) {
                os << ", {" << args->imm << '}';
        } else if (args->imm > 0) {
                os << ", #" << (args->sign ? "" : "-") << "0x" << hex <<
                    (args->imm << 2) << dec;
        }

        if (args->p) os << ']' << (args->write ? "!" : "");
}

static void
da_instr_ostream_cp_reg(ostream& os, const da_instr_t *instr,
                       const da_args_cp_reg_t *args, da_addr_t addr)
{
        os << 'm' <<
                (args->load ? "rc" : "cr") <<
                (args->cond != DA_COND_NV ? da_cond_map[args->cond] : "2") << "\tp" <<
                args->cp_num << ", " << args->op_1 << ", r" << args->rd << ", cr" << args->crn << ", cr" << args->crm
                << ", " << args->op_2;
}

static void
da_instr_ostream_data_imm(ostream& os, const da_instr_t *instr,
                         const da_args_data_imm_t *args, da_addr_t addr)
{
        os << da_data_op_map[args->op] << da_cond_map[args->cond] <<
          ((args->flags && (args->op < DA_DATA_OP_TST || args->op > DA_DATA_OP_CMN)) ? "s" : "")
          << '\t' ;

        if (args->op >= DA_DATA_OP_TST && args->op <= DA_DATA_OP_CMN) {
                os << 'r' << args->rn;
        } else if (args->op == DA_DATA_OP_MOV || args->op == DA_DATA_OP_MVN) {
                os << 'r' << args->rd;
        } else {
                os << 'r' << args->rd << ", r" << args->rn;
        }

        os << ", #0x" << hex << args->imm << dec;

        if (args->rn == DA_REG_R15) {
                if (args->op == DA_DATA_OP_ADD) {
                        os << "\t; 0x" << hex << (addr + 8 + args->imm) << dec;
                } else if (args->op == DA_DATA_OP_SUB) {
                        os << "\t; 0x" << hex << (addr + 8 - args->imm) << dec;
                }
        }
}

static void
da_instr_ostream_data_imm_sh(ostream& os, const da_instr_t *instr,
                            const da_args_data_imm_sh_t *args, da_addr_t addr)
{
        os << da_data_op_map[args->op] <<
                da_cond_map[args->cond] <<
                ((args->flags &&
                  (args->op < DA_DATA_OP_TST ||
                   args->op > DA_DATA_OP_CMN)) ? "s" : "")
                   << '\t';

        if (args->op >= DA_DATA_OP_TST && args->op <= DA_DATA_OP_CMN) {
                os << 'r' << args->rn << ", r" << args->rm;
        } else if (args->op == DA_DATA_OP_MOV || args->op == DA_DATA_OP_MVN) {
                os << 'r' << args->rd << ", r" << args->rm;
        } else {
                os << 'r' << args->rd << ", r" << args->rn << ", r" << args->rm;
        }

        da_uint_t sha = args->sha;

        if (args->sh == DA_SHIFT_LSR || args->sh == DA_SHIFT_ASR) {
                sha = ((sha > 0) ? sha : 32);
        }

        if (sha > 0) {
                os << ", " << da_shift_map[args->sh] << " #0x" << hex << sha << dec;
        } else if (args->sh == DA_SHIFT_ROR) {
                os << ", rrx";
        }
}

static void
da_instr_ostream_data_reg_sh(ostream& os, const da_instr_t *instr,
                            const da_args_data_reg_sh_t *args, da_addr_t addr)
{
        os << da_data_op_map[args->op] <<
                da_cond_map[args->cond] <<
                ((args->flags &&
                  (args->op < DA_DATA_OP_TST ||
                   args->op > DA_DATA_OP_CMN)) ? "s" : "") << '\t';

        if (args->op >= DA_DATA_OP_TST && args->op <= DA_DATA_OP_CMN) {
                os << 'r' << args->rn << ", r" << args->rm;
        } else if (args->op == DA_DATA_OP_MOV || args->op == DA_DATA_OP_MVN) {
                os << 'r' << args->rd << ", r"<< args->rm;
        } else {
                os << 'r' << args->rd << ", r" << args->rn << ", r" << args->rm;
        }

       os << ", " << da_shift_map[args->sh] << " r" << args->rs;
}

static void
da_instr_ostream_dsp_add_sub(ostream& os, const da_instr_t *instr,
                            const da_args_dsp_add_sub_t *args, da_addr_t addr)
{
        os << 'q' << ((args->op & 2) ? "d" : "") << ((args->op & 1) ? "sub" : "add") << da_cond_map[args->cond]
            << "\tr" << args->rd << ", r" << args->rm << ", r" << args->rn;
}

static void
da_instr_ostream_dsp_mul(ostream& os, const da_instr_t *instr,
                        const da_args_dsp_mul_t *args, da_addr_t addr)
{
        switch (args->op) {
        case 0:
                os << "smla" << (args->x ? 't' : 'b') << (args->y ? 't' : 'b') << da_cond_map[args->cond] << "\tr"
                    << args->rd << ", r" << args->rm << ", r" << args->rs << ", r" << args->rn;
                break;
        case 1:
                os << 's' << (args->x ? "mul" : "mla") << 'w' << (args->y ? 't' : 'b') << da_cond_map[args->cond]
                    << "\tr" << args->rd << ", r" << args->rm << ", r" << args->rs;
                if (!args->x) os << ", r" << args->rn;
                break;
        case 2:
                os << "smlal" << (args->x ? 't' : 'b') << (args->y ? 't' : 'b') << da_cond_map[args->cond]
                    << "\tr" << args->rn << ", r" << args->rd << ", r" << args->rm << ", r" << args->rs;
                break;
        case 3:
                os << "smul" << (args->x ? 't' : 'b') << (args->y ? 't' : 'b') << da_cond_map[args->cond]
                    << "\tr" << args->rd << ", r" << args->rm << ", r" << args->rs;
                break;
        }
}

static void
da_instr_ostream_l_sign_imm(ostream& os, const da_instr_t *instr,
                           const da_args_l_sign_imm_t *args, da_addr_t addr)
{
        os << "ldr" << da_cond_map[args->cond] << 's' << (args->hword ? 'h' : 'b') << "\tr" << args->rd << ", [r" << args->rn;

        if (!args->p) os << ']';

        if (args->off != 0) {
                os << ", #" << (args->off < 0 ? "-" : "") << "0x" << hex << (abs(args->off)) << dec;
        }

        if (args->p) os << ']' << (args->write ? "!" : "");

        if (args->rn == DA_REG_R15) {
                os << "\t; 0x" << hex << (addr + 8 + args->off) << dec;
        }
}

static void
da_instr_ostream_l_sign_reg(ostream& os, const da_instr_t *instr,
                           const da_args_l_sign_reg_t *args, da_addr_t addr)
{
        os << "ldr" << da_cond_map[args->cond] << 's' << (args->hword ? 'h' : 'b')
            << "\tr" << args->rd << ", [r" << args->rn;

        if (!args->p) os << ']';

        os << ", " << (args->sign ? "" : "-") << 'r' << args->rm;

        if (args->p) os << ']' << (args->write ? "!" : "");
}

static void
da_instr_ostream_ls_hw_imm(ostream& os, const da_instr_t *instr,
                          const da_args_ls_hw_imm_t *args, da_addr_t addr)
{
        os << (args->load ? "ld" : "st") << 'r' << da_cond_map[args->cond]
            << "h\tr" << args->rd << ", [r" << args->rn;

        if (!args->p) os << ']';

        if (args->off != 0) {
                os << ", #" << (args->off < 0 ? "-" : "") << "0x" << hex << (abs(args->off)) << dec;
        }

        if (args->p) os << ']' << (args->write ? "!" : "");

        if (args->rn == DA_REG_R15) {
                os << "\t; 0x" << hex << (addr + 8 + args->off) << dec;
        }
}

static void
da_instr_ostream_ls_hw_reg(ostream& os, const da_instr_t *instr,
                          const da_args_ls_hw_reg_t *args, da_addr_t addr)
{
        os << (args->load ? "ld" : "st") << 'r' << da_cond_map[args->cond]
            << "h\tr" << args->rd << ", [r" << args->rn;

        if (!args->p) os << ']';

        os << ", " << (args->sign ? "" : "-") << 'r' << args->rm;

        if (args->p) os << ']' << (args->write ? "!" : "");
}

static void
da_instr_ostream_ls_imm(ostream& os, const da_instr_t *instr,
                       const da_args_ls_imm_t *args, da_addr_t addr)
{
        os << (args->load ? "ld" : "st") << 'r' << da_cond_map[args->cond] << (args->byte ? "b" : "")
            << ((!args->p && args->w) ? "t" : "") << "\tr" << args->rd << ", [r" << args->rn;

        if (!args->p) os << ']';

        if (args->off != 0) {
                os << ", #" << (args->off < 0 ? "-" : "") << "0x" << hex << abs(args->off) << dec;
        }

        if (args->p) os << ']' << (args->w ? "!" : "");

        if (args->rn == DA_REG_R15) {
                os << "\t; 0x" << hex << (addr + 8 + args->off) << dec;
        }
}

static void
da_instr_ostream_ls_multi(ostream& os, const da_instr_t *instr,
                         const da_args_ls_multi_t *args, da_addr_t addr)
{
        os << (args->load ? "ld" : "st") << 'm' << da_cond_map[args->cond]
           << (args->u ? 'i' : 'd') << (args->p ? 'b' : 'a') << "\tr"
           << args->rn << (args->write ? "!" : "") << ", {";

        da_reglist_ostream(os, args->reglist);

        os << " }" << (args->s ? "^" : "");
}

static void
da_instr_ostream_ls_reg(ostream& os, const da_instr_t *instr,
                       const da_args_ls_reg_t *args, da_addr_t addr)
{
        os << (args->load ? "ld" : "st") << 'r' << da_cond_map[args->cond] << (args->byte ? "b" : "")
            << ((!args->p && args->write) ? "t" : "") << "\tr" << args->rd << ", [r" << args->rn;

        if (!args->p) os << ']';

        os << ", " << (args->sign ? "" : "-") << 'r' << args->rm;

        da_uint_t sha = args->sha;

        if (args->sh == DA_SHIFT_LSR || args->sh == DA_SHIFT_ASR) {
                sha = (sha ? sha : 32);
        }

        if (sha > 0) {
                os << ", " << da_shift_map[args->sh] << " #0x" << hex << sha << dec;
        } else if (args->sh == DA_SHIFT_ROR) {
                os << ", rrx";
        }

        if (args->p) os << ']' << (args->write ? "!" : "");
}

static void
da_instr_ostream_ls_two_imm(ostream& os, const da_instr_t *instr,
                           const da_args_ls_two_imm_t *args, da_addr_t addr)
{
        os << (args->store ? "st" : "ld") << 'r' << da_cond_map[args->cond] << "d\tr" << args->rd << ", [r" << args->rn;

        if (!args->p) os << ']';

        if (args->off != 0) {
                os << ", #" << (args->off < 0 ? "-" : "") << "0x" << hex << (abs(args->off)) << dec;
        }

        if (args->p) os << ']' << (args->write ? "!" : "");

        if (args->rn == DA_REG_R15) {
                os << "\t; 0x" << hex << (addr + 8 + args->off) << dec;
        }
}

static void
da_instr_ostream_ls_two_reg(ostream& os, const da_instr_t *instr,
                           const da_args_ls_two_reg_t *args, da_addr_t addr)
{
        os << (args->store ? "st" : "ld") << 'r' << da_cond_map[args->cond] << "d\tr" << args->rd << ", [r" << args->rn;

        if (!args->p) os << ']';

        os << ", " << (args->sign ? "" : "-") << 'r' << args->rm;

        if (args->p) os << ']' << (args->write ? "!" : "");
}

static void
da_instr_ostream_mrs(ostream& os, const da_instr_t *instr,
                    const da_args_mrs_t *args, da_addr_t addr)
{
        os << "mrs" << da_cond_map[args->cond] << "\tr" << args->rd << ", " << (args->r ? "SPSR" : "CPSR");
}

static void
da_instr_ostream_msr(ostream& os, const da_instr_t *instr,
                    const da_args_msr_t *args, da_addr_t addr)
{
        os << "msr" << da_cond_map[args->cond] << '\t' << (args->r ? "SPSR" : "CPSR") << "_"
            << ((args->mask & 1) ? "c" : "") << ((args->mask & 2) ? "x" : "")
            << ((args->mask & 4) ? "s" : "") << ((args->mask & 8) ? "f" : "");

        os << ", r" << args->rm;
}

static void
da_instr_ostream_msr_imm(ostream& os, const da_instr_t *instr,
                        const da_args_msr_imm_t *args, da_addr_t addr)
{
        os << "msr" << da_cond_map[args->cond] << '\t' << (args->r ? "SPSR" : "CPSR") << "_" << ((args->mask & 1) ? "c" : "")
            << ((args->mask & 2) ? "x" : "") << ((args->mask & 4) ? "s" : "") << ((args->mask & 8) ? "f" : "")
            << ", #0x" << hex << args->imm << dec;
}

static void
da_instr_ostream_mul(ostream& os, const da_instr_t *instr,
                    const da_args_mul_t *args, da_addr_t addr)
{
        os << 'm' << (args->acc ? "la" : "ul") << da_cond_map[args->cond] << (args->flags ? "s" : "") << "\tr"
            << args->rd << ", r" << args->rm << ", r" << args->rs;

        if (args->acc) os << ", r" << args->rn;
}

static void
da_instr_ostream_mull(ostream& os, const da_instr_t *instr,
                     const da_args_mull_t *args, da_addr_t addr)
{
        os << (args->sign ? 's' : 'u') << 'm' << (args->acc ? "la" : "ul") << "l"
            << da_cond_map[args->cond] << (args->flags ? "s" : "") << "\tr"
            << args->rd_lo << ", r" << args->rd_hi << ", r" << args->rm << ", r" << args->rs;
}

static void
da_instr_ostream_swi(ostream& os, const da_instr_t *instr,
                    const da_args_swi_t *args, da_addr_t addr)
{
        os << "swi" << da_cond_map[args->cond] << "\t0x" << hex << args->imm << dec;
}

static void
da_instr_ostream_swp(ostream& os, const da_instr_t *instr,
                    const da_args_swp_t *args, da_addr_t addr)
{
        os << "swp" << da_cond_map[args->cond] << (args->byte ? "b" : "") << "\tr" << args->rd << ", r" << args->rm << ", [r" << args->rn << ']';
}

DA_API void
da_instr_ostream(ostream& os, const da_instr_t *instr, const da_instr_args_t *args,
                da_addr_t addr)
{
        switch (instr->group) {
        case DA_GROUP_BKPT:
                da_instr_ostream_bkpt(os, instr, &args->bkpt, addr);
                break;
        case DA_GROUP_BL:
                da_instr_ostream_bl(os, instr, &args->bl, addr);
                break;
        case DA_GROUP_BLX_IMM:
                da_instr_ostream_blx_imm(os, instr, &args->blx_imm, addr);
                break;
        case DA_GROUP_BLX_REG:
                da_instr_ostream_blx_reg(os, instr, &args->blx_reg, addr);
                break;
        case DA_GROUP_CLZ:
                da_instr_ostream_clz(os, instr, &args->clz, addr);
                break;
        case DA_GROUP_CP_DATA:
                da_instr_ostream_cp_data(os, instr, &args->cp_data, addr);
                break;
        case DA_GROUP_CP_LS:
                da_instr_ostream_cp_ls(os, instr, &args->cp_ls, addr);
                break;
        case DA_GROUP_CP_REG:
                da_instr_ostream_cp_reg(os, instr, &args->cp_reg, addr);
                break;
        case DA_GROUP_DATA_IMM:
                da_instr_ostream_data_imm(os, instr, &args->data_imm, addr);
                break;
        case DA_GROUP_DATA_IMM_SH:
                da_instr_ostream_data_imm_sh(os, instr, &args->data_imm_sh,
                                            addr);
                break;
        case DA_GROUP_DATA_REG_SH:
                da_instr_ostream_data_reg_sh(os, instr, &args->data_reg_sh,
                                            addr);
                break;
        case DA_GROUP_DSP_ADD_SUB:
                da_instr_ostream_dsp_add_sub(os, instr, &args->dsp_add_sub,
                                            addr);
                break;
        case DA_GROUP_DSP_MUL:
                da_instr_ostream_dsp_mul(os, instr, &args->dsp_mul, addr);
                break;
        case DA_GROUP_L_SIGN_IMM:
                da_instr_ostream_l_sign_imm(os, instr, &args->l_sign_imm, addr);
                break;
        case DA_GROUP_L_SIGN_REG:
                da_instr_ostream_l_sign_reg(os, instr, &args->l_sign_reg, addr);
                break;
        case DA_GROUP_LS_HW_IMM:
                da_instr_ostream_ls_hw_imm(os, instr, &args->ls_hw_imm, addr);
                break;
        case DA_GROUP_LS_HW_REG:
                da_instr_ostream_ls_hw_reg(os, instr, &args->ls_hw_reg, addr);
                break;
        case DA_GROUP_LS_IMM:
                da_instr_ostream_ls_imm(os, instr, &args->ls_imm, addr);
                break;
        case DA_GROUP_LS_MULTI:
                da_instr_ostream_ls_multi(os, instr, &args->ls_multi, addr);
                break;
        case DA_GROUP_LS_REG:
                da_instr_ostream_ls_reg(os, instr, &args->ls_reg, addr);
                break;
        case DA_GROUP_LS_TWO_IMM:
                da_instr_ostream_ls_two_imm(os, instr, &args->ls_two_imm, addr);
                break;
        case DA_GROUP_LS_TWO_REG:
                da_instr_ostream_ls_two_reg(os, instr, &args->ls_two_reg, addr);
                break;
        case DA_GROUP_MRS:
                da_instr_ostream_mrs(os, instr, &args->mrs, addr);
                break;
        case DA_GROUP_MSR:
                da_instr_ostream_msr(os, instr, &args->msr, addr);
                break;
        case DA_GROUP_MSR_IMM:
                da_instr_ostream_msr_imm(os, instr, &args->msr_imm, addr);
                break;
        case DA_GROUP_MUL:
                da_instr_ostream_mul(os, instr, &args->mul, addr);
                break;
        case DA_GROUP_MULL:
                da_instr_ostream_mull(os, instr, &args->mull, addr);
                break;
        case DA_GROUP_SWI:
                da_instr_ostream_swi(os, instr, &args->swi, addr);
                break;
        case DA_GROUP_SWP:
                da_instr_ostream_swp(os, instr, &args->swp, addr);
                break;
        case DA_GROUP_UNDEF_1:
        case DA_GROUP_UNDEF_2:
        case DA_GROUP_UNDEF_3:
        case DA_GROUP_UNDEF_4:
        case DA_GROUP_UNDEF_5:
                os << "undefined";
                break;
        default:
          os << "UNDEFINED_ERROR";
        }
}


