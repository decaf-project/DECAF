/* 
   Tracecap is owned and copyright (C) BitBlaze, 2007-2010.
   All rights reserved.
   Do not copy, disclose, or distribute without explicit written
   permission. 

   Author: Juan Caballero <jcaballero@cmu.edu>
*/
#ifndef _READWRITE_H_
#define _READWRITE_H_
#include "trace.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

void update_written_operands (EntryHeader *eh);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // _READWRITE_H_

