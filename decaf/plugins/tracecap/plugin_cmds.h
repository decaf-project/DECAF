
#ifdef CONFIG_TCG_TAINT
//NIC
{
	.name		= "taint_nic",
	.args_type	= "state:i",
	.mhandler.cmd	= do_taint_nic,
	.params		= "state",
	.help		= "set the network input to be tainted or not"
},
{
	.name		= "taint_nic_filter",
	.args_type	= "type:s,value:s",
	.mhandler.cmd	= (void (*)())update_nic_filter,
	.params		= "<clear|proto|sport|dport|src|dst> value",
	.help		= "Update filter for tainting NIC"
},
{
	.name		= "ignore_dns",
	.args_type	= "state:i",
	.mhandler.cmd	= set_ignore_dns,
	.params		= "state",
	.help		= "set flag to ignore received DNS packets"
},
{
	.name		= "filter_tainted_only",
	.args_type	= "state:i",
	.mhandler.cmd	= set_tainted_only,
	.params		= "state",
	.help		= "set flag to trace only tainted instructions"
},
{
	.name		= "filter_single_thread_only",
	.args_type	= "state:i",
	.mhandler.cmd	= set_single_thread_only,
	.params		= "state",
	.help		= "set flag to trace only instructions from the same thread as the first instruction"
},
{
	.name		= "filter_kernel_tainted",
	.args_type	= "state:i",
	.mhandler.cmd	= set_kernel_tainted,
	.params		= "state",
	.help		= "set flag to trace tainted kernel instructions in addition to user instructions"
},
//taint keystroke
{
	.name		= "taint_sendkey",
	.args_type	= "key:s",
	.mhandler.cmd	= do_taint_sendkey,
	.params		= "key taint_origin offset",
	.help		= "send a tainted key to the guest system"
},

#endif//CONFIG_TCG_TAINT


{
	.name		= "trace",
	.args_type	= "pid:i,filepath:F",
	.mhandler.cmd	= do_tracing,
	.params		= "pid filepath",
	.help		= "save the execution trace of a process into the specified file"
},
{
        .name           = "trace_kernel",
        .args_type      = "filepath:F",
        .mhandler.cmd   = set_trace_kernel,
        .params         = "filepath",
        .help           = "save the kernel execution trace  into the specified file"
},


{ 
	.name		= "tracebyname",
	.args_type	= "name:s,filepath:F", 
	.mhandler.cmd	= do_tracing_by_name,
	.params		= "name filepath",
	.help		= "save the execution trace of a process into the specified file"
},
{
	.name		= "trace_stop", 
	.args_type	= "", 
	.mhandler.info	= do_tracing_stop,
	.params		= "", 
	.help		= "stop tracing current process(es)"
},



