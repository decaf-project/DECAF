/*
Copyright (C) <2012> <Syracuse System Security (Sycure) Lab>
This is a plugin of DECAF. You can redistribute and modify it
under the terms of BSD license but it is made available
WITHOUT ANY WARRANTY. See the top-level COPYING file for more details.

For more information about DECAF and other softwares, see our
web site at:
http://sycurelab.ecs.syr.edu/

If you have any questions about DECAF,please post it on
http://code.google.com/p/decaf-platform/
*/
/**
 * @author Lok Yan
 * @date Oct 18 2012
 */

#include <sys/time.h>

#include "DECAF_types.h"
#include "DECAF_main.h"
#include "DECAF_callback.h"
#include "vmi_callback.h"
#include "utils/Output.h"
#include "vmi_c_wrapper.h"
//basic stub for plugins
static plugin_interface_t callbacktests_interface;
static int bVerboseTest = 0;

typedef struct _callbacktest_t
{
  char name[64];
  DECAF_callback_type_t cbtype;
  OCB_t ocbtype;
  gva_t from;
  gva_t to;
  DECAF_Handle handle;
  struct timeval tick;
  struct timeval tock;
  int count;
  double elapsedtime;
}callbacktest_t;

#define CALLBACKTESTS_TEST_COUNT 7

static callbacktest_t callbacktests[CALLBACKTESTS_TEST_COUNT] = {
    {"Block Begin Single", DECAF_BLOCK_BEGIN_CB, OCB_CONST, 0x7C90d9b0, 0, DECAF_NULL_HANDLE, {0, 0}, {0, 0}, 0, 0.0}, //0x7C90d580 is NtOpenFile 0x7C90d9b0 is NtReadFile
    {"Block Begin Page", DECAF_BLOCK_BEGIN_CB, OCB_PAGE, 0x7C90d9b0, 0, DECAF_NULL_HANDLE, {0, 0}, {0, 0}, 0, 0.0}, //0x7C90d090 is NtCreateFile
    {"Block Begin All", DECAF_BLOCK_BEGIN_CB, OCB_ALL, 0, 0, DECAF_NULL_HANDLE, {0, 0}, {0, 0}, 0, 0.0},
    {"Block End From Page", DECAF_BLOCK_END_CB, OCB_PAGE, 0x7C90d9b0, INV_ADDR, DECAF_NULL_HANDLE, {0, 0}, {0, 0}, 0, 0.0},
    {"Block End To Page", DECAF_BLOCK_END_CB, OCB_PAGE, INV_ADDR, 0x7C90d9b0, DECAF_NULL_HANDLE, {0, 0}, {0, 0}, 0, 0.0},
    {"Insn Begin", DECAF_INSN_BEGIN_CB, OCB_ALL, 0, 0, DECAF_NULL_HANDLE, {0, 0}, {0, 0}, 0, 0.0},
    {"Insn End", DECAF_INSN_END_CB, OCB_ALL, 0, 0, DECAF_NULL_HANDLE, {0, 0}, {0, 0}, 0, 0.0},
};

static int curTest = 0;
static DECAF_Handle processbegin_handle = DECAF_NULL_HANDLE;
static DECAF_Handle removeproc_handle = DECAF_NULL_HANDLE;

static char targetname[512];
static uint32_t targetpid;
static uint32_t targetcr3;

static void runTests(void);

static void callbacktests_printSummary(void);
static void callbacktests_resetTests(void);


static void callbacktests_printSummary(void)
{
  int i = 0;
  DECAF_printf("******* SUMMARY *******\n");
  DECAF_printf("%+30s\t%12s\t%10s\n", "Test", "Count", "Time");
  for (i = 0; i < CALLBACKTESTS_TEST_COUNT; i++)
  {
    DECAF_printf("%-30s\t%12u\t%.5f\n", callbacktests[i].name, callbacktests[i].count, callbacktests[i].elapsedtime);
  }
}

static void callbacktests_resetTests(void)
{
  int i = 0;
  for (i = 0; i < CALLBACKTESTS_TEST_COUNT; i++)
  {
    callbacktests[i].tick.tv_sec = 0;
    callbacktests[i].tick.tv_usec = 0;
    callbacktests[i].tock.tv_sec = 0;
    callbacktests[i].tock.tv_usec = 0;
    callbacktests[i].handle = 0;
    callbacktests[i].count = 0;
    callbacktests[i].elapsedtime = 0.0;
  }
}

static void callbacktests_loadmainmodule_callback(VMI_Callback_Params* params)
{
  char procname[64];
  uint32_t pid;

  if (params == NULL)
  {
    return;
  }

  //DECAF_printf("Process with pid = %d and cr3 = %u was just created\n", params->lmm.pid, params->lmm.cr3);

  VMI_find_process_by_cr3_c(params->cp.cr3, procname, 64, &pid);
  //in find_process pid is set to 1 if the process is not found
  // otherwise the number of elements in the module list is returned
  if (pid == (uint32_t)(-1))
  {
    return;
  }

  if (strcmp(targetname, procname) == 0)
  {
    targetpid = pid;
    targetcr3 = params->cp.cr3;
    runTests();
  }
}

static void callbacktests_removeproc_callback(VMI_Callback_Params* params)
{
  double elapsedtime;

  if (params == NULL)
  {
    return;
  }

  if (targetpid == params->rp.pid)
  {
    if (curTest >= CALLBACKTESTS_TEST_COUNT)
    {
      return;
    }

    if (callbacktests[curTest].handle == DECAF_NULL_HANDLE)
    {
      return;
    }

    //unregister the callback FIRST before getting the time of day - so
    // we don't get any unnecessary callbacks (although we shouldn't
    // since the guest should be paused.... right?)
    DECAF_unregister_callback(callbacktests[curTest].cbtype, callbacktests[curTest].handle);
    callbacktests[curTest].handle = DECAF_NULL_HANDLE;
    DECAF_printf("Callback Count = %u\n", callbacktests[curTest].count);

    gettimeofday(&callbacktests[curTest].tock, NULL);

    elapsedtime = (double)callbacktests[curTest].tock.tv_sec + ((double)callbacktests[curTest].tock.tv_usec / 1000000.0);
    elapsedtime -= ((double)callbacktests[curTest].tick.tv_sec + ((double)callbacktests[curTest].tick.tv_usec / 1000000.0));
    DECAF_printf("Process [%s] with pid [%d] ended at %u:%u\n", targetname, targetpid, callbacktests[curTest].tock.tv_sec, callbacktests[curTest].tock.tv_usec);
    DECAF_printf("  Elapsed time = %0.6f seconds\n", elapsedtime);

    callbacktests[curTest].elapsedtime = elapsedtime;

    //increment for the next test
    curTest++;
    if (curTest < CALLBACKTESTS_TEST_COUNT)
    {
      DECAF_printf("%d of %d tests completed\n", curTest, CALLBACKTESTS_TEST_COUNT);
      DECAF_printf("Please execute %s again to start next test\n", targetname);
    }
    else
    {
      DECAF_printf("All tests have completed\n");
      callbacktests_printSummary();
    }
    targetpid = (uint32_t)(-1);
    targetcr3 = 0;
  }
}

static void callbacktests_genericcallback(DECAF_Callback_Params* param)
{
  if (curTest >= CALLBACKTESTS_TEST_COUNT)
  {
    return;
  }

  //LOK: Setup to do a more comprehensive test that does something with the parameters
  //if (1 || bVerboseTest)
  if (0)
  {
    switch(callbacktests[curTest].cbtype)
    {
      case (DECAF_BLOCK_BEGIN_CB):
      {
        DECAF_printf("BB @ [%x]\n", param->bb.tb->pc);
        break;
      }
      case (DECAF_BLOCK_END_CB):
      {
        DECAF_printf("BE @ [%x] [%x] -> [%x]\n", param->be.tb->pc, param->be.cur_pc, param->be.next_pc);
        break;
      }
      default:
      case (DECAF_INSN_BEGIN_CB):
      case (DECAF_INSN_END_CB):
      {
        //do nothing yet?
      }
    }
  }

  //TODO: Add support for ONLY tracking target process and not ALL processes
  callbacktests[curTest].count++;
}

static void runTests(void)
{
  if (curTest >= CALLBACKTESTS_TEST_COUNT)
  {
    DECAF_printf("All tests have completed\n");
    return;
  }

  if (callbacktests[curTest].handle != DECAF_NULL_HANDLE)
  {
    DECAF_printf("%s test is currently running\n", callbacktests[curTest].name);
    return;
  }

  DECAF_printf("\n");
  DECAF_printf("**********************************************\n");
  DECAF_printf("Running the %s test\n", callbacktests[curTest].name);
  DECAF_printf("\n");
  gettimeofday(&callbacktests[curTest].tick, NULL);
  DECAF_printf("Process [%s] with pid [%d] started at %u:%u\n", targetname, targetpid, callbacktests[curTest].tick.tv_sec, callbacktests[curTest].tick.tv_usec);
  DECAF_printf("Registering for callback\n");

  switch(callbacktests[curTest].cbtype)
  {
    case (DECAF_BLOCK_BEGIN_CB):
    {
      callbacktests[curTest].handle = DECAF_registerOptimizedBlockBeginCallback(&callbacktests_genericcallback, NULL, callbacktests[curTest].from, callbacktests[curTest].ocbtype);
      break;
    }
    case (DECAF_BLOCK_END_CB):
    {
      callbacktests[curTest].handle = DECAF_registerOptimizedBlockEndCallback(&callbacktests_genericcallback, NULL, callbacktests[curTest].from, callbacktests[curTest].to);
      break;
    }
    default:
    case (DECAF_INSN_BEGIN_CB):
    case (DECAF_INSN_END_CB):
    {
      callbacktests[curTest].handle = DECAF_register_callback(callbacktests[curTest].cbtype, &callbacktests_genericcallback, NULL);
    }
  }

  if (callbacktests[curTest].handle == DECAF_NULL_HANDLE)
  {
    DECAF_printf("Could not register the event\n");
    return;
  }

  callbacktests[curTest].count = 0;
  DECAF_printf("Callback Registered\n");
}

void do_callbacktests(Monitor* mon, const QDict* qdict)
{

  if ((qdict != NULL) && (qdict_haskey(qdict, "procname")))
  {
    strncpy(targetname, qdict_get_str(qdict, "procname"), 512);
  }
  else
  {
    DECAF_printf("A program name was not specified, so we will use sort.exe\n");
    strncpy(targetname, "sort.exe", 512);
  }
  targetname[511] = '\0';

  curTest = 0;
  callbacktests_resetTests();
  DECAF_printf("Tests will be completed using: %s (case sensitive).\n", targetname);
  DECAF_printf("  Run the program to start the first test\n");
}

static int callbacktests_init(void)
{
  DECAF_output_init(NULL);
  DECAF_printf("Hello World\n");
  //register for process create and process remove events
  processbegin_handle = VMI_register_callback(VMI_CREATEPROC_CB, &callbacktests_loadmainmodule_callback, NULL);
  removeproc_handle = VMI_register_callback(VMI_REMOVEPROC_CB, &callbacktests_removeproc_callback, NULL);
  if ((processbegin_handle == DECAF_NULL_HANDLE) || (removeproc_handle == DECAF_NULL_HANDLE))
  {
    DECAF_printf("Could not register for the create or remove proc events\n");
  }

  targetname[0] = '\0';
  targetcr3 = 0;
  targetpid = (uint32_t)(-1);

  do_callbacktests(NULL, NULL);
  return (0);
}

static void callbacktests_cleanup(void)
{
  VMI_Callback_Params params;

  DECAF_printf("Bye world\n");

  if (processbegin_handle != DECAF_NULL_HANDLE)
  {
    VMI_unregister_callback(VMI_CREATEPROC_CB, processbegin_handle);
    processbegin_handle = DECAF_NULL_HANDLE;
  }

  if (removeproc_handle != DECAF_NULL_HANDLE)
  {
    VMI_unregister_callback(VMI_REMOVEPROC_CB, removeproc_handle);
    removeproc_handle = DECAF_NULL_HANDLE;
  }

  //make one final call to removeproc to finish any currently running tests
  if (targetpid != (uint32_t)(-1))
  {
    params.rp.pid = targetpid;
    callbacktests_removeproc_callback(&params);
  }

  curTest = 0;
}

static mon_cmd_t callbacktests_term_cmds[] = {
  #include "plugin_cmds.h"
  {NULL, NULL, },
};

plugin_interface_t* init_plugin(void)
{
  callbacktests_interface.mon_cmds = callbacktests_term_cmds;
  callbacktests_interface.plugin_cleanup = &callbacktests_cleanup;
  
  //initialize the plugin
  callbacktests_init();
  return (&callbacktests_interface);
}

