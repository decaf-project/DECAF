/* 
   Tracecap is owned and copyright (C) BitBlaze, 2007-2010.
   All rights reserved.
   Do not copy, disclose, or distribute without explicit written
   permission. 

   Author: Juan Caballero <jcaballero@cmu.edu>
           Min Gyung Kang <mgkang@cmu.edu>
*/
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "trace.h"
#include "trackproc.h"
#include "DECAF_target.h"

/* Tracked Process information */
typedef struct {
  uint32_t m_uiHandle; // Process handle
  uint32_t m_uiPID;    // Process PID
  int m_iParent;       // Index of parent process in array
} TrackProcInfo;

/* Total number of processes being tracked (includes parent) */
static int l_iNumProc;

/* Flag to mark whether we are tracking process creation */
static int l_iRunning;

/* Array of processes. Zero index is parent, rest are children */
TrackProcInfo l_arTrackProcInfo[MAX_CHILDPROC];

void trackproc_start(uint32_t pid)
{
  int i;

  /* Process tracking running */
  l_iRunning = 1;

  /* Parent process being tracked */
  l_iNumProc = 1;

  /* Set parent information */
  l_arTrackProcInfo[0].m_uiPID = pid;
  l_arTrackProcInfo[0].m_uiHandle = -1;
  l_arTrackProcInfo[0].m_iParent = -1;

  /* Initialize children information */
  for (i = 1; i < MAX_CHILDPROC; i ++) {
    l_arTrackProcInfo[i].m_uiHandle = -1;
    l_arTrackProcInfo[i].m_uiPID = -1;
    l_arTrackProcInfo[i].m_iParent = -1;
  }

}

void trackproc_stop()
{
  l_iRunning = 0;
  l_iNumProc = 0;
}

uint32_t trackproc_get_root_pid()
{
  return (l_arTrackProcInfo[0].m_uiPID);
}

int trackproc_is_running()
{
  //fprintf(stderr, "in running... %d\n", l_iRunning);
  return (l_iRunning);
}

uint32_t trackproc_get_current_pid(void)
{
  return VMI_find_pid_by_cr3_c(cpu_single_env->cr[3]);
}


int trackproc_find_handle(uint32_t uiHandle)
{
  int i;
  for (i = 0; i < l_iNumProc; i ++) {
    if (l_arTrackProcInfo[i].m_uiHandle == uiHandle)
      return (i);
  }
  return (-1);
}

int trackproc_find_pid(uint32_t uiPID)
{
  int i;
  for (i = 0; i < l_iNumProc; i ++) {
    if (l_arTrackProcInfo[i].m_uiPID == uiPID)
      return (i);
  }
  return (-1);
}

int trackproc_add_new_handle(uint32_t uiHandle, uint32_t uiParentPID)
{
  /* Check that maximum number of children is not exceeded */
  if (l_iNumProc >= MAX_CHILDPROC) {
    monitor_printf(default_mon, "Maximum number of child processes exceeded, ignoring\n");
    return -1;
  }
  /* Check if the handle is already in the array */
  if (trackproc_find_handle(uiHandle) >= 0) {
    return -2;
  }
  l_arTrackProcInfo[l_iNumProc].m_uiHandle = uiHandle;
  l_arTrackProcInfo[l_iNumProc].m_uiPID = -1;

  int iParentIndex = trackproc_find_pid(uiParentPID);

  /* Check that parent process is in array */
  if (iParentIndex < 0) {
    return -3;
  }

  l_arTrackProcInfo[l_iNumProc].m_iParent = iParentIndex;

  /* Increase number of processes being tracked */
  l_iNumProc++;

  return 0;
}

void trackproc_set_pid(uint32_t uiHandle, uint32_t uiPID)
{
  int iPos;
  iPos = trackproc_find_handle(uiHandle);
  if (iPos == -1)
    return;

  l_arTrackProcInfo[iPos].m_uiPID = uiPID;
  //reset the handle value (it can be reused by another process)
  l_arTrackProcInfo[iPos].m_uiHandle = -1;

  monitor_printf(default_mon, "Process %u forked child %u (Handle: 0x%x)\n", 
    l_arTrackProcInfo[l_arTrackProcInfo[iPos].m_iParent].m_uiPID,
    uiPID,  uiHandle);

}

uint32_t trackproc_get_parent_pid(uint32_t uiPID)
{
  int iPos = trackproc_find_pid(uiPID);
  if (iPos == -1)
    return -1;
  return (l_arTrackProcInfo[l_arTrackProcInfo[iPos].m_iParent].m_uiPID);
}

unsigned int trackproc_found_child() {
  return (l_iNumProc > 1);
}

procinfo_t *trackproc_get_tracked_processes_info(
  uint32_t tracked_pid, uint32_t *num_tracked_proc)
{
  procinfo_t *proc_arr = NULL;
  size_t num_procs = 0, num_mods = 0;
  char proc_name[512];
  uint32_t cr3, curr_pid;
  unsigned int i;

  if (tracked_pid == -1) {
	num_procs = VMI_get_all_processes_count_c();
	proc_arr = (procinfo_t *) malloc(sizeof(procinfo_t*));
    VMI_find_all_processes_info_c(num_procs, proc_arr);
    *num_tracked_proc = num_procs;
  }
  else {
    proc_arr = (procinfo_t *) malloc(l_iNumProc * sizeof(procinfo_t));

    if (proc_arr) {
      for (i = 0; i < l_iNumProc; i++) {
	curr_pid = l_arTrackProcInfo[i].m_uiPID;
	VMI_find_process_by_pid_c(curr_pid, proc_name, 512,&cr3);
	num_mods = VMI_get_loaded_modules_count_c(curr_pid);
	if (cr3 != -1) {
	  proc_arr[i].pid = curr_pid;
	  proc_arr[i].cr3 = cr3;
	  proc_arr[i].n_mods = num_mods;
	  strncpy(proc_arr[i].name, proc_name, 511);
	  proc_arr[i].name[511] = '\0';
	}
	else {
	  proc_arr[i].pid = 0;
	  proc_arr[i].cr3 = -1;
	  proc_arr[i].n_mods = 0;
	  proc_arr[i].name[0] = '\0';
	}
      }
      *num_tracked_proc = l_iNumProc;
    }
  }

  return proc_arr;
}
