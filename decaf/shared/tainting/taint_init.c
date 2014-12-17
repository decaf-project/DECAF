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
#include "shared/tainting/taint_init.h"



char *memoryTaint = 0;
/* Variables to keep disassembler state */
xed_state_t dstate;

EntryHeader eh;


/* Trace statistics */
struct trace_stats tstats = {0};

/* Store eflags register index */
int eflags_idx = -1;

/* This flags we might want to put as part of the EntryHeader without
 * writing to file */
int has_page_fault = 0;
int access_user_mem = 0;

xed_decoded_inst_t xedd;

int taintInit(unsigned long ram_size)
{
	memoryTaint = (char*)malloc(sizeof(char)* (ram_size));

	if(memoryTaint == 0)
		return ENOMEM;
	monitor_printf(default_mon,"taint_init: Allocated %dM for `memoryTaint`\n", (sizeof(char)* (ram_size) / (1024*1024)));

	xed2_init();

	monitor_printf(default_mon, "\n\nNAIVE TAINT IMPLEMENTATION!\n\n");
	return 0;
}

/* XED2 initialization */
void xed2_init() {

  xed_tables_init ();
  xed_state_zero (&dstate);

  xed_state_init(&dstate,
    XED_MACHINE_MODE_LEGACY_32,
    XED_ADDRESS_WIDTH_32b,
    XED_ADDRESS_WIDTH_32b);
  monitor_printf(default_mon,"taint_init: XED2 initialization complete!\n");

}

/* Given an operand, check taint information and store it */
inline void set_operand_taint(OperandVal *op)
{
#if TAINT_ENABLED
  taint_record_t taintrec[MAX_OPERAND_LEN];
  switch (op->type) {
    case TRegister: {
      int regnum = REGNUM(op->addr.reg_addr);
      int offset = getOperandOffset(op);
      op->tainted =
        (uint16_t) taintcheck_register_check(regnum, offset, op->length,
          (uint8_t *)taintrec);
      break;
    }
    case TMemLoc: {
      op->tainted = taintcheck_check_virtmem(op->addr.mem32_addr, op->length, taintrec);
      break;
    }
    default:
      op->tainted = 0;
      break;
  }

  if (op->tainted) {
    record_taint_value(op, taintrec);
  }
#else
  op->tainted = 0;
#endif
}

void decode_address(uint32_t address, EntryHeader *eh, int ignore_taint)
{
  unsigned char insn_buf[MAX_INSN_BYTES];
  unsigned int is_stackpush = 0, is_stackpop = 0, is_x87push = 0;
  unsigned int fp_idx = 0;
  unsigned int stackpushpop_acc = 0;
  unsigned int add_fpus = 0;
  //unsigned int add_fputags = 0;
  int i;
  int op_idx = -1;
  int regnum = 0;

  /* Read memory from TEMU */
  TEMU_read_mem(address, MAX_INSN_BYTES, insn_buf);

  /* Disassemble instruction buffer */
  xed_decoded_inst_zero_set_mode(&xedd, &dstate);
  xed_error_enum_t xed_error =
    xed_decode(&xedd, XED_STATIC_CAST(const xed_uint8_t*,insn_buf),MAX_INSN_BYTES);
  xed_bool_t okay = (xed_error == XED_ERROR_NONE);
  if (!okay) return;

  // Increase counters
  tstats.insn_counter_decoded++;

  /* Clear out Entry header
     This should not be needed if everything is initialized
     So, commenting it out for performance reasons
  */
  //memset(eh, 0, sizeof(EntryHeader));

  /* Copy the address and instruction size */
  eh->address = address;
  uint16_t inst_size = xed_decoded_inst_get_length(&xedd);
  eh->inst_size = (inst_size <= MAX_INSN_BYTES) ? inst_size : MAX_INSN_BYTES;

  /* Set process identifier */
  //if (tracepid != -1) {
  //  eh->pid = tracepid;
  //}
  //else {
  //  eh->pid = find_pid(TEMU_cpu_cr[3]);
  //}

  /* Set thread identifier */
  //eh->tid = current_tid;

  /* Copy instruction rawbytes */
  memcpy(eh->rawbytes, insn_buf, eh->inst_size);

  /* Get the number of XED operands */
  const xed_inst_t* xi = xed_decoded_inst_inst(&xedd);
  int xed_ops = xed_inst_noperands(xi);

  /* Initialize eflags index */
  eflags_idx = -1;

  /* Initialize remaining fields in EntryHeader */
  eh->num_operands = 0;
  eh->tp = TP_NONE; /* Gets updated at tracing_taint_propagate */
  eh->eflags = 0;   /* Gets updated at insn_end */
  eh->df = 0;       /* Gets updated at insn_end */
  eh->cc_op = *TEMU_cc_op;

  /* Get the category of the instruction */
  xed_category_enum_t category = xed_decoded_inst_get_category(&xedd);

  /* Iterate over the XED operands */
  for(i = 0; i < xed_ops; i++) {
  	if(op_idx >= MAX_NUM_OPERANDS)
  	  break;
    //assert(op_idx < MAX_NUM_OPERANDS);

    /* Get operand */
    const xed_operand_t* op = xed_inst_operand(xi,i);
    xed_operand_enum_t op_name = xed_operand_name(op);

    switch(op_name) {
      /* Register */
      case XED_OPERAND_REG0:
      case XED_OPERAND_REG1:
      case XED_OPERAND_REG2:
      case XED_OPERAND_REG3:
      case XED_OPERAND_REG4:
      case XED_OPERAND_REG5:
      case XED_OPERAND_REG6:
      case XED_OPERAND_REG7:
      case XED_OPERAND_REG8:
      case XED_OPERAND_REG9:
      case XED_OPERAND_REG10:
      case XED_OPERAND_REG11:
      case XED_OPERAND_REG12:
      case XED_OPERAND_REG13:
      case XED_OPERAND_REG14:
      case XED_OPERAND_REG15: {
        xed_reg_enum_t xed_regid = xed_decoded_inst_get_reg(&xedd, op_name);
        regnum = xed2chris_regmapping[xed_regid][1];

        // Special handling for Push/Pop
        if (xed_regid == XED_REG_STACKPUSH) is_stackpush = 1;
        else if (xed_regid == XED_REG_STACKPOP) is_stackpop = 1;
        else if (xed_regid == XED_REG_X87PUSH) is_x87push = 1;

        if (-1 == regnum)
          break;
        else {
          op_idx++;
          eh->num_operands++;
          eh->operand[op_idx].addr.reg_addr =
            xed2chris_regmapping[xed_regid][0];
          eh->operand[op_idx].length =
            xed2chris_regmapping[xed_regid][2];
            //(uint8_t) xed_decoded_inst_operand_length (&xedd, i);
          eh->operand[op_idx].access = (uint8_t) xed_operand_rw (op);
          switch (eh->operand[op_idx].addr.reg_addr) {
            /* 32-bit general purpose registers */
            case eax_reg:
            case ebx_reg:
            case ecx_reg:
            case edx_reg:
            case ebp_reg:
            case esp_reg:
            case esi_reg:
            case edi_reg:
              eh->operand[op_idx].type = TRegister;
              eh->operand[op_idx].usage = unknown;
              eh->operand[op_idx].value.val32 = TEMU_cpu_regs[regnum];
              break;
            /* 16-bit general purpose registers */
            case ax_reg:
            case bx_reg:
            case cx_reg:
            case dx_reg:
            case bp_reg:
            case sp_reg:
            case si_reg:
            case di_reg:
              eh->operand[op_idx].type = TRegister;
              eh->operand[op_idx].usage = unknown;
              eh->operand[op_idx].value.val32 = TEMU_cpu_regs[regnum];
              eh->operand[op_idx].value.val32 &= 0xFFFF;
              break;
            /* 8-bit general purpose registers */
            case al_reg:
            case bl_reg:
            case cl_reg:
            case dl_reg:
              eh->operand[op_idx].type = TRegister;
              eh->operand[op_idx].usage = unknown;
              eh->operand[op_idx].value.val32 = TEMU_cpu_regs[regnum];
              eh->operand[op_idx].value.val32 &= 0xFF;
              break;
            case ah_reg:
            case bh_reg:
            case ch_reg:
            case dh_reg:
              eh->operand[op_idx].type = TRegister;
              eh->operand[op_idx].usage = unknown;
              eh->operand[op_idx].value.val32 = TEMU_cpu_regs[regnum];
              eh->operand[op_idx].value.val32 =
                (eh->operand[i].value.val32 & 0xFF00) >> 8;
              break;
            /* Segment registers */
            case cs_reg:
            case ds_reg:
            case es_reg:
            case ss_reg:
            case fs_reg:
            case gs_reg:
              eh->operand[op_idx].type = TRegister;
              eh->operand[op_idx].usage = unknown;
              eh->operand[op_idx].value.val32 = TEMU_cpu_segs[regnum].selector;
              eh->operand[op_idx].value.val32 &= 0xFFFF;
              break;
            /* MMX registers */
            case mmx0_reg:
            case mmx1_reg:
            case mmx2_reg:
            case mmx3_reg:
            case mmx4_reg:
            case mmx5_reg:
            case mmx6_reg:
            case mmx7_reg:
              eh->operand[op_idx].type = TMMXRegister;
              eh->operand[op_idx].usage = unknown;
              eh->operand[op_idx].value.val64 = TEMU_cpu_fp_regs[regnum].mmx.q;
              break;
            case xmm0_reg:
            case xmm1_reg:
            case xmm2_reg:
            case xmm3_reg:
            case xmm4_reg:
            case xmm5_reg:
            case xmm6_reg:
            case xmm7_reg:
            case xmm8_reg:
            case xmm9_reg:
            case xmm10_reg:
            case xmm12_reg:
            case xmm13_reg:
            case xmm14_reg:
            case xmm15_reg:
              eh->operand[op_idx].type = TXMMRegister;
              eh->operand[op_idx].usage = unknown;
              eh->operand[op_idx].value.xmm_val._q[0] =
                TEMU_cpu_xmm_regs[regnum]._q[0];
              eh->operand[op_idx].value.xmm_val._q[1] =
                TEMU_cpu_xmm_regs[regnum]._q[1];
              break;
            /* Float data registers */
            case fpu_st0_reg:
            case fpu_st1_reg:
            case fpu_st2_reg:
            case fpu_st3_reg:
            case fpu_st4_reg:
            case fpu_st5_reg:
            case fpu_st6_reg:
            case fpu_st7_reg:
              add_fpus = 1;
              //add_fputags = 1;
              fp_idx = (*TEMU_cpu_fp_stt + regnum) & 7;
              eh->operand[op_idx].type = TFloatRegister;
              eh->operand[op_idx].usage = unknown;
              eh->operand[op_idx].value.float_val = TEMU_cpu_fp_regs[fp_idx].d;
              // Operand address encodes both stack and hardware indices
              eh->operand[op_idx].addr.reg_addr = regnum | (fp_idx << 4);
              break;
            /* Float control registers */
            case fpu_status_reg:
              /*
                XED does not include the FPU status word as an operand
                  if it is only read by the instruction.
                  So this case is only for instructions that write it,
                  e.g., fucompp
                We add it as operand, if needed, after processing the operands
              */
              add_fpus = 0;
              //add_fputags = 1;
              eh->operand[op_idx].type = TFloatControlRegister;
              eh->operand[op_idx].usage = unknown;
              eh->operand[op_idx].value.val32 =
                ((uint32_t) *TEMU_cpu_fpus & 0xC7FF) |
                (((uint32_t) *TEMU_cpu_fp_stt & 0x7) << 11);
              // The FPU status word is always read, so fix the access
              eh->operand[op_idx].access = XED_OPERAND_ACTION_RW;
              // The following should not be needed. Check if bug in XED
              eh->operand[op_idx].length = 2;
              break;
            case fpu_control_reg:
              add_fpus = 1;
              //add_fputags = 1;
              eh->operand[op_idx].type = TFloatControlRegister;
              eh->operand[op_idx].usage = unknown;
              eh->operand[op_idx].value.val32 = (uint32_t) *TEMU_cpu_fpuc;
              // The following should not be needed. Check if bug in XED
              eh->operand[op_idx].length = 2;
              break;
            /* EFLAGS register */
            case eflags_reg:
              eflags_idx = op_idx;
              eh->operand[op_idx].type = TRegister;
              eh->operand[op_idx].usage = eflags;
              eh->operand[op_idx].value.val32 = 0;
              break;
            /* Default case: ignore register */
            default:
              break;
          }
        }
        if (ignore_taint == 0) {
          set_operand_taint(&(eh->operand[op_idx]));
        }
        else {
          eh->operand[op_idx].tainted = 0;
        }
        break;
      }

      /* Immediate */
      case XED_OPERAND_IMM0: {
        op_idx++;
        eh->num_operands++;
        eh->operand[op_idx].type = TImmediate;
        eh->operand[op_idx].usage = unknown;
        eh->operand[op_idx].addr.reg_addr = 0;
        eh->operand[op_idx].tainted = 0;
        eh->operand[op_idx].length =
          (uint8_t) xed_decoded_inst_operand_length (&xedd, i);
        eh->operand[op_idx].access = (uint8_t) xed_operand_rw (op);
        //xed_uint_t width = xed_decoded_inst_get_immediate_width(&xedd);
        if (xed_decoded_inst_get_immediate_is_signed(&xedd)) {
          xed_int32_t signed_imm_val =
            xed_decoded_inst_get_signed_immediate(&xedd);
          eh->operand[op_idx].value.val32 = (uint32_t) signed_imm_val;
        }
        else {
          xed_uint64_t unsigned_imm_val =
            xed_decoded_inst_get_unsigned_immediate(&xedd);
          eh->operand[op_idx].value.val32 = (uint32_t) unsigned_imm_val;
        }
        break;
      }

      /* Special immediate only used in ENTER instruction */
      case XED_OPERAND_IMM1: {
        op_idx++;
        eh->num_operands++;
        eh->operand[op_idx].type = TImmediate;
        eh->operand[op_idx].usage = unknown;
        eh->operand[op_idx].addr.reg_addr = 0;
        eh->operand[op_idx].tainted = 0;
        eh->operand[op_idx].length =
          (uint8_t) xed_decoded_inst_operand_length (&xedd, i);
        eh->operand[op_idx].access = (uint8_t) xed_operand_rw (op);
        xed_uint8_t unsigned_imm_val =
          xed_decoded_inst_get_second_immediate(&xedd);
        eh->operand[op_idx].value.val32 = (uint32_t) unsigned_imm_val;
        break;
      }


      /* Memory */
      case XED_OPERAND_AGEN:
      case XED_OPERAND_MEM0:
      case XED_OPERAND_MEM1: {
        unsigned long base = 0;
        unsigned long index = 0;
        unsigned long scale = 1;
        unsigned long segbase = 0;
        unsigned short segsel = 0;
        unsigned long displacement = 0;
        unsigned int j;
        size_t remaining = 0;

        /* Set memory index */
        int mem_idx = 0;
        if (op_name == XED_OPERAND_MEM1) mem_idx = 1;

        unsigned int memlen = xed_decoded_inst_operand_length (&xedd, i);

        for (j = 0; j < memlen; j+=4) {
          /* Initialization */
          base = 0;
          index = 0;
          scale = 1;
          segbase = 0;
          segsel = 0;
          displacement = 0;
          remaining = memlen - j;

          op_idx++;
          if(op_idx >= MAX_NUM_OPERANDS)
            break;
          //assert(op_idx < MAX_NUM_OPERANDS);
          eh->num_operands++;
          eh->operand[op_idx].type = TMemLoc;
          eh->operand[op_idx].usage = unknown;
          eh->operand[op_idx].access = (uint8_t) xed_operand_rw (op);
          eh->operand[op_idx].length =
            remaining > 4 ? 4 : (uint8_t) remaining;

          // Get Segment register
          xed_reg_enum_t seg_regid =
            xed_decoded_inst_get_seg_reg(&xedd,mem_idx);

          if (seg_regid != XED_REG_INVALID) {
            const xed_operand_values_t *xopv =
              xed_decoded_inst_operands_const(&xedd);
            xed_bool_t default_segment =
              xed_operand_values_using_default_segment (xopv,mem_idx);

            if (!default_segment) {
              eh->num_operands++;
              int segmentreg = xed2chris_regmapping[seg_regid][0] - 100;

              segbase = TEMU_cpu_segs[segmentreg].base;
              segsel = TEMU_cpu_segs[segmentreg].selector;

              eh->memregs[op_idx][0].type = TRegister;
              eh->memregs[op_idx][0].usage = memsegment;
              eh->memregs[op_idx][0].length = 2;
              eh->memregs[op_idx][0].addr.reg_addr =
                xed2chris_regmapping[seg_regid][0];
              eh->memregs[op_idx][0].access = (uint8_t) XED_OPERAND_ACTION_R;
              eh->memregs[op_idx][0].value.val32 = segsel;
              if (ignore_taint == 0) {
                set_operand_taint(&(eh->memregs[op_idx][0]));
              }
              else {
                eh->memregs[op_idx][0].tainted = 0;
              }

              int dt;
              if (segsel & 0x4)       // ldt
                dt = TEMU_cpu_ldt->base;
              else                    //gdt
                dt = TEMU_cpu_gdt->base;
              segsel = segsel >> 3;

              unsigned long segent = dt + 8 * segsel;
              unsigned char segdes[8];
              TEMU_read_mem(segent, 8, segdes);

#if 0
              // debugging code to double check segbase value
              unsigned long segbasenew = segdes[2] + segdes[3] * 256 +
              segdes[4] * 256 * 256 + segdes[7] * 256 * 256 * 256;
              if (segbase != segbasenew) {
                monitor_printf(default_mon, "segbase unexpected: 0x%08lX v.s 0x%08lX\n",
                  segbase, segbasenew);
              }
#endif
              /* Segment descriptor is stored as a memory operand */
              eh->num_operands+=2;
              eh->memregs[op_idx][3].type = TMemLoc;
              eh->memregs[op_idx][3].usage = memsegent0;
              eh->memregs[op_idx][3].length = 4;
              eh->memregs[op_idx][3].addr.mem32_addr = segent;
              eh->memregs[op_idx][3].access =
                (uint8_t) XED_OPERAND_ACTION_R;
              eh->memregs[op_idx][3].value.val32 = *(uint32_t *) segdes;
              eh->memregs[op_idx][3].tainted = 0;

              eh->memregs[op_idx][4].type = TMemLoc;
              eh->memregs[op_idx][4].usage = memsegent1;
              eh->memregs[op_idx][4].length = 4;
              eh->memregs[op_idx][4].addr.mem32_addr = segent + 4;
              eh->memregs[op_idx][4].access =
                (uint8_t) XED_OPERAND_ACTION_R;
              eh->memregs[op_idx][4].value.val32 = *(uint32_t *) (segdes + 4);
              eh->memregs[op_idx][4].tainted = 0;
            }
            else {
              eh->memregs[op_idx][0].type = TNone;
              eh->memregs[op_idx][3].type = TNone;
              eh->memregs[op_idx][4].type = TNone;
            }
          }
          else {
            eh->memregs[op_idx][0].type = TNone;
            eh->memregs[op_idx][3].type = TNone;
            eh->memregs[op_idx][4].type = TNone;
          }

          // Get Base register
          xed_reg_enum_t base_regid =
            xed_decoded_inst_get_base_reg(&xedd,mem_idx);
          if (base_regid != XED_REG_INVALID) {
            eh->num_operands++;
            int basereg = xed2chris_regmapping[base_regid][1];
            base = TEMU_cpu_regs[basereg];
            eh->memregs[op_idx][1].type = TRegister;
            eh->memregs[op_idx][1].usage = membase;
            eh->memregs[op_idx][1].addr.reg_addr =
              xed2chris_regmapping[base_regid][0];
            eh->memregs[op_idx][1].length =
              xed2chris_regmapping[base_regid][2];
            eh->memregs[op_idx][1].access = (uint8_t) XED_OPERAND_ACTION_R;
            eh->memregs[op_idx][1].value.val32 = base;
            if (ignore_taint == 0) {
              set_operand_taint(&(eh->memregs[op_idx][1]));
            }
            else {
              eh->memregs[op_idx][1].tainted = 0;
            }
          }
          else {
            eh->memregs[op_idx][1].type = TNone;
          }

          // Get Index register and Scale
          xed_reg_enum_t index_regid =
            xed_decoded_inst_get_index_reg(&xedd,mem_idx);
          if (mem_idx == 0 && index_regid != XED_REG_INVALID) {
            eh->num_operands++;
            int indexreg = xed2chris_regmapping[index_regid][1];
            index = TEMU_cpu_regs[indexreg];
            eh->memregs[op_idx][2].type = TRegister;
            eh->memregs[op_idx][2].usage = memindex;
            eh->memregs[op_idx][2].addr.reg_addr =
              xed2chris_regmapping[index_regid][0];
            eh->memregs[op_idx][2].length =
              xed2chris_regmapping[index_regid][2];
            eh->memregs[op_idx][2].access = (uint8_t) XED_OPERAND_ACTION_R;
            eh->memregs[op_idx][2].value.val32 = index;
            if (ignore_taint == 0) {
              set_operand_taint(&(eh->memregs[op_idx][2]));
            }
            else {
              eh->memregs[op_idx][2].tainted = 0;
            }

            // Get Scale (AKA width) (only have a scale if the index exists)
            if (xed_decoded_inst_get_scale(&xedd,i) != 0) {
              scale =
                (unsigned long) xed_decoded_inst_get_scale(&xedd,mem_idx);
              eh->num_operands++;
              eh->memregs[op_idx][6].type = TImmediate;
              eh->memregs[op_idx][6].usage = memscale;
                eh->memregs[op_idx][6].addr.reg_addr = 0;
              eh->memregs[op_idx][6].length = 1;
              eh->memregs[op_idx][6].access = (uint8_t) XED_OPERAND_ACTION_R;
              eh->memregs[op_idx][6].value.val32 = scale;
              eh->memregs[op_idx][6].tainted = 0;
            }
          }
          else {
            eh->memregs[op_idx][2].type = TNone;
            eh->memregs[op_idx][6].type = TNone;
          }

          // Get displacement (AKA offset)
          displacement =
            (unsigned long) xed_decoded_inst_get_memory_displacement
            (&xedd,mem_idx);
          if (displacement > 0) {
            eh->num_operands++;
            eh->memregs[op_idx][5].type = TDisplacement;
            eh->memregs[op_idx][5].usage = memdisplacement;
            eh->memregs[op_idx][5].addr.reg_addr = 0;
            eh->memregs[op_idx][5].length =
              xed_decoded_inst_get_memory_displacement_width(&xedd,mem_idx);
            eh->memregs[op_idx][5].access = (uint8_t) XED_OPERAND_ACTION_R;
            eh->memregs[op_idx][5].value.val32 = displacement;
            eh->memregs[op_idx][5].tainted = 0;
          }
          else {
            eh->memregs[op_idx][5].type = TNone;
          }

          // Fix displacement for:
          //   1) Any instruction that pushes into the stack, since ESP is
          //        decremented before memory operand is written using ESP.
          //        Affects: ENTER,PUSH,PUSHA,PUSHF,CALL
          if (is_stackpush) {
            stackpushpop_acc += eh->operand[op_idx].length;
            displacement = displacement - stackpushpop_acc -j;
          }
          //   2) Pop instructions where the
          //      destination operand is a memory location that uses ESP
          //        as base or index register.
          //      The pop operations increments ESP and the written memory
          //        location address needs to be adjusted.
          //      Affects: pop (%esp)
          else if ((category == XED_CATEGORY_POP) && (!is_stackpop)) {
            if ((eh->memregs[op_idx][1].addr.reg_addr == esp_reg) ||
                (eh->memregs[op_idx][2].addr.reg_addr == esp_reg))
            {
              displacement = displacement + eh->operand[op_idx].length;
            }
          }

          // Calculate memory address accessed
          eh->operand[op_idx].addr.mem32_addr =
            j + segbase + base + index * scale + displacement;

          // Special handling for LEA instructions
          eh->operand[op_idx].value.val32 = 0;
          if (op_name == XED_OPERAND_AGEN) {
            eh->operand[op_idx].type = TMemAddress;
            eh->operand[op_idx].length = 4;
            has_page_fault = 0; // LEA won't trigger page fault
          }
          else {
            has_page_fault = TEMU_read_mem(eh->operand[op_idx].addr.mem32_addr,
              (int)(eh->operand[op_idx].length),
              (uint8_t *)&(eh->operand[op_idx].value.val32));
          }

          // Check if instruction accesses user memory
          // kernel_mem_start defined in shared/read_linux.c
          if ((eh->operand[op_idx].addr.mem32_addr < kernel_mem_start) &&
            (op_name != XED_OPERAND_AGEN))
          {
            access_user_mem = 1;
          }
          if (ignore_taint == 0) {
            set_operand_taint(&(eh->operand[op_idx]));
          }
          else {
            eh->operand[op_idx].tainted = 0;
          }
        }
        break;
      }

      /* Jumps */
      case XED_OPERAND_PTR:  // pointer (always in conjunction with a IMM0)
      case XED_OPERAND_RELBR: { // branch displacements
        xed_uint_t disp = xed_decoded_inst_get_branch_displacement(&xedd);
        /* Displacement is from instruction end */
        /* Adjust displacement with instruction size */
        disp = disp + eh->inst_size;
        op_idx++;
        eh->num_operands++;
        eh->operand[op_idx].type = TJump;
        eh->operand[op_idx].usage = unknown;
        eh->operand[op_idx].addr.reg_addr = 0;
        eh->operand[op_idx].length = 4;
        eh->operand[op_idx].access = (uint8_t) xed_operand_rw (op);
        eh->operand[op_idx].value.val32 = disp;
        eh->operand[op_idx].tainted = 0;
        break;
      }

      /* Extra checks for floating point control registers */
      case XED_REG_X87CONTROL:
        monitor_printf(default_mon, "Found unexpected FPU control word. Exiting\n");
        exit(-1);
        break;
      case XED_REG_X87STATUS:
        monitor_printf(default_mon, "Found unexpected FPU status word. Exiting\n");
        exit(-1);
        break;
      case XED_REG_X87TAG:
        monitor_printf(default_mon, "Found unexpected FPU tag word. Exiting\n");
        exit(-1);
        break;
      case XED_REG_X87TOP:
        monitor_printf(default_mon, "Found unexpected FPU top of stack. Exiting\n");
        exit(-1);
        break;

      default:
        break;
    }
  }

  /* XED does not add the FPU status word as an operand if it is only read
      Thus, we add it ourselves if needed */
  if (add_fpus) {
    op_idx++;
    eh->num_operands++;
    eh->operand[op_idx].type = TFloatControlRegister;
    eh->operand[op_idx].usage = unknown;
    eh->operand[op_idx].addr.reg_addr = fpu_status_reg;
    eh->operand[op_idx].length = 2;
    eh->operand[op_idx].access = XED_OPERAND_ACTION_R;
    eh->operand[op_idx].value.val32 =
      ((uint32_t) *TEMU_cpu_fpus & 0xC7FF) |
      (((uint32_t) *TEMU_cpu_fp_stt & 0x7) << 11);
    eh->operand[op_idx].tainted = 0;
  }

  /* XED does not add the FPU tag word as an operand
      Thus, we add it ourselves if needed */
  /*
  if (add_fputags) {
    op_idx++;
      eh->num_operands++;
      eh->operand[op_idx].type = TFloatControlRegister;
      eh->operand[op_idx].usage = unknown;
      eh->operand[op_idx].addr.reg_addr = fpu_tag_reg;
      eh->operand[op_idx].length = 2;
      eh->operand[op_idx].access = XED_OPERAND_ACTION_RW;
      eh->operand[op_idx].value.val32 = get_fpu_tag_word();
      eh->operand[op_idx].tainted = 0;
    }
  */

  /* For instructions that push into the float stack,
    need to adjust the hardware index for the destination float register */
  if (is_x87push) {
    for (i = 0; i <= op_idx; i++) {
      if (eh->operand[i].type == TFloatRegister) {
        fp_idx = eh->operand[i].addr.reg_addr >> 4;
        regnum = eh->operand[i].addr.reg_addr & 0xF;
        eh->operand[i].addr.reg_addr = regnum | (((fp_idx - 1) & 7) << 4);
        break;
      }
    }
  }

  /* Increment the operand counter without including ESP */
  tstats.operand_counter += eh->num_operands;

  /* Make sure we mark the end of the list of valid operands */
  op_idx++;
  eh->operand[op_idx].type = TNone;
}

void debug_taint_set_flag(int flag)
{
	taintPrint_flag ++;
	taintPrint_flag = taintPrint_flag%2;
}

void taint_addr(int addr)
{
	int i=0;
	if (addr==0)
		eaxTaint = 1;

	else if (addr!=-1)
		memoryTaint[addr]=1;

	//for(i=0;i<ram_size;i++)
		//if(memoryTaint[i]&&i!=0x)
			//monitor_printf(default_mon,"{0x%08x}:%d\n",i,memoryTaint[i]);

}

void MOV_Ev_Gv(void)
{
	//monitor_printf(default_mon, "MOV_Ev_Gv called\n\n\n");
}

void MOV_Eb_Gb(uint32_t src,uint32_t dst)
{
	//monitor_printf(default_mon, "MOV_Eb_Gb called\n(0x%08x),(0x%08x)\n\n",src,dst);

	//EK: Taint_Policy BEGIN
	//memoryTaint[dst]=memoryTaint[src];
	//EK: Taint_Policy END

}

void taint_add_handler(uint32_t eip)
{

	//EK: replace opcode with insn mnemonic
	//cb[0x89] = &MOV_Ev_Gv;
	cb[0x80] = &MOV_Eb_Gb;
	int i;
	decode_address(eip, &eh, 0);
	if (taintPrint_flag)
	{
		//printf("taint_add_handler\n");
		//printf("eip:0x%08x",eip);
		monitor_printf(default_mon,"EIP:0x%08x",eh.address);

		monitor_printf(default_mon,"\tNumber of operands:%d",eh.num_operands);
		monitor_printf(default_mon,"\toperand type:{");
		for(i=0;i<eh.num_operands;i++){

			monitor_printf(default_mon," %s ",OpTypesNames[eh.operand[i].type]);
			if(eh.operand[i].type==TRegister)
				monitor_printf(default_mon,"(0x%02x)",eh.operand[i].addr.reg_addr);
			else if(eh.operand[i].type == TMemLoc)
				//EK: Check if this is the computed address (from XED2) or do you have to compute it manually
				monitor_printf(default_mon, "(0x%08x)",eh.operand[i].addr.mem32_addr);
			else if(eh.operand[i].type == TImmediate)
				monitor_printf(default_mon, "(0x%08x)",eh.operand[i].addr.mem32_addr);

			//monitor_printf(default_mon," %s ",OpUsageNames[eh.operand[i].usage]);

		}
		monitor_printf(default_mon,"}");

		monitor_printf(default_mon,"\tRawbytes:");
		for(i = 0;i<eh.inst_size;i++)
			monitor_printf(default_mon,"%x ",eh.rawbytes[i]);

		monitor_printf(default_mon,"insn size:%d",eh.inst_size);
		monitor_printf(default_mon,"\n");



	}
	if (cb[eh.rawbytes[0]])
				cb[eh.rawbytes[0]](eh.operand[0].addr.mem32_addr,eh.operand[1].addr.mem32_addr);

}
