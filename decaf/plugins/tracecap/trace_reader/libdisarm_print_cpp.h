#ifndef LIB_DISARM_PRINT_CPP_H
#define LIB_DISARM_PRINT_CPP_H

#include "libdisarm_types.h"

//LOK: Added this for C++ support and strings
#include <iostream>

void da_instr_ostream(std::ostream&, const da_instr_t *instr,
    const da_instr_args_t *args, da_addr_t addr);

#endif//LIB_DISARM_PRINT_CPP_H
