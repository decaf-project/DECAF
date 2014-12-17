/**
 *  @Author Lok Yan
 */
#ifndef X86CPUSTATE_H
#define X86CPUSTATE_H


#include <inttypes.h>

typedef uint32_t reg32t;

struct SegmentRegister
{
  uint32_t selector;
  uint32_t base;
  uint32_t limit;
  uint32_t flags; //AR_BYTES = Access Rights Bytes
};

struct X86CPUState
{
  union
  {
    struct
    {
      uint32_t eax;
      uint32_t ecx;
      uint32_t edx;
      uint32_t ebx;
    };
    uint32_t arr[4]; //a, c, d, b
  } gen;

  union
  {
    struct
    {
      uint32_t esp;
      uint32_t ebp;
      uint32_t esi;
      uint32_t edi;
    };
    uint32_t arr[4];
  } stk;

  union
  {
    struct
    {
      uint32_t es;
      uint32_t cs;
      uint32_t ss;
      uint32_t ds;
      uint32_t fs;
      uint32_t gs;
    };
    uint32_t arr[6];
  } segregs;

  uint32_t eip;
  uint32_t cr[5];
  uint32_t eflags;

  uint32_t hflags;


  /*
  union
  {
    struct
    {
      SegmentRegister ES;
      SegmentRegister CS;
      SegmentRegister SS;
      SegmentRegister DS;
      SegmentRegister FS;
      SegmentRegister GS;
    };
    SegmentRegister arr[6];
  } segregs;
  */
  /*
//these registers are in the FXSAVE structure inside x86.c, so that is
// where we can find them.
//There is also a struct kvm_fpu inside asm/kvm.h of include. This contains a
// bunch of registers too

//Look at: http://www.softeng.rl.ac.uk/st/archive/SoftEng/SESP/html/SoftwareTools/vtune/users_guide/mergedProjects/analyzer_ec/mergedProjects/reference_olh/mergedProjects/instructions/instruct32_hh/vc112.htm
//  for information about the FRSTOR and FXRSTOR instructions.

//mmx registers
//in op_helper.c in fsave, there is a reference to ST() which is a macro
// that is defined in exec.h in the same directory
// The macro goes like this:
//   ST(n) (env->fpregs[(env->fpstt + (n)) & 7].d)
// Also there is a function call to helper_fstt  which does some magic on the side
// Will need to copy these functions over in order to get the same results.
//Looks like the best bet is to copy over the fldenv function
// or even better is to copy over the frstor function from op_helper.c
//In the end frstor is called in cpu-exec.c from cpu_x86_frstor.
// data32 is a 1 or 0 depending on whether it is 32bit data - which is true
// and ptr is the pointer to the fxstate or fstate data structure.
    union {
        long double d;
        //MMXReg mmx;
        uint64_t mmx;
    } fpregs[8];

//floating point registers
// look in target-i386/op_helper.c in helper_fstenv where env is the
// CPUState structure
//The thing that is missing is information about fptags[8] so for that
// we have to look into helper_fldenv
// In that function we see the following code:
// for (i = 0; i < 8; i++)
// {
//   env->fptags[i] = ((fptag & 3) == 3);
//   fptag >>= 2;
// }
// In this case fptag is going to be twd in fxstate
    uint32_t fpstt; // top of stack index
    uint32_t fpus; // floating point unit status - swd in fxstate
    uint32_t fpuc; // floating point unit control - cwd in fxstate
    uint8_t fptags[8];   // 0 = valid, 1 = empty
*/
};

#endif//X86CPUSTATE_H
