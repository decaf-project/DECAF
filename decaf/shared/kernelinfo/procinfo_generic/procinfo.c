
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/security.h>
#include <linux/kprobes.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/syscalls.h>
#include <linux/fs.h>
#include <linux/dcache.h>

// UTS_RELEASE may be defined in different files for different release versions
#include <generated/utsrelease.h>

#define OFFSET_OF(type, field) (  (unsigned long)&( ((struct type *)0)->field ) )
#define JPROBE_TOTAL 8

/* variables for hookpoints */
static struct jprobe jprobes[JPROBE_TOTAL];
static struct kretprobe my_kretprobe;

/* this will be called before do_fork finish */ //Keep
static void linuxdrv_finish_fork(struct task_struct * task)
{
    jprobe_return();
}

/* this will be called before do_exit finish */ //Keep
static void linuxdrv_finish_exit(struct task_struct * task)
{
    jprobe_return();
}

/* this will be called before do_execve finish */  //Keep
static void linuxdrv_finish_exec(struct task_struct * task)
{
    jprobe_return();
}

static void linuxdrv_insert_vma(struct mm_struct *mm, struct vm_area_struct *vma)
{
    jprobe_return();
}

/* this will be called when a vma will be adjusted */
static void linuxdrv_adjust_vma(struct vm_area_struct *const_vma, unsigned long const_start,
                unsigned long const_end, pgoff_t pgoff, struct vm_area_struct *insert)
{
    jprobe_return();
}

/* this will be called when a vma will be removed */
static void linuxdrv_remove_vma(struct vm_area_struct *vma)
{
    jprobe_return();
}

static int linuxdrv_init(void)
{
    int i;

    jprobes[0].kp.symbol_name = "proc_fork_connector";
    jprobes[0].entry = (void*)linuxdrv_finish_fork;
    jprobes[1].kp.symbol_name = "proc_exit_connector";
    jprobes[1].entry = (void*)linuxdrv_finish_exit;
    jprobes[2].kp.symbol_name = "proc_exec_connector";
    jprobes[2].entry = (void*)linuxdrv_finish_exec;
    jprobes[3].kp.symbol_name = "vma_link";
    jprobes[3].entry = (void*)linuxdrv_insert_vma;
    jprobes[4].kp.symbol_name = "vma_adjust";
    jprobes[4].entry = (void*)linuxdrv_adjust_vma;
    jprobes[5].kp.symbol_name = "remove_vma";
    jprobes[5].entry = (void*)linuxdrv_remove_vma;
    jprobes[6].kp.symbol_name = "modules";
    jprobes[6].entry = (void*)linuxdrv_remove_vma;
    jprobes[7].kp.symbol_name = "trim_init_extable";
    jprobes[7].entry = (void*)linuxdrv_remove_vma;


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

    printk(KERN_INFO
        "module_name   = %lu\n"
        "module_size   = %lu\n"
        "module_init   = %lu\n"
        "module_list   = %lu\n",
        OFFSET_OF(module, name),
        OFFSET_OF(module, core_size),
        OFFSET_OF(module, module_core),
        OFFSET_OF(module, list)
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
        "file_inode      = %lu\n" /* inode of file struct */
        "dentry_d_name   = %lu\n" /* offset of d_name in dentry */
        "dentry_d_iname  = %lu\n" /* offset of d_iname in dentry */
        "dentry_d_parent = %lu\n" /* offset of d_parent in dentry */
        "ti_task         = %lu\n" /* offset of task in thread_info */
        "inode_ino   = %lu\n", /* offset of inode index in inode struct */
        OFFSET_OF(file, f_dentry),
        OFFSET_OF(dentry, d_inode),
        OFFSET_OF(dentry, d_name),
        OFFSET_OF(dentry, d_iname),
        OFFSET_OF(dentry, d_parent),
        OFFSET_OF(thread_info, task),
        OFFSET_OF(inode,i_ino)
        );


    for(i = 0; i < JPROBE_TOTAL; i++) {
        register_jprobe(&jprobes[i]);
    }

    for(i = 0; i < JPROBE_TOTAL; i++)  {
        if(jprobes[i].kp.addr != NULL)  {
            printk(KERN_INFO
            "%s  = %lu\n",
            jprobes[i].kp.symbol_name,
            jprobes[i].kp.addr);
        }
    }

    for(i = 0; i < JPROBE_TOTAL; i++) {
        unregister_jprobe(&jprobes[i]);
    }
    return -1;
}

static void linuxdrv_exit(void)
{
    printk(KERN_INFO "linuxdrv: module removed.\n");
}

module_init(linuxdrv_init);
module_exit(linuxdrv_exit);

MODULE_DESCRIPTION("Provide task information to temu.");
MODULE_LICENSE("GPL");

