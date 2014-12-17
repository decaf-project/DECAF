/* Copyright (C) 2006-2007 The Android Open Source Project
**
** This software is licensed under the terms of the GNU General Public
** License version 2, as published by the Free Software Foundation, and
** may be copied, distributed, and modified under those terms.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
*/

#ifndef TRACE_H
#define TRACE_H

#include <inttypes.h>
#include "android-trace_common.h"

extern uint64_t start_time, end_time;
extern uint64_t elapsed_usecs;
extern uint64_t Now();

struct TranslationBlock;

// The simulated time, in clock ticks, starting with one.
extern uint64_t sim_time;
extern uint64_t trace_static_bb_num(void);;

// This variable == 1 if we are currently tracing, otherwise == 0.
extern int tracing;
extern int trace_all_addr;
extern int trace_cache_miss;

extern void start_tracing();
extern void stop_tracing();
extern void trace_init(const char *filename);
extern void trace_bb_start(uint32_t bb_addr);
extern void trace_add_insn(uint32_t insn, int is_thumb);
extern void trace_bb_end();

extern int get_insn_ticks_arm(uint32_t insn);
extern int get_insn_ticks_thumb(uint32_t  insn);

extern void trace_exception(uint32_t pc);
extern void trace_bb_helper(uint64_t bb_num, struct TranslationBlock *tb);
extern void trace_insn_helper();
extern void sim_dcache_load(uint32_t addr);
extern void sim_dcache_store(uint32_t addr, uint32_t val);
extern void sim_dcache_swp(uint32_t addr);
extern void trace_interpreted_method(uint32_t addr, int call_type);

extern const char *trace_filename;
extern int tracing;
extern int trace_cache_miss;
extern int trace_all_addr;

// Trace process/thread operations
extern void trace_switch(int pid);
extern void trace_fork(int tgid, int pid);
extern void trace_clone(int tgid, int pid);
extern void trace_exit(int exitcode);
extern void trace_name(char *name);

#endif /* TRACE_H */
