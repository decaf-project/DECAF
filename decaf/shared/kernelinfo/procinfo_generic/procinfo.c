#include <linux/module.h>	/* Needed by all modules */
#include <linux/kernel.h>	/* Needed for KERN_INFO */
#include <linux/version.h>
#include <linux/syscalls.h>
#include <linux/security.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/dcache.h>
// UTS_RELEASE may be defined in different files for different release versions
#include <generated/utsrelease.h>

#define OFFSET_OF(type, field) (  (unsigned long)&( ((struct type *)0)->field ) )


int init_module(void)
{
    /* 
     * A non 0 return means init_module failed; module can't be loaded. 
     */
    printk(KERN_INFO 
		"strName = %s\n" /* entry name */
		"init_task_addr  = %lu\n" /* address of init_task */
		"init_task_size  = %lu\n" /* size of task_struct */
		"ts_tasks        = %lu\n" /* offset of tasks */
		"ts_pid          = %lu\n" /* offset of pid */
		"ts_tgid         = %lu\n" /* offset of tgid */
		"ts_group_leader = %lu\n" /* offset of group_leader */
		"ts_thread_group = %lu\n" /* offset of thread_group */
		"ts_real_parent  = %lu\n" /* offset of real_parent */
		"ts_mm           = %lu\n" /* offset of mm */
		"ts_stack        = %lu\n", /* offset of stack */
		UTS_RELEASE,
		(unsigned long)&init_task, 
		(unsigned long)sizeof(init_task),
		OFFSET_OF(task_struct, tasks),
		OFFSET_OF(task_struct, pid), 
		OFFSET_OF(task_struct, tgid),
		OFFSET_OF(task_struct, group_leader),
		OFFSET_OF(task_struct, thread_group),
		OFFSET_OF(task_struct, real_parent),
		OFFSET_OF(task_struct, mm),
		OFFSET_OF(task_struct, stack)  // TODO: not sure which field of the union should be filled
	);

    // cred, real_cred and related fields may not exist in Linux kernel 2.6
	printk(KERN_INFO
		"ts_real_cred    = %lu\n" /* offset of real_cred */
		"ts_cred         = %lu\n" /* offset of cred */
		"ts_comm         = %lu\n" /* offset of comm */
		"cred_uid        = %lu\n" /* offset of uid in cred */
		"cred_gid        = %lu\n" /* offset of gid in cred */
		"cred_euid       = %lu\n" /* offset of euid in cred */
		"cred_egid       = %lu\n", /* offset of egid in cred */
		OFFSET_OF(task_struct, real_cred),
		OFFSET_OF(task_struct, cred),
		OFFSET_OF(task_struct, comm),
		OFFSET_OF(cred, uid),
		OFFSET_OF(cred, gid),
		OFFSET_OF(cred, euid),
		OFFSET_OF(cred, egid)
		);

	printk(KERN_INFO 
		"mm_mmap         = %lu\n" /* offset of mmap in mm_struct */
		"mm_pgd          = %lu\n" /* offset of pgd in mm_struct */
		"mm_arg_start    = %lu\n" /* offset of arg_start in mm_struct */
		"mm_start_brk    = %lu\n" /* offset of start_brk in mm_struct */
		"mm_brk          = %lu\n" /* offset of brk in mm_struct */
		"mm_start_stack  = %lu\n", /* offset of start_stack in mm_struct */
		OFFSET_OF(mm_struct, mmap),
		OFFSET_OF(mm_struct, pgd),
		OFFSET_OF(mm_struct, arg_start),
		OFFSET_OF(mm_struct, start_brk),
		OFFSET_OF(mm_struct, brk),
		OFFSET_OF(mm_struct, start_stack)
		);

	printk(KERN_INFO
		"vma_vm_start    = %lu\n" /* offset of vm_start in vm_area_struct */
		"vma_vm_end      = %lu\n" /* offset of vm_end in vm_area_struct */
		"vma_vm_next     = %lu\n" /* offset of vm_next in vm_area_struct */
		"vma_vm_file     = %lu\n" /* offset of vm_file in vm_area_struct */
		"vma_vm_flags    = %lu\n" /* offset of vm_flags in vm_area_struct */
		"vma_vm_pgoff    = %lu\n", /* offset of vm_pgoff in vm_area_struct */
		OFFSET_OF(vm_area_struct, vm_start),
		OFFSET_OF(vm_area_struct, vm_end),
		OFFSET_OF(vm_area_struct, vm_next),
		OFFSET_OF(vm_area_struct, vm_file),
		OFFSET_OF(vm_area_struct, vm_flags),
		OFFSET_OF(vm_area_struct, vm_pgoff)
		);

	printk(KERN_INFO
		"file_dentry     = %lu\n" /* offset of f_dentry in file */
		"file_inode   	 = %lu\n" /* inode of file struct */
		"dentry_d_name   = %lu\n" /* offset of d_name in dentry */
		"dentry_d_iname  = %lu\n" /* offset of d_iname in dentry */ 
		"dentry_d_parent = %lu\n" /* offset of d_parent in dentry */
		"ti_task         = %lu\n" /* offset of task in thread_info */
		"inode_ino	     = %lu\n", /* offset of inode index in inode struct */
		OFFSET_OF(file, f_dentry),
		OFFSET_OF(dentry, d_inode),
		OFFSET_OF(dentry, d_name),
		OFFSET_OF(dentry, d_iname),
		OFFSET_OF(dentry, d_parent),
		OFFSET_OF(thread_info, task),
		OFFSET_OF(inode,i_ino)
		);
    

    printk(KERN_INFO "Information module registered.\n");
    return -1;
}

void cleanup_module(void)
{
    
    printk(KERN_INFO "Information module removed.\n");
}

MODULE_LICENSE("GPL"); 
