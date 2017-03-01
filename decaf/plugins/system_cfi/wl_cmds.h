/*
 * wl_cmds.h
 *
 *  Created on: Apr 27, 2012
 *      Author: aravind
 */

#ifndef WL_CMDS_H_
#define WL_CMDS_H_

{
		.name = "set_wl_dir",
		.args_type = "whitelist_directory:F",
		.mhandler.cmd = do_set_wl_dir,
		.params = "whitelist_directory",
		.help = "load whitelist information from this directory"
},
{
		.name = "set_guest_drive",
		.args_type = "guest_drive:F",
		.mhandler.cmd = do_set_guest_dir,
		.params = "guest_directory",
		.help = "Set the location on host where guest fs is mapped"
},
{
		.name = "monitor_proc",
		.args_type = "proc_name:s",
		.mhandler.cmd = do_monitor_proc,
		.params = "proc_name",
		.help = "monitor call stack for the proc with proc name"
},
{
	.name		= "print_tid",
	.args_type	= "",
	.mhandler.cmd	= do_print_tid,
	.params		= "",
	.help		= "Print current thread id"
},
{
		.name  = "enforce_kernel_cfi",
		.args_type = "option:b",
		.params = "on|off",
		.help = "Set or reset kernel mode cfi enforcement",
		.mhandler.cmd = do_set_kernel_cfi,
},
{
		.name  = "dump_system_dyn_regions",
		.args_type = "",
		.params = "",
		.help = "dump the dynamic regions for the system",
		.mhandler.cmd = do_dump_system_dyn_regions,
},
{
		.name  = "dump_file_wl",
		.args_type = "",
		.params = "",
		.help = "dump the whitelists corresponding to files",
		.mhandler.cmd = do_dump_file_wl,
},
{
		.name  = "pslist",
		.args_type = "",
		.params = "",
		.help = "pslist from cr3 hashmap",
		.mhandler.cmd = do_pslist_from_hashmap,
},

#endif /* WL_CMDS_H_ */
