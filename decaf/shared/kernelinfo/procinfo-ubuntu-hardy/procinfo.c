#include <linux/module.h>	/* Needed by all modules */
#include <linux/kernel.h>	/* Needed for KERN_INFO */
#include <linux/version.h>
#include <linux/utsrelease.h>
#include <linux/syscalls.h>
#include <linux/security.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/dcache.h>

int init_module(void)
{
	struct vm_area_struct vma; 
	struct file filestruct; 
	struct dentry dentrystr; 
    /* 
     * A non 0 return means init_module failed; module can't be loaded. 
     */
    printk(KERN_INFO 
	   "    {  \"%s\", /* entry name */\n"
	   "       0x%08lX, 0x%08lX, /* hooking address */\n"
	   "       0x%08lX, /* task struct root */\n"
	   "       %d, /* size of task_struct */\n"
	   "       %d, /* offset of task_struct list */\n"
	   "       %d, /* offset of pid */\n"
	   "       %d, /* offset of mm */\n"
	   "       %d, /* offset of pgd in mm */\n"
	   "       %d, /* offset of comm */\n"
	   "       %d, /* size of comm */\n"
	   "       %d, /* offset of vm_start in vma */\n"
	   "       %d, /* offset of vm_end in vma */\n"
	   "       %d, /* offset of vm_next in vma */\n"
	   "       %d, /* offset of vm_file in vma */\n"
	   "       %d, /* offset of dentry in file */\n"
	   "       %d, /* offset of d_name in dentry */\n"
	   "       %d, /* offset of d_iname in dentry */ \n"
           "       0x00000000 /* hooking address: force_sig_info */ \n"
	   "    },\n",
	   UTS_RELEASE, 
	   /* Many of the functions we'd like to hook aren't exported,
	      so just look them up in /boot/System.map by hand. */
	   (long)0, (long)0,
	   (long)&init_task, 
	   sizeof(init_task),
	   (int)&init_task.tasks - (int)&init_task,
	   (int)&init_task.pid - (int)&init_task, 
	   (int)&init_task.mm - (int)&init_task,
	   (int)&init_task.mm->pgd - (int)init_task.mm,
           (int)&init_task.comm - (int)&init_task,
	   sizeof(init_task.comm),
           (int)&vma.vm_start - (int)&vma,
           (int)&vma.vm_end - (int)&vma,
           (int)&vma.vm_next - (int)&vma,
           (int)&vma.vm_file - (int)&vma,
	   (int)&filestruct.f_dentry - (int)&filestruct,
	   (int)&dentrystr.d_name - (int)&dentrystr,
	   (int)&dentrystr.d_iname - (int)&dentrystr
	);
    

    printk(KERN_INFO "Information module retistered.\n");
    return -1;
}

void cleanup_module(void)
{
    
    printk(KERN_INFO "Information module removed.\n");
}

MODULE_LICENSE("GPL"); 
