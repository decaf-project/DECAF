#ifndef LIBDASM_PART2_H
#define LIBDASM_PART2_H

// Registers
#define REGISTER_EAX 0
#define REGISTER_ECX 1
#define REGISTER_EDX 2
#define REGISTER_EBX 3
#define REGISTER_ESP 4
#define REGISTER_EBP 5
#define REGISTER_ESI 6
#define REGISTER_EDI 7
#define REGISTER_NOP 8  // no register defined

// Registers
#define REG_EAX REGISTER_EAX
#define REG_AX REG_EAX
#define REG_AL REG_EAX
#define REG_ES REG_EAX          // Just for reg_table consistence
#define REG_ST0 REG_EAX         // Just for reg_table consistence
#define REG_ECX REGISTER_ECX
#define REG_CX REG_ECX
#define REG_CL REG_ECX
#define REG_CS REG_ECX
#define REG_ST1 REG_ECX
#define REG_EDX REGISTER_EDX
#define REG_DX REG_EDX
#define REG_DL REG_EDX
#define REG_SS REG_EDX
#define REG_ST2 REG_EDX
#define REG_EBX REGISTER_EBX
#define REG_BX REG_EBX
#define REG_BL REG_EBX
#define REG_DS REG_EBX
#define REG_ST3 REG_EBX
#define REG_ESP REGISTER_ESP
#define REG_SP REG_ESP
#define REG_AH REG_ESP          // Just for reg_table consistence
#define REG_FS REG_ESP
#define REG_ST4 REG_ESP
#define REG_EBP REGISTER_EBP
#define REG_BP REG_EBP
#define REG_CH REG_EBP
#define REG_GS REG_EBP
#define REG_ST5 REG_EBP
#define REG_ESI REGISTER_ESI
#define REG_SI REG_ESI
#define REG_DH REG_ESI
#define REG_ST6 REG_ESI
#define REG_EDI REGISTER_EDI
#define REG_DI REG_EDI
#define REG_BH REG_EDI
#define REG_ST7 REG_EDI
#define REG_NOP REGISTER_NOP

// Implied operands
#define IOP_EAX         1
#define IOP_ECX         (1 << REG_ECX)
#define IOP_EDX         (1 << REG_EDX)
#define IOP_EBX         (1 << REG_EBX)
#define IOP_ESP         (1 << REG_ESP)
#define IOP_EBP         (1 << REG_EBP)
#define IOP_ESI         (1 << REG_ESI)
#define IOP_EDI         (1 << REG_EDI)
#define IOP_ALL         IOP_EAX|IOP_ECX|IOP_EDX|IOP_ESP|IOP_EBP|IOP_ESI|IOP_EDI
#define IS_IOP_REG(x,y) (x >> y) & 1
#define IS_IOP_EAX(x)   (x) & 1
#define IS_IOP_ECX(x)   (x >> REG_ECX) & 1
#define IS_IOP_EDX(x)   (x >> REG_EDX) & 1
#define IS_IOP_EBX(x)   (x >> REG_EBX) & 1
#define IS_IOP_EBP(x)   (x >> REG_EBP) & 1
#define IS_IOP_ESI(x)   (x >> REG_ESI) & 1
#define IS_IOP_EDI(x)   (x >> REG_EDI) & 1


// Register types
#define REGISTER_TYPE_GEN       1
#define REGISTER_TYPE_SEGMENT   2
#define REGISTER_TYPE_DEBUG     3
#define REGISTER_TYPE_CONTROL   4
#define REGISTER_TYPE_TEST      5
#define REGISTER_TYPE_XMM       6
#define REGISTER_TYPE_MMX       7
#define REGISTER_TYPE_FPU       8

#endif//LIBDASM_PART2_H
