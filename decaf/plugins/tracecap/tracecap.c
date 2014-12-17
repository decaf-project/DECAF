/* 
   Tracecap is owned and copyright (C) BitBlaze, 2007-2010.
   All rights reserved.
   Do not copy, disclose, or distribute without explicit written
   permission. 

   Author: Juan Caballero <jcaballero@cmu.edu>
           Zhenkai Liang <liangzk@comp.nus.edu.sg>
*/
#include "config.h"
#include <stdio.h>
#include <sys/user.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "tracecap.h"
#include "bswap.h"
#include "shared/function_map.h"
#include "vmi_callback.h"
#include "vmi_c_wrapper.h"
#include "conf.h"
#include "conditions.h" // AWH


FILE *tracelog = 0;
FILE *tracenetlog = 0;
FILE *tracehooklog = 0;
FILE *calllog = 0;
FILE *alloclog = 0;
uint32_t tracepid = 0;
target_ulong tracecr3 = 0;
uint32_t dump_pc_start = 0;
int skip_decode_address = 0;
int skip_trace_write = 0;
unsigned int tracing_child = 0;
uint32_t insn_tainted=0;
extern int should_trace_all_kernel;

/* Filename for functions file */
char functionsname[128]= "";

/* Filename for trace file */
char tracename[128]= "";
char *tracename_p = tracename;


/* Start usage */
struct rusage startUsage;

/* Entry header */
EntryHeader eh;


int tracing_start(uint32_t pid, const char *filename)
{
  /* Copy trace filename to global variable */
  strncpy(tracename, filename, 128);

  /* Set name for functions file */
  snprintf(functionsname, 128, "%s.functions", filename);

  /* If previous trace did not close properly, close files now */
  if (tracelog){
 //   close_trace(tracelog); //update to tracereader version 50
  fclose(tracelog);
}
  if (tracenetlog)
    fclose(tracenetlog);

  /* Initialize trace file */
  tracelog = fopen(filename, "w");
  if (0 == tracelog) {
    perror("tracing_start");
    tracepid = 0;
    tracecr3 = 0;
    return -1;
  }
  setvbuf(tracelog, filebuf, _IOFBF, FILEBUFSIZE);

  /* Initialize netlog file */
  char netname[128];
  snprintf(netname, 128, "%s.netlog", filename);
  tracenetlog = fopen(netname, "w");
  if (0 == tracenetlog) {
    perror("tracing_start");
    tracepid = 0;
    tracecr3 = 0;
    return -1;
  }
  else {
    fprintf(tracenetlog, "Flow       Off  Data\n");
    fflush(tracenetlog);
  }

  /* Initialize external calls file (if requested) */ 
  if (conf_log_external_calls) {
    skip_trace_write = 1;
    char callname[128];
    if (calllog)
      fclose(calllog);
    snprintf(callname, 128, "%s.calls", filename);
    calllog = fopen(callname, "w");
    if (0 == calllog) {
      perror("tracing_start");
      tracepid = 0;
      tracecr3 = 0;
      return -1;
    }
    setvbuf(calllog, filebuf, _IOFBF, FILEBUFSIZE);
  }

  /* Set PID and CR3 of the process to be traced */
  if(pid == -1) //trace kernel
	  tracecr3  = VMI_find_cr3_by_pid_c(0);
  else
	  tracecr3 = VMI_find_cr3_by_pid_c(pid);
  if (0 == tracecr3) {
    monitor_printf(default_mon, "CR3 for PID %d not found. Tracing all processes!\n",pid);
    tracepid = -1;
  }
  else {
    tracepid = pid;
  }
  monitor_printf(default_mon, "PID: %d CR3: 0x%08x\n", tracepid, tracecr3);

  /* Initialize disassembler */
  xed2_init();

  /* Clear trace statistics */
  clear_trace_stats();

  //-hu
  /* Clear skip taint flags */
  //init_st();

  /* Initialize hooks only for this process */
  decaf_plugin->monitored_cr3 = tracecr3;

  /* Get system start usage */
  if (getrusage(RUSAGE_SELF, &startUsage) != 0)
    monitor_printf (default_mon, "Could not get start usage\n");

  return 0;
}

void tracing_stop()
{
  /* If not tracing return */
  if (tracepid == 0)
    return;

  if (tracelog) {
    //close_trace(tracelog); //update to tracereader version 50
   fclose(tracelog);
    tracelog = 0;
  }

  monitor_printf(default_mon, "Stop tracing process %d\n", tracepid);
  print_trace_stats();
  should_trace_all_kernel=0;
  procname_clear();
  tracepid=0;
  /* Get system stop usage */
  struct rusage stopUsage;
  if (getrusage(RUSAGE_SELF, &stopUsage) == 0) {
    double startUT = (double)startUsage.ru_utime.tv_sec +
                    (double)startUsage.ru_utime.tv_usec / 1e6;
    double startST = (double)startUsage.ru_stime.tv_sec +
                    (double)startUsage.ru_stime.tv_usec / 1e6;
    double stopUT = (double)stopUsage.ru_utime.tv_sec +
                    (double)stopUsage.ru_utime.tv_usec / 1e6;
    double stopST = (double)stopUsage.ru_stime.tv_sec +
                    (double)stopUsage.ru_stime.tv_usec / 1e6;

    double userProcessTime = (stopUT - startUT);
    double systemProcessTime = (stopST - startST);
    double processTime =  userProcessTime + systemProcessTime;

    monitor_printf (default_mon, "Processing time: %g U: %g S: %g\n",
      processTime, userProcessTime, systemProcessTime);
  }
  else {
    monitor_printf(default_mon, "Could not get usage\n");
  }


  tracepid = 0;
  header_already_written = 0;

  if (tracenetlog) {
    fclose(tracenetlog);
    tracenetlog = 0;
  }

  if (tracehooklog) {
    fclose(tracehooklog);
    tracehooklog = 0;
  }

  if (alloclog) {
    fclose(alloclog);
    alloclog = 0;
  }

  // Clear statistics
  clear_trace_stats();

  // Clear received_data flag
  received_tainted_data = 0;

// Print file with all functions offsets
#if PRINT_FUNCTION_MAP
  //map_to_file(functionsname);
#endif

  if (conf_log_external_calls) {
    if (calllog) {
      fclose(calllog);
      calllog = 0;
    }
  }

  if (conf_save_state_at_trace_stop) {
    char statename[128];
    snprintf(statename, 128, "%s.state", tracename);
//TODO: fix this -hu
  //  int err = save_state_by_cr3(tracecr3, statename);
 //   if (err) {
  //    monitor_printf(default_mon, "Could not save state");
   // }
  }

  /* Clear tracing child flag */
  if (tracing_child) {
    tracing_child = 0;
    skip_trace_write = 0;
  }

  /* Do not unload hooks, it'd crash emulation */

}

