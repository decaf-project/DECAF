/**
 * Copyright (C) <2012> <Syracuse System Security (Sycure) Lab>
 *
 * This library is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
**/

/**
 * @author Lok Yan
 * @date Oct 18 2012
 */

#include <sys/time.h>

#include "DECAF_shared/DECAF_main.h"
#include "DECAF_shared/DroidScope/DalvikAPI.h"
#include "DECAF_shared/utils/OutputWrapper.h"
#include "DS_Common.h"
#include "dalvikAPI/DalvikOpcodeTable.h"
#include "dalvikAPI/AndroidHelperFunctions.h"
#include "dalvikAPI/DalvikConstants.h"
#include "dalvikAPI/DalvikPrinter.h"

gpid_t gTracingPID = -1;

void do_trace_by_pid(Monitor* mon, gpid_t pid)
{
  printf("%d, %d\n", mterp_initIBase(pid, getIBase(pid)), disableJitInitGetCodeAddr(pid, getGetCodeAddrAddress(pid)));
  printf("[%d],[%d]\n", addMterpOpcodesRange(pid, 0, 0xFFFFFFFF), addDisableJitRange(pid, 0, 0xFFFFFFFF));
  gTracingPID = pid;
}

static DECAF_Handle DIT_handle;

static LogLevel eLogLevel = LOG_LEVEL_VERBOSE;
//static LogLevel eLogLevel = LOG_LEVEL_SIMPLE;

static void DIT_IBCallback(Dalvik_Callback_Params* params)
{
  int insnWidth = 0;
  DecodedInstruction decInsn;
  u2 insns[128];

  if (params == NULL)
  {
    return;
  }
  
  CPUState* env = params->ib.env;
  gva_t rpc = params->ib.dalvik_pc;

  if (getDalvikInstruction(env, rpc, &insnWidth, insns, 128) != 0)
  {
    DECAF_printf("Could not read the instruction at [%x]\n", rpc);
    return;
  }

  if (eLogLevel == LOG_LEVEL_SIMPLE)
  {
    char symbolName[128];

    if (getSymbol(symbolName, 128, getCurrentPID(), rpc) == 0)
    {
      DECAF_printf(" ***** %s ***** \n", symbolName);
    }

    DECAF_printf("[%08x] %s ", rpc, dalvikOpcodeToString(params->ib.opcode));
    int i = 0;
    for (i = 0; i < 8; i++) {
        if (i < insnWidth) {
            if (i == 7) {
                DECAF_printf(" ... ");
            } else {
                /* print 16-bit value in little-endian order */
                const u1* bytePtr = (const u1*) &insns[i];
                DECAF_printf(" %02x%02x", bytePtr[0], bytePtr[1]);
            }
        } else {
            DECAF_printf("     ");
         }
    }
    DECAF_printf("\n");
    return;
  }

  decodeDalvikInstruction(insns, &decInsn);

  dumpDalvikInstruction(stdout, env, insns, 0, insnWidth, &decInsn, 0, rpc, eLogLevel);
  
}

static int DIT_init(void)
{
  DIT_handle = DS_Dalvik_register_callback(DS_DALVIK_INSN_BEGIN_CB, &DIT_IBCallback, NULL);
  
  if (DIT_handle == DECAF_NULL_HANDLE)
  {
    DECAF_printf("Could not register for Dalvik Instruction Begin \n");
    return (-1);
  }

  return (0);
}

static mon_cmd_t DIT_term_cmds[] = {
  #include "plugin_cmds.h"
  {NULL, NULL, },
};

void DIT_cleanup()
{
  if (gTracingPID != -1)
  {
    mterp_clear(gTracingPID);
    gTracingPID = -1;
  }

  if (DIT_handle != DECAF_NULL_HANDLE)
  {
    DS_Dalvik_unregister_callback(DS_DALVIK_INSN_BEGIN_CB, DIT_handle);
    DIT_handle = DECAF_NULL_HANDLE;
  }
}

plugin_interface_t DIT_interface;

plugin_interface_t* init_plugin(void)
{
  DIT_interface.mon_cmds = DIT_term_cmds;
  DIT_interface.plugin_cleanup = &DIT_cleanup;
  
  //initialize the plugin
  DIT_init();
  return (&DIT_interface);
}

