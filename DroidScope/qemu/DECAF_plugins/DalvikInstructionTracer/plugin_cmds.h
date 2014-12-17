{
  .name         = "trace_by_pid",
  .args_type    = _QEMU_MON_KEY_VALUE("pid","i"),
  ._QEMU_MON_HANDLER_CMD = do_trace_by_pid,
  .params       = "[pid]",
  .help	        = "Start tracing process with PID"
},
