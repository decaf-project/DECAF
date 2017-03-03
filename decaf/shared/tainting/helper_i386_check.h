#ifndef TARGET_I386
#error Included I386 helper header in non-I386 build
#endif /* TARGET_I386 */

#if defined(HELPER_SECTION_ONE)
#undef HELPER_SECTION_ONE
/* HELPER_SECTION_ONE declares all of the flag variables that are used
  to determine which tainting rules to apply in op_call */
  static int in_helper_func = 0;
  static int out_helper_func = 0;
  static int sysexit_helper_func = 0;
  static int divb_helper_func = 0;
  static int divw_helper_func = 0;
  static int divl_helper_func = 0;
  static int clean_eax_after = 0;
  static int clean_ebx_after = 0;
  static int clean_ecx_after = 0;
  static int clean_edx_after = 0;
  static int clean_esp_after = 0;
  static int clean_tempidx_before = 0;
  static TCGv backup_orig[10];
#elif defined(HELPER_SECTION_TWO)
#undef HELPER_SECTION_TWO
  /* HELPER_SECTION_TWO is included in the op_movi opcode logic for
    tainting.  It checks if the in parm is a helper function address
    and then sets the flags so that op_call can apply the proper
    taint logic for the helper. */ 
  in_helper_func = out_helper_func = sysexit_helper_func = 0;
  divb_helper_func = divw_helper_func = divl_helper_func = 0;
  clean_eax_after = clean_ebx_after = clean_ecx_after = 0;
  clean_edx_after = clean_esp_after = 0; 
  clean_tempidx_before = 1;
  /* Check if the constant is a helper function for IN* opcodes */
  if ( (gen_opparam_ptr[-1] == (tcg_target_ulong)helper_inb) ||
    (gen_opparam_ptr[-1] == (tcg_target_ulong)helper_inw) ||
    (gen_opparam_ptr[-1] == (tcg_target_ulong)helper_inl) )
  {
    in_helper_func = 1;
    clean_tempidx_before = 0;
    //fprintf(stderr, "helper_in*() found\n");
  }
  /* Check if the constant is a helper function for OUT* opcodes */
  else if ( (gen_opparam_ptr[-1] == (tcg_target_ulong)helper_outb) ||
    (gen_opparam_ptr[-1] == (tcg_target_ulong)helper_outw) ||
    (gen_opparam_ptr[-1] == (tcg_target_ulong)helper_outl) )
  {
    out_helper_func = 1;
    clean_tempidx_before = 0;
    //fprintf(stderr, "helper_out*() found\n");
  }
  /* Check if the constant is a helper function for CMPXCHG */
  else if (gen_opparam_ptr[-1] == (tcg_target_ulong)helper_DECAF_taint_cmpxchg)
  {
    clean_eax_after = 1;
    //fprintf(stderr, "helper_cmpxchg() found\n");
  }
  /* Check if the constant is a helper function for RDMSR or RDTSC */
  else if ( (gen_opparam_ptr[-1] == (tcg_target_ulong)helper_rdmsr) ||
    (gen_opparam_ptr[-1] == (tcg_target_ulong)helper_rdtsc) )
  {
    clean_eax_after = 1;
    clean_edx_after = 1;
    //fprintf(stderr, "helper_rdmsr/rdtsc() found\n");
  }
  /* Check if the constant is a helper function for RDTSCP */
  else if (gen_opparam_ptr[-1] == (tcg_target_ulong)helper_rdtscp)
  {
    clean_eax_after = 1;
    clean_ecx_after = 1;
    clean_edx_after = 1;
    //fprintf(stderr, "helper_rdtscp() found\n");
  }
  /* Check if the constant is a helper function for CPUID */
  else if (gen_opparam_ptr[-1] == (tcg_target_ulong)helper_cpuid)
  {
    clean_eax_after = 1;
    clean_ebx_after = 1;
    clean_ecx_after = 1;
    clean_edx_after = 1;
    //fprintf(stderr, "helper_cpuid() found\n");
  }
  /* Check if the constant is a helper function for SYSEXIT */
  else if (gen_opparam_ptr[-1] == (tcg_target_ulong)helper_sysexit)
  {
    sysexit_helper_func = 1;
    //fprintf(stderr, "helper_sysexit() found\n");
  }
  /* Check if the constant is a helper function for IDIVB/DIVB */
  else if ( (gen_opparam_ptr[-1] == (tcg_target_ulong)helper_divb_AL) ||
    (gen_opparam_ptr[-1] == (tcg_target_ulong)helper_idivb_AL) )
  {
    divb_helper_func = 1;
  }
  /* Check if the constant is a helper function for IDIVW/DIVW */
  else if ( (gen_opparam_ptr[-1] == (tcg_target_ulong)helper_divw_AX) ||
    (gen_opparam_ptr[-1] == (tcg_target_ulong)helper_idivw_AX) )
  {
    divw_helper_func = 1;
  }
  /* Check if the constant is a helper function for IDIVL/DIVL */
  else if ( (gen_opparam_ptr[-1] == (tcg_target_ulong)helper_divl_EAX) ||
    (gen_opparam_ptr[-1] == (tcg_target_ulong)helper_idivl_EAX) )
  {
    divl_helper_func = 1;
  } 
#elif defined(HELPER_SECTION_THREE)
#undef HELPER_SECTION_THREE
    /* HELPER_SECTION_THREE is included in the op_call opcode logic for
    tainting.  It checks the helper function flags and performs some
    operation PRIOR to the op_call.  Usually, this is clearing tempidx. */

    /* Are we inserting something prior to the call? */
    if (out_helper_func || clean_tempidx_before)
    {
      /* Backup the op_call args */
      for (i=0; i < nb_args; i++)
        backup_orig[i] = gen_opparam_ptr[-(i+1)];

      /* Any helper functions that need to grab shadow registers for
        op_call parms should do that here. */
      if (out_helper_func)
      { 
        /* Find the shadow for the data input */
        arg6 = find_shadow_arg(gen_opparam_ptr[-4]);
      }

      /* Back up in the opcode stream */
      gen_opparam_ptr -= nb_args;
      gen_opc_ptr--;
    
      /* Insert whatever needs to go before the op_call in the 
         opcode stream */
      if (out_helper_func && arg6) 
      {
        tcg_gen_st_tl(arg6, cpu_env, offsetof(OurCPUState,tempidx));
      } else {
        clean_tempidx_before = 1;
      }

      if (clean_tempidx_before)
      {
#if TCG_TARGET_REG_BITS == 32
        t0 = tcg_temp_new_i32();
        tcg_gen_movi_i32(t0, 0);
        tcg_gen_st_tl(t0, cpu_env, offsetof(OurCPUState,tempidx));
#else
        t0 = tcg_temp_new_i64();
        tcg_gen_movi_i64(t0, 0);
        tcg_gen_st_tl(t0, cpu_env, offsetof(OurCPUState,tempidx));
#endif /* TARGET_REG_BITS == 32 */
      }

      /* Manually reinsert the CALL opcode */
      *(gen_opc_ptr++) = INDEX_op_call;
      gen_opparam_ptr += nb_args;
      for(i=0; i < nb_args; i++)
      {
        gen_opparam_ptr[-(i+1)] = backup_orig[i];
      }
    }

#elif defined(HELPER_SECTION_FOUR)
#undef HELPER_SECTION_FOUR
    /* HELPER_SECTION_FOUR is included in the op_call opcode logic for
    tainting.  It checks the helper function flags and performs some
    operation AFTER the op_call for the helper output. */
    if (in_helper_func) 
    {
      out_helper_func = 0;
      tcg_gen_ld32u_tl(arg0, cpu_env, offsetof(OurCPUState,tempidx));
      in_helper_func = 0;
    } else
#elif defined(HELPER_SECTION_FIVE)
#undef HELPER_SECTION_FIVE
    /* HELPER_SECTION_FIVE is included in the op_call opcode logic for
    tainting.  It checks the helper function flags and performs some
    operation AFTER the op_call.  Usually, this is clearing some regs. */
#if TCG_TARGET_REG_BITS == 32
      t0 = tcg_temp_new_i32();
#else
      t0 = tcg_temp_new_i64();
#endif /* TARGET_REG_BITS == 32 */

    if (clean_eax_after || clean_ebx_after || clean_ecx_after ||
      clean_edx_after || clean_esp_after)
    {
#if TCG_TARGET_REG_BITS == 32
      tcg_gen_movi_i32(t0, 0);
#else
      tcg_gen_movi_i64(t0, 0);
#endif /* TARGET_REG_BITS == 32 */
    }
    if (clean_eax_after)
      tcg_gen_st_tl(t0, cpu_env, offsetof(OurCPUState,taint_regs[R_EAX]));
    if (clean_ebx_after)
      tcg_gen_st_tl(t0, cpu_env, offsetof(OurCPUState,taint_regs[R_EBX]));
    if (clean_ecx_after)
      tcg_gen_st_tl(t0, cpu_env, offsetof(OurCPUState,taint_regs[R_ECX]));
    if (clean_edx_after)
      tcg_gen_st_tl(t0, cpu_env, offsetof(OurCPUState,taint_regs[R_EDX]));
    if (clean_esp_after)
      tcg_gen_st_tl(t0, cpu_env, offsetof(OurCPUState,taint_regs[R_ESP]));
    if (sysexit_helper_func)
    {
      /* ESP <- ECX */
      tcg_gen_ld_tl(t0, cpu_env, offsetof(OurCPUState,taint_regs[R_ECX]));
      tcg_gen_st_tl(t0, cpu_env, offsetof(OurCPUState,taint_regs[R_ESP]));
      /* EIP <- EDX */
      tcg_gen_ld_tl(t0, cpu_env, offsetof(OurCPUState,taint_regs[R_EDX]));
      tcg_gen_st_tl(t0, cpu_env, offsetof(OurCPUState,eip_taint));
    }
    if (divb_helper_func || divw_helper_func || divl_helper_func)
    {
#if TCG_TARGET_REG_BITS == 32
      t0 = tcg_temp_new_i32(); /* Original EAX taint */
      t1 = tcg_temp_new_i32(); /* Original EDX taint */
      t2 = tcg_temp_new_i32(); /* Mask */
      t3 = tcg_temp_new_i32(); /* Arg to helper */
      t4 = tcg_temp_new_i32(); /* Temp result for taint compare */
      t5 = tcg_temp_new_i32(); /* New EAX taint */
      t6 = tcg_temp_new_i32(); /* New EDX taint */
      t_zero = tcg_temp_new_i32(); /* Zero constant */
#else
      t0 = tcg_temp_new_i64(); /* Original EAX */
      t1 = tcg_temp_new_i64(); /* Original EDX */
      t2 = tcg_temp_new_i64(); /* Mask */
      t3 = tcg_temp_new_i64(); /* Arg to helper */
      t4 = tcg_temp_new_i64(); /* Temp result for taint compare */
      t5 = tcg_temp_new_i32(); /* New EAX taint */
      t6 = tcg_temp_new_i32(); /* New EDX taint */
      t_zero = tcg_temp_new_i64(); /* Zero constant */
#endif /* TARGET_REG_BITS == 32 */
      /* Load denominator arg into t3 */
      t3 = find_shadow_arg(gen_opparam_ptr[-4]);
      /* Load EAX into t0 */
      tcg_gen_ld_tl(t0, cpu_env, offsetof(OurCPUState,taint_regs[R_EAX]));
      if (!divb_helper_func)
      {
        /* Load EDX into t1 */
        tcg_gen_ld_tl(t1, cpu_env, offsetof(OurCPUState,taint_regs[R_EDX]));
      }
      else 
      {
        /* Make a mask for the LS8 bits */
        tcg_gen_movi_i32(t2, 0xFF);
        /* Mask the denominator */
        tcg_gen_and_i32(t3, t3, t2);
      }
      if (divb_helper_func || divw_helper_func)
      {
        /* Make a mask for the LS16 bits */
        tcg_gen_movi_i32(t2, 0xFFFF);
        /* Make a copy of EAX taint */
        tcg_gen_mov_i32(t5, t0);
        /* Mask EAX */
        tcg_gen_and_i32(t0, t0, t2);
        if (divw_helper_func) {
          /* Make a copy of EDX taint */
          tcg_gen_mov_i32(t6, t1);
          /* Mask EDX */
          tcg_gen_and_i32(t1, t1, t2);
          /* Mask the denominator */
          tcg_gen_and_i32(t3, t3, t2); 
        }
      } 
      /* OR together the various taint sources */
      if (!divb_helper_func)
        tcg_gen_or_i32(t3, t3, t1); // AWH - Rearranged
      tcg_gen_or_i32(t3, t3, t0); // AWH - Rearranged

      /* Set taint in all bits if there is any taint in t3 */
      tcg_gen_movi_i32(t_zero, 0);
      tcg_gen_setcond_i32(TCG_COND_NE, t4, t3, t_zero);
      tcg_gen_neg_i32(t0, t4);
      if (!divb_helper_func)
        tcg_gen_mov_i32(t1, t0); // AWH - Added

      /* Apply a mask to the taint for DIVB and DIVW */
      if (divb_helper_func || divw_helper_func)
      {
        /* Mask the taint in EAX */
        tcg_gen_and_i32(t0, t0, t2);
        if (divw_helper_func)
          /* Mask the taint in EDX */
          tcg_gen_mov_i32(t1, t0);
      }

      /* Mask off the original taint and OR the new taint with it */
      if (divb_helper_func || divw_helper_func)
      {
        /* Create a new mask for the MS16B */
        tcg_gen_movi_i32(t2, 0xFFFF0000);
        /* Mask off the original EAX taint */
        tcg_gen_and_i32(t5, t5, t2);
        /* OR the new taint with the old */
        tcg_gen_or_i32(t0, t0, t5);
        if (divw_helper_func)
        {
          /* Mask off the original EDX taint */
          tcg_gen_and_i32(t6, t6, t2);
          /* OR the new taint with the old */
          tcg_gen_or_i32(t1, t1, t6); 
        }
      }
      /* Finally, store the taint back into the registers */
      tcg_gen_st_tl(t0, cpu_env, offsetof(OurCPUState,taint_regs[R_EAX]));
      if (!divb_helper_func)
        tcg_gen_st_tl(t1, cpu_env, offsetof(OurCPUState,taint_regs[R_EDX]));
    }
#else
#error You need to define a HELPER_SECTION_... before including header
#endif /* HELPER_SECTION_* */
 
