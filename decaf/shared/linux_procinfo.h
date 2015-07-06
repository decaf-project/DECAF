
#ifndef LINUX_PROCINFO_H_
#define LINUX_PROCINFO_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef target_ulong target_ptr;

/** Data structure that helps keep things organized. **/
typedef struct _ProcInfo
{
	char strName[32];
	target_ulong init_task_addr;
	target_ulong init_task_size;

	target_ulong ts_tasks;
	target_ulong ts_pid;
	target_ulong ts_tgid;
	target_ulong ts_group_leader;
	target_ulong ts_thread_group;
	target_ulong ts_real_parent;
	target_ulong ts_mm;
	union
	{
		target_ulong ts_stack;
		target_ulong ts_thread_info;
	};
	target_ulong ts_real_cred;
	target_ulong ts_cred;
	target_ulong ts_comm;
	target_ulong cred_uid;
	target_ulong cred_gid;
	target_ulong cred_euid;
	target_ulong cred_egid;
	target_ulong mm_mmap;
	target_ulong mm_pgd;
	target_ulong mm_arg_start;
	target_ulong mm_start_brk;
	target_ulong mm_brk;
	target_ulong mm_start_stack;
	target_ulong vma_vm_start;
	target_ulong vma_vm_end;
	target_ulong vma_vm_next;
	target_ulong vma_vm_file;
	target_ulong vma_vm_flags;
	target_ulong vma_vm_pgoff;
	target_ulong file_dentry;
	target_ulong dentry_d_name;
	target_ulong dentry_d_iname;
	target_ulong dentry_d_parent;
	target_ulong ti_task;
	target_ulong file_inode;
	target_ulong inode_ino;
	
	target_ulong proc_fork_connector;
	target_ulong proc_exit_connector;
	target_ulong proc_exec_connector;
	target_ulong vma_link;
	target_ulong remove_vma;
	target_ulong vma_adjust;
// #ifdef TARGET_MIPS
	target_ulong mips_pgd_current;
// #endif	
} ProcInfo;

// int populate_mm_struct_offsets(CPUState *env, target_ptr mm, ProcInfo* pPI);
// int populate_vm_area_struct_offsets(CPUState *env, target_ptr vma, ProcInfo* pPI);
// int populate_dentry_struct_offsets(CPUState * env, target_ptr dentry, ProcInfo* pPI);
// int getDentryFromFile(CPUState * env, target_ptr file, ProcInfo* pPI);
// //runs through the guest's memory and populates the offsets within the
// // ProcInfo data structure. Returns the number of elements/offsets found
// // or -1 if error
// int populate_kernel_offsets(CPUState *env, target_ptr threadinfo, ProcInfo* pPI);

int printProcInfo(ProcInfo* pPI);
int load_proc_info(CPUState * env, gva_t threadinfo, ProcInfo &pi);
void load_library_info(const char *strName);

#ifdef __cplusplus
};
#endif
#endif /* RECON_H_ */

