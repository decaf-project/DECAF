#ifndef __ANALYSIS_LOG_H__
#define __ANALYSIS_LOG_H__

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* This is the datatype for every analysis log entry.  Not every log
  entry type uses all of these fields, but by using a fixed record
  size for each log entry, maintenance of the log becomes easier. */
typedef struct {
  uint32_t op;
  uint64_t arg[5];
} analysis_log_entry_t;

/* When a new block begins, any pending taint events from the previous 
  block are evaluated prior to being written to disk.  If the taint events
  eventually land in some concrete register or a qemu_ld/st, they are
  logged to disk.  Otherwise, the taint events only impact temps, so they
  don't need to be logged.  This function determines which events to keep
  and logs them to disk. */
extern void DECAF_block_begin_for_analysis(void);

/* When the MMU resolves a virtual address to a physical one, it gets logged */
extern void DECAF_resolved_phys_addr(uint32_t addr);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __ANALYSIS_LOG_H__ */

