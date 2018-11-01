# Interfaces

DECAF provides many event-driven interfaces to make instrumentation easier. There are two kinds of interfaces.The first is callback for you to insert instrumentation code at specific event. Another set of interfaces is used as utils to get information of guest os or read memory from guest OS.

## Callback interfaces

To get a better understanding of these interfaces, a basic understanding of QEMU translation part is needed,please see [qemu](http://wiki.qemu.org/Manual) for more details.

### Callback types

#### VMI callback

It’s defined in /shared/vmi_callback.h: VMI_callback_type_t

| VMI Callback Type     | Description                                |
| --------------------- | ------------------------------------------ |
| VMI_CREATEPROC_CB     | Invoked when eprocess structure is created |
| VMI_REMOVEPROC_CB     | Invoked when delete the process            |
| VMI_LOADMODULE_CB     | Invoked when dll or driver is loaded       |
| VMI_LOADMAINMODULE_CB | Invoked when process starts to run         |
| VMI_PROCESSBEGIN_CB   | alias of VMI_LOADMAINMODULE_CB             |

#### Instruction callback

It’s defined in /shared/DECAF_callback_common.h:DECAF_callback_type_t. This callback instruments guest operating system at instruction level.

| Instruction Callback Type | Description                                 |
| ------------------------- | ------------------------------------------- |
| DECAF_BLOCK_BEGIN_CB      | Invoked at the start of the block           |
| DECAF_BLOCK_END_CB        | Invoked at the end of the block             |
| DECAF_INSN_BEGIN_CB       | Invoked before this instruction is executed |
| DECAF_INSN_END_CB         | Invoked after this instruction is executed  |

#### Mem read/write callback

It’s defined in /shared/DECAF_callback_common.h:DECAF_callback_type_t.

| Mem Read/Write Callback Type | Description                                           |
| ---------------------------- | ----------------------------------------------------- |
| DECAF_MEM_READ_CB            | Invoked when the memory read operation has been done  |
| DECAF_MEM_WRITE_CB           | Invoked when the memory write operation has been done |

#### Keystroke Callback

It’s defined in /shared/DECAF_callback_common.h:DECAF_callback_type_t.

| Keystroke Callback Type | Description                                          |
| ----------------------- | ---------------------------------------------------- |
| DECAF_KEYSTROKE_CB      | Invoked when system read a keystroke from ps2 driver |

#### EIP Check callback

It’s defined in /shared/DECAF_callback_common.h:DECAF_callback_type_t.

| EIP Check Callback Type | Description                                                  |
| ----------------------- | ------------------------------------------------------------ |
| DECAF_EIP_CHECK_CB      | For every function call, it will invoked this callback before it jump to target function specified by EIP. This is used to check if this EIP is tainted when you specify some taint source. If it’s tainted, it usually means exploit happens |

#### Network Callback

 It’s defined in /shared/DECAF_callback_common.h:DECAF_callback_type_t.

| Network Callback Type | Description                             |
| --------------------- | --------------------------------------- |
| DECAF_NIC_REC_CB      | Invoked when network card receives data |
| DECAF_NIC_SEND_CB     | Invoked when network card sends data    |



### Callback Register/Unregister Function

To use above callback, you should firstly register callback. Before you unload your plugin, please make sure this callback is unregistered. Or it may cause unexpected crash.

To register/unregister VMI callback types, you should use the following code:

```c
DECAF_Handle 
VMI_register_callback(procmod_callback_type_t cb_type, 
                      procmod_callback_func_t cb_func, 
                      int *cb_cond );

int 
VMI_unregister_callback(procmod_callback_type_t cb_type,
                        DECAF_Handle handle); 
```

To register/unregister other callback types, you should use the following code:

```c
DECAF_Handle 
DECAF_register_callback(DECAF_callback_type_t cb_type, 
                        DECAF_callback_func_t cb_func, 
                        int *cb_cond );

int 
DECAF_unregister_callback(DECAF_callback_type_t cb_type,
                          DECAF_Handle handle);
```

For block begin/end callback ,you can also use the following for a high performance.

```c
DECAF_Handle 
DECAF_registerOptimizedBlockBeginCallback(DECAF_callback_func_t cb_func, 
                                          int *cb_cond, 
                                          gva_t addr, 
                                          OCB_t type);

DECAF_Handle 
DECAF_registerOptimizedBlockEndCallback(DECAF_callback_func_t cb_func,
                                        int *cb_cond, 
                                        gva_t from, 
                                        gva_t to);

int 
DECAF_unregisterOptimizedBlockBeginCallback(DECAF_Handle handle);

int 
DECAF_unregisterOptimizedBlockEndCallback(DECAF_Handle handle);
```



### API Hook Interfaces

DECAF provides interfaces to hook any api in guest os. You can find the detailed description of these interfaces in `/path/to/decaf/shared/hookapi.h`.

```c
uintptr_t 
hookapi_hook_function_byname(const char *mod,
                             const char *func,
                             int is_global,
                             target_ulong cr3,
                             hook_proc_t fnhook,
                             void *opaque,
                             uint32_t sizeof_opaque);

void 
hookapi_remove_hook(uintptr_t handle);

uintptr_t 
hookapi_hook_return(target_ulong pc,
                    hook_proc_t fnhook, 
                    void *opaque, 
                    uint32_t sizeof_opaque);

uintptr_t 
hookapi_hook_function(int is_global, 
                      target_ulong pc, 
                      target_ulong cr3, 
                      hook_proc_t fnhook, 
                      void *opaque, 
                      uint32_t sizeof_opaque );
```

## Utils

As you see in the [sample plugins](https://github.com/sycurelab/DECAF/tree/master/decaf/plugins), you registered a `DECAF_INSN_BEGIN_CB` callback, but what can you do in your handler `my_insn_begin_callback` ? The utils interfaces is supposed to help you get more about guest os. You can find these definitions in `DECAF/shared/DECAF_main.c/h` or `DECAF/shared/vmi.h/vmi_c_wrapper.h`. Now, We make a short table for these utils.

1. Utils for read/write memory from/to guest OS

```c
 /*given a virtual address of guest os, get the corresponded physical address */ 
gpa_t 
DECAF_get_phys_addr(CPUState env, 
                    gva_t addr);

DECAF_errno_t 
DECAF_memory_rw(CPUState* env, 
                uint32_t addr, 
                void *buf,
                int len,
                int is_write);

DECAF_errno_t 
DECAF_memory_rw_with_cr3(CPUState* env, 
                         target_ulong cr3,
                         gva_t addr, 
                         void *buf, 
                         int len, 
                         int is_write);

DECAF_errno_t
DECAF_read_mem(CPUState* env,
               gva_t vaddr, 
               int len, 
               void *buf);

DECAF_errno_t 
DECAF_write_mem(CPUState* env, 
                gva_t vaddr, 
                int len, 
                void *buf);

DECAF_errno_t 
DECAF_read_mem_with_cr3(CPUState* env, 
                        target_ulong cr3,
                        gva_t vaddr, 
                        int len, 
                        void *buf);

DECAF_errno_t 
DECAF_write_mem_with_cr3(CPUState* env, 
                         target_ulong cr3,
                         gva_t vaddr, 
                         int len, 
                         void *buf);

int 
DECAF_get_page_access(CPUState* env, // this functin is in ./target-i386/DECAF_target.c
                      gva_t addr); 


```



### C++ Interfaces

```c++
module* 
VMI_find_module_by_pc(target_ulong pc,
                      target_ulong pgd,
                      target_ulong *base);

module* 
VMI_find_module_by_name(const char *name,
                        target_ulong pgd,
                        target_ulong *base);

module* 
VMI_find_module_by_base(target_ulong pgd,
                        uint32_t base);

process* 
VMI_find_process_by_pid(uint32_t pid);

process* 
VMI_find_process_by_pgd(uint32_t pgd);

process* 
VMI_find_process_by_name(char *name); 
```

### C Interfaces

```c
/// @ingroup semantics 
/// locate the module that a given instruction belongs to 
/// @param eip virtual address of a given instruction 
/// @param cr3 memory space id: physical address of page table 
/// @param proc process name (output argument) 
/// @param tm return tmodinfo_t structure 
extern int 
VMI_locate_module_c(gva_t eip, 
                    gva_t cr3, 
                    char proc[],
                    tmodinfo_t *tm);

extern int 
checkcr3(uint32_t cr3,
         uint32_t eip, 
         uint32_t tracepid, 
         char *name, 
         int len, 
         uint32_t * offset);

extern int 
VMI_locate_module_byname_c(const char *name,
                           uint32_t pid,tmodinfo_t * tm);

extern int 
VMI_find_cr3_by_pid_c(uint32_t pid);

extern int 
VMI_find_pid_by_cr3_c(uint32_t cr3);

extern int 
VMI_find_pid_by_name_c(char* proc_name);

/// @ingroup semantics 
/// find process given a memory space id 
/// @param cr3 memory space id: physical address of page table 
/// @param proc process name (output argument) 
/// @param pid process pid (output argument) 
/// @return number of modules in this process 

extern int
VMI_find_process_by_cr3_c(uint32_t cr3,
                          char proc_name[], 
                          size_t len, 
                          uint32_t pid); 


/* find process name and CR3 using the PID as search key */ 
extern int 
VMI_find_process_by_pid_c(uint32_t pid, 
                          char proc_name[],
                          size_t len, 
                          uint32_t *cr3);


extern int 
VMI_get_proc_modules_c(uint32_t pid, 
                       uint32_t mod_no, 
                       tmodinfo_t *buf);

extern int 
VMI_get_all_processes_count_c(void); 


// Create array with info about all processes running in system  
extern int 
VMI_find_all_processes_info_c(size_t num_proc, 
                              procinfo_t *arr);


// Aravind - added to get the number of loaded modules for the process. 
// This is needed to create the memory required by get_proc_modules 
extern int 
VMI_get_loaded_modules_count_c(uint32_t pid); //end - Aravind


/// @ingroup semantics 
/// @return the current thread id. If for some reason, this operation 
/// is not successful, the return value is set to -1. 
/// This function only works in Windows XP for Now. 
extern int VMI_get_current_tid_c(CPUState* env);

//0 unknown 1 windows 2 linux 
extern int VMI_get_guest_version_c(void);
```

