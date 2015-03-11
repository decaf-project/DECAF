
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/security.h>
#include <linux/kprobes.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include "linuxdrv.h"

/* variables for hookpoints */
static struct jprobe jprobes[JPROBE_SUM];
static struct task_struct * task_search_cache = NULL;

/* this function will send a message to temu */
static void linuxdrv_send_message(char *fmt, ...)
{
    char message[MESSAGE_MAX];
    
    va_list argl;
    va_start(argl, fmt);
    vsprintf(message, fmt, argl);
    va_end(argl);

    printk(KERN_INFO "%s", message);
    
    /* send message to temu by writing bytes to the virtual port */
    __asm__ __volatile__(
    "cld              \n"
    "1:           \n"
    "lodsb            \n"
    "outb %%al,$"VIRTUAL_PORT"\n"
    "testb %%al,%%al      \n"
    "jnz 1b           \n"
    ::"S"(message):"%eax");
}

/* get physical address of task page directory */
static unsigned int linuxdrv_get_mem_cr3(struct mm_struct * mm)
{
    return mm ? ((unsigned int)(mm->pgd) - PAGE_OFFSET) : -PAGE_OFFSET;
}

/* search task_struct by memory page directory address */
static unsigned int linuxdrv_pgd_to_pid(struct mm_struct * mm)
{
#ifndef CONFIG_MM_OWNER
    int list_offset = (int)&((struct task_struct *)0)->tasks;
    
    if(task_search_cache && task_search_cache->mm == mm)
    return task_search_cache->tgid;
    if(current->mm == mm)
    return (task_search_cache = current)->tgid;
    for(task_search_cache = &init_task; (task_search_cache = (void *)((int)(task_search_cache->tasks.prev) - list_offset)) != &init_task; )
    if(task_search_cache->mm == mm)
        return task_search_cache->tgid;
    return (task_search_cache = current)->tgid;
#else
    return mm->owner->tgid;
#endif
}

/* notify temu to update it's mmap information */
static void linuxdrv_vma_update(struct vm_area_struct *vma, unsigned long new_start, unsigned long new_end, enum vma_state state)
{
    char *name, *null = "[]";
    unsigned int cr3 = linuxdrv_get_mem_cr3(vma->vm_mm);
    unsigned int owner = linuxdrv_pgd_to_pid(vma->vm_mm);
    
    if(!vma->vm_file || !vma->vm_file->f_dentry || !(name = vma->vm_file->f_dentry->d_iname)) {
    name = null;
    }
    if(VMA_CREATE == state) {
    new_start = vma->vm_start;
    new_end = vma->vm_end;
    }
    
    /* send message to temu to notify vma removed */
    if(VMA_REMOVE == state || VMA_MODIFY == state){
    linuxdrv_send_message("M %d %x \"%s\" %x %x \"%s\" %c \n",
                  owner,
                  cr3,
                  name,
                  vma->vm_start,
                  vma->vm_end - vma->vm_start,
                  name,
                  '-');
    }
    /* send message to temu to notify vma created */
    if(VMA_CREATE == state || VMA_MODIFY == state) {
    linuxdrv_send_message("M %d %x \"%s\" %x %x \"%s\" %c \n",
                  owner,
                  cr3,
                  name,
                  new_start,
                  new_end - new_start,
                  name,
                  '+');
    }
}

/* traversing virtual memory area of task */
static void linuxdrv_task_vma_traversing(struct task_struct *task)
{
    struct vm_area_struct * vma;

    if(!task->mm || !(vma = task->mm->mmap))
    return;
    do {
    linuxdrv_vma_update(vma, 0, 0, VMA_CREATE);
    } while((vma = vma->vm_next));
}

/* notify temu to update it's task information */
static void linuxdrv_task_update(struct task_struct *task, enum task_state state)
{
    /* send message to temu to notify new task exiting */
    if(TASK_EXIT == state || TASK_EXEC == state)
    linuxdrv_send_message("P - %d \n", task->tgid);
    /* send message to temu to notify new task creatation */
    if(TASK_FORK == state || TASK_EXEC == state) {
    linuxdrv_send_message("P + %d -1 %08x %s \n", task->tgid, linuxdrv_get_mem_cr3(task->mm), task->comm);
    linuxdrv_task_vma_traversing(task);
    }
}

/* traversing all tasks in linux right now */
static void linuxdrv_task_traversing(void)
{
    struct task_struct * task = &init_task;
    
    int list_offset = (int)&((struct task_struct *)0)->tasks;
        
    /* tell temu to update it's default kernel task */
    linuxdrv_task_update(task, TASK_EXEC);
    
    while((task = (void *)((int)(task->tasks.next) - list_offset)) != &init_task)
    linuxdrv_task_update(task, TASK_FORK);
}

/* this will be called before do_fork finish */
static void linuxdrv_finish_fork(struct task_struct * task)
{
    if(task != task->group_leader)
    jprobe_return();
    linuxdrv_task_update(task, TASK_FORK);  
    jprobe_return();
}

/* this will be called before do_exit finish */
static void linuxdrv_finish_exit(struct task_struct * task)
{
    if(task != task->group_leader)
    jprobe_return();
    linuxdrv_task_update(task, TASK_EXIT);

    if(task == task_search_cache)
    task_search_cache = NULL;
    jprobe_return();
}

/* this will be called before do_execve finish */
static void linuxdrv_finish_exec(struct task_struct * task)
{
    linuxdrv_task_update(task, TASK_EXEC);
    jprobe_return();
}

/* this will be called when a vma will be linked */
static void linuxdrv_insert_vma(struct mm_struct *mm, struct vm_area_struct *vma)
{
    linuxdrv_vma_update(vma, 0, 0, VMA_CREATE);
    jprobe_return();
}

/* this will be called when a vma will be removed */
static void linuxdrv_remove_vma(struct vm_area_struct *vma)
{
    linuxdrv_vma_update(vma, 0, 0, VMA_REMOVE);
    jprobe_return();
}

/* this will be called when a vma will be adjusted */
static void linuxdrv_adjust_vma(struct vm_area_struct *const_vma, unsigned long const_start,
                unsigned long const_end, pgoff_t pgoff, struct vm_area_struct *insert)
{
    int adjust_next = 0, remove_next = 0;
    unsigned long start = const_start, end = const_end;
    struct vm_area_struct *vma = const_vma, *next = vma->vm_next;

    /*
     * it is very hard for us looking into the original function,
     * so we create a function behaves completely the same to guess
     * the final result
     */
    
    if(next && !insert) {
    if (end >= next->vm_end) {
    again:          remove_next = 1 + (end > next->vm_end);
        end = next->vm_end;
    } else if (end > next->vm_start) {
        adjust_next = (end - next->vm_start) >> PAGE_SHIFT;
    } else if (end < vma->vm_end) {
        adjust_next = - ((vma->vm_end - end) >> PAGE_SHIFT);
    }
    }

    linuxdrv_vma_update(vma, start, end, VMA_MODIFY);
    
    if(adjust_next) {
    linuxdrv_vma_update(next, next->vm_start + (adjust_next << PAGE_SHIFT), next->vm_end, VMA_MODIFY);
    }
    if(remove_next) {
    linuxdrv_vma_update(next, 0, 0, VMA_REMOVE);
    if (remove_next == 2) {
        next = vma->vm_next->vm_next;
        goto again;
    }
    }
    
    jprobe_return();
}

/* this will be called when stack vma need to expand */
static void linuxdrv_expand_stack(struct vm_area_struct *vma, unsigned long const_address)
{
    unsigned long address = const_address & PAGE_MASK;
    
    /*
     * Note: this stack must grow downwards. We also assume the expansion is legal.
     * In other word, this operation may fail, but the failure will cause current task
     * terminated by SIGSEGV signal. Will we run into any problem ?
     */

    if(address < vma->vm_start)
    linuxdrv_vma_update(vma, address, vma->vm_end, VMA_MODIFY);
    jprobe_return();
}

static int linuxdrv_init(void)
{
    int i;
    int error;
    
    jprobes[0].kp.symbol_name = "proc_fork_connector";
    jprobes[0].entry = (void*)linuxdrv_finish_fork;
    jprobes[1].kp.symbol_name = "proc_exit_connector";
    jprobes[1].entry = (void*)linuxdrv_finish_exit;
    jprobes[2].kp.symbol_name = "proc_exec_connector";
    jprobes[2].entry = (void*)linuxdrv_finish_exec;
    jprobes[3].kp.symbol_name = "__vma_link";
    jprobes[3].entry = (void*)linuxdrv_insert_vma;
    jprobes[4].kp.symbol_name = "remove_vma";
    jprobes[4].entry = (void*)linuxdrv_remove_vma;
    jprobes[5].kp.symbol_name = "vma_adjust";
    jprobes[5].entry = (void*)linuxdrv_adjust_vma;
    jprobes[6].kp.symbol_name = "expand_stack";
    jprobes[6].entry = (void*)linuxdrv_expand_stack;

    for(i = 0, error = 0; i < JPROBE_SUM; i++) {
    if(error || register_jprobe(&jprobes[i])) {
        if(error) {
        unregister_jprobe(&jprobes[JPROBE_SUM-i-1]);
        continue;
        }
        i = JPROBE_SUM-i-1;
        error = 1;
    }
    }
    
    if(error) {
    printk("linuxdrv: fail to register jprobes.\n");
    goto out;
    }
    
    linuxdrv_task_traversing();

    printk(KERN_INFO "linuxdrv: module initialized.\n");
    
    return 0;
out:    
    return -1;
}

static void linuxdrv_exit(void)
{
    int i;

    for(i = 0; i < JPROBE_SUM; i++) {
    unregister_jprobe(&jprobes[i]);
    }
    printk(KERN_INFO "linuxdrv: module removed.\n");
}

module_init(linuxdrv_init);
module_exit(linuxdrv_exit);

MODULE_DESCRIPTION("Provide task information to temu.");
MODULE_LICENSE("GPL");

