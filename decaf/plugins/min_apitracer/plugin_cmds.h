/*
 * plugin_cmds.h
 *
 *  Created on: Jun 8, 2012
 *      Author: aravind
 */
{
	.name		= "trace_stop",
	.args_type	= "",
	.mhandler.cmd	= do_tracing_stop,
	.params		= "",
	.help		= "stop tracing all traced process(es)",
},
{
		.name 	= "trace_by_name",
		.args_type = "proc_name:s,tracefile:F,configfile:F",
		.mhandler.cmd = do_trace_by_name,
		.params = "process_name trace_file_name config_file_name",
		.help = "Trace by name of the process. The process must not have started yet. The trace is saved to the said file. Configuration is parsed from the configfile",
},
