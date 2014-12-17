
{
	.name		= "taint_sendkey",
	.args_type	= "key:s",
	.mhandler.cmd	= do_taint_sendkey,
	.params		= "key taint_origin offset",
	.help		= "send a tainted key to the guest system"
},
{
		.name = "enable_keylogger_check",
		.args_type = "tracefile:F",
		.mhandler.cmd = do_enable_keylogger_check,
		.params = "trace_file name",
		.help = "check every tainted instruction to see what module it belongs to "
},
{
		.name = "disable_keylogger_check",
		.args_type = "",
		.mhandler.cmd = do_disable_keylogger_check,
		.params = "no params",
		.help = "disable function that check every tainted instruction to see what module it belongs to "
},
