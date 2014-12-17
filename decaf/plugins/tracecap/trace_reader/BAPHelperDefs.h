/**
 * @author Lok Yan
 * @date 24 APR 2013
**/

#ifndef BAP_HELPER_DEFS_H
#define BAP_HELPER_DEFS_H

#define EFLAGS_CF_MASK (0x1 << 0)
#define EFLAGS_PF_MASK (0x1 << 2)
#define EFLAGS_AF_MASK (0x1 << 4)
#define EFLAGS_ZF_MASK (0x1 << 6)
#define EFLAGS_SF_MASK (0x1 << 7)
#define EFLAGS_OF_MASK (0x1 << 11)

#define EFLAGS_STATUS_FLAGS_MASK (EFLAGS_CF_MASK | EFLAGS_PF_MASK | EFLAGS_AF_MASK | EFLAGS_ZF_MASK | EFLAGS_SF_MASK | EFLAGS_OF_MASK) // 0x000008D5; 

#define BAP_HELPER_IS_STATUS_BIT(_i) (( 0x1 << _i ) & EFLAGS_STATUS_FLAGS_MASK)

#define ADDR_BYTES 4

static const char* eflags_to_string[32] = {
"R_CF:bool", //0
"R_RESERVED:bool", 
"R_PF:bool", //2
"R_RESERVED:bool",
"R_AF:bool", //4
"R_RESERVED:bool",
"R_ZF:bool", //6
"R_SF:bool", //7
"R_UNSUPPORTED:bool", //"R_TF:bool", //8 
"R_UNSUPPORTED:bool", //"R_IF:bool", //9
"R_UNSUPPORTED:bool", //"R_DF:bool", //10
"R_OF:bool", //11
"R_UNSUPPORTED:bool", //"R_IOPL0:bool", //12
"R_UNSUPPORTED:bool", //"R_IOPL2:bool", //13
"R_UNSUPPORTED:bool", //"R_NT:bool", //14
"R_RESERVED:bool", //15
"R_UNSUPPORTED:bool", //"R_RF:bool", //16
"R_UNSUPPORTED:bool", //"R_VM:bool", //17
"R_UNSUPPORTED:bool", //"R_AC:bool", //18
"R_UNSUPPORTED:bool", //"R_VIF:bool", //19
"R_UNSUPPORTED:bool", //"R_VIP:bool", //20
"R_UNSUPPORTED:bool", //"R_ID:bool", //21
"R_RESERVED:bool",
"R_RESERVED:bool",
"R_RESERVED:bool",
"R_RESERVED:bool", //25
"R_RESERVED:bool",
"R_RESERVED:bool",
"R_RESERVED:bool",
"R_RESERVED:bool",
"R_RESERVED:bool", //30
"R_RESERVED:bool" //31
};

static const char* regnum_to_string[40][2] = {
{"R_ES","R_ES:u32"}, //0
{"R_CS","R_CS:u32"}, 
{"R_SS","R_SS:u32"}, 
{"R_DS","R_DS:u32"},
{"R_FS","R_FS:u32"}, 
{"R_GS","R_GS:u32"}, //5
{"",""}, 
{"",""}, 
{"",""}, 
{"",""}, 
{"",""}, //10
{"",""}, 
{"",""}, 
{"",""}, 
{"",""}, 
{"",""}, 
{"R_EAX","R_EAX:u32"}, //{"R_AL","R_AL:u8"}, //16 //IN BAP EAX is used, but then the logic chooses the lowest byte for example
{"R_ECX","R_ECX:u32"}, 
{"R_EDX","R_EDX:u32"}, 
{"R_EBX","R_EBX:u32"},
{"R_EAX","R_EAX:u32"}, //20
{"R_ECX","R_ECX:u32"},
{"R_EDX","R_EDX:u32"}, 
{"R_EBX","R_EBX:u32"}, 
{"R_EAX","R_EAX:u32"}, //24
{"R_ECX","R_ECX:u32"}, 
{"R_EDX","R_EDX:u32"}, 
{"R_EBX","R_EBX:u32"},
{"R_ESP","R_ESP:u32"}, 
{"R_EBP","R_EBP:u32"}, 
{"R_ESI","R_ESI:u32"}, //30
{"R_EDI","R_EDI:u32"},
{"R_EAX","R_EAX:u32"}, //32
{"R_ECX","R_ECX:u32"}, 
{"R_EDX","R_EDX:u32"}, 
{"R_EBX","R_EBX:u32"},
{"R_ESP","R_ESP:u32"},
{"R_EBP","R_EBP:u32"}, 
{"R_ESI","R_ESI:u32"}, 
{"R_EDI","R_EDI:u32"} //39
};

static const uint8_t regnum_to_size[40] = {
32, //0
32,
32,
32,
32,
32, //5
0,
0,
0,
0,
0, //10
0,
0,
0,
0,
0,
32, //8, //16 //Need to change this to 32bit because thats what BAP does
32, //8,
32, //8,
32, //8,
32, //8,
32, //8,
32, //8,
32, //8,
32, //16, //24
32, //16,
32, //16,
32, //16,
32, //16,
32, //16,
32, //16,
32, //16,
32, //32
32,
32,
32,
32,
32,
32,
32 //39
};

static const char* bytesize_to_string[5] = {
"u0",
"u8",
"u16",
"u24",
"u32"
};

//have some protections here, should I use %?
#define EFLAGS_TO_STRING(_i) (eflags_to_string[ _i & 0x1F ])
#define REGNUM_TO_STRING(_i) (regnum_to_string[_i][0])
#define REGNUM_TO_FULLSTRING(_i) (regnum_to_string[_i][1])
#define BYTESIZE_TO_STRING(_i) (bytesize_to_string[ _i])
#define REGNUM_TO_SIZE_STRING(_i) (BYTESIZE_TO_STRING( regnum_to_size[_i] >> 3 ))

#endif //BAP_HERLPER_DEFS_H

