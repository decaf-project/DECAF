#ifndef UNPACKER_H_INCLUDED
#define UNPACKER_H_INCLUDED

typedef struct _taint_record {
//	uint32_t 	origin;
//	char 		mod[32];
//	char		proc[32];
//	uint32_t 	offset; 
	char 		version;
	char		dumpped;
//	char	    tl_count;
//	char		new_id;
//	uint32_t	source_id;
}taint_record_t;

#if 0
void do_set_max_unpack_rounds(int rounds);
void do_taint_sendkey(const char *string, int id);
void do_taint_nic(int state);
void do_taint_file(const char *filename, int dev_index, uint32_t taint_id);
void do_linux_ps();
void do_stop_unpack();
#endif

//change to decaf interface as
void do_set_max_unpack_rounds(Monitor *mon, const QDict *qdict);
void do_trace_process(Monitor *mon, const QDict *qdict);
void do_stop_unpack(Monitor *mon, const QDict *qdict);
void do_linux_ps(Monitor *mon, const QDict *qdict);
void do_guest_procs(Monitor *mon, const QDict *qdict);
//end change

#endif
