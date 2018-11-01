# Tutorials

This part is a tutorials on developing plugins.

## Introduction

DECAF provides many callback interfaces for developers to write powerful plugins to instrument the execution of the guest operating system. It invokes the callback at runtime,so you can dynamically enable or disable,register or unregister callbacks. With these callback interfaces, you can retrieve OS-level semantics including process, system api, keystroke, network,etc, completely outside the guest operating system. This manual provides some basic knowledge on developing plugins for DECAF.

## Sample plugin

The following code is a sample plugin. With this plugin, you can specify a process you wanna trace and print out the process name when the process starts. It's small but complete.

After DECAF loads the plugin, it will call `init_plugin(void)` first. You can use `plugin_interface_t`(DECAF_main.h) to define the behaviors of plugins. But the most important thing is to specify the `plugin_cleanup` interface.We usually free the resources allocated for this plugin using this interface. If we do not do this or do not free resources completely, DECAF may crash. Another thing you can do using `plugin_interface_t` is to define your own command for DECAF. So you can interact with your plugin at runtime.

```c
#include "DECAF_types.h" 
#include "DECAF_main.h" 
#include "DECAF_callback.h" 
#include "DECAF_callback_common.h"
#include "vmi_callback.h"
#include "utils/Output.h"
#include "DECAF_target.h"
//basic stub for plugins static 
plugin_interface_t my_interface; 
static DECAF_Handle processbegin_handle = DECAF_NULL_HANDLE;

/* 
 * This callback is invoked when a new process starts in the guest OS. 
 */ 
static void my_loadmainmodule_callback(VMI_Callback_Params params) {
    if(strcmp(params->cp.name,targetname)==0)
        DECAF_printf("Process %s you spcecified starts \n",params->cp.name); 
}

/* 
 * Handler to implement the command monitor_proc. 
 */
void do_monitor_proc(Monitor* mon, const QDict* qdict) {
    /* 
    * Copy the name of the process to be monitored to targetname. 
    */
    if ((qdict != NULL) && (qdict_haskey(qdict, "procname"))) {
        strncpy(targetname, qdict_get_str(qdict, "procname"), 512); 
    } 
    targetname[511] = '\0'; 
}

static int my_init(void) { 
    DECAF_printf("Hello World\n");
//register for process create and process remove events  
	processbegin_handle = 
        VMI_register_callback(VMI_CREATEPROC_CB,
                              &my_loadmainmodule_callback, NULL);
	if (processbegin_handle == DECAF_NULL_HANDLE){
  		DECAF_printf("Could not register for the create or remove proc events\n");  
	}
	return (0);
}

/* 
 * This function is invoked when the plugin is unloaded. 
 */ 
static void my_cleanup(void) {
    DECAF_printf("Bye world\n"); 
    /* Unregister for the process start and exit callbacks. */
    if (processbegin_handle != DECAF_NULL_HANDLE) {
        VMI_unregister_callback(VMI_CREATEPROC_CB, processbegin_handle); 
		processbegin_handle = DECAF_NULL_HANDLE; 
    } 
} 
/* 
 *Commands supported by the plugin. Included in plugin_cmds.h 
 */ 
static mon_cmd_t my_term_cmds[] = { 
    { .name = "monitor_proc", 
      .args_type = "procname:s?", 
      .mhandler.cmd = do_monitor_proc, 
      .params = "[procname]", 
      .help = "Run the tests with program [procname]" },
    {
        NULL, 
        NULL, 
    }, 
};

/* 
* This function registers the plugin_interface with DECAF. 
* The interface is used to register custom commands, let 
* DECAF know which cleanup function to call upon plugin 
* unload, etc,. 
*/ 
plugin_interface_t init_plugin(void) { 
    my_interface.mon_cmds = my_term_cmds; 
    my_interface.plugin_cleanup = &my_cleanup;
	my_init();
	return (&my_interface);
} 
```

Now,you know the **plugin_cleanup** and **mon_cmds**. Let's talk about the sample plugin code.

As you see, in the function `init_plugin(void)`, we define our own command using `my_term_cmds`. In `my_term_cmds`, we specify the command name, command handler, command parameters and help message. You can define whatever you like following this convention. The `plugin_cleanup` is also defined by `my_cleanup`. Using `my_cleanup`, DECAF cleanup all the resources used by the plugin.

In `my_init()`, we registe `VMI_CREATEPROC_CB` callback and its handler `my_loadmainmodule_callback`. So when a process starts, DECAF will call `my_loadmainmodule_callback`. And from its parameters, you can get the pid, name and cr3 of the process. So you can check if it's the process you specified with monitor_proc command. If so, we print out the process name. Of course, we can do many other things.

For example, we can register `DECAF_INSN_BEGIN_CB` callback and its handler at this place. And in its handler, we check if it belongs to the specified process and print out the instruction's EIP using the following code.

```c
uint32_t target_cr3; 
static void my_insn_begin_callback(DECAF_Callback_Params* params) {
    if(params->ib.env->cr[3]==target_cr3) {
        DECAF_printf("EIP 0x%08x \n",params->ib.env->eip); 
    } 
} 
/*
 * This callback is invoked when a new process starts in the guest OS. 
 */ 
static void my_loadmainmodule_callback(VMI_Callback_Params params) {
    if(strcmp(params->cp.name,targetname)==0){
        DECAF_printf("Process %s you spcecified starts \n",params->cp.name);
        target_cr3=params->cp.cr3; 
		DECAF_register_callback(DECAF_INSN_BEGIN_CB,&my_insn_begin_callback,NULL) 
    } 
}
```

One thing you need to take care of is the third parameter of `VMI_register_callback`. 

* If it's NULL, it means this callback will be invoked all the time.
* If it's a pointer pointing to 1, this callback will be invoked all the time. 
* If it's a pointer pointing to 0, this callback is disabled. 

Other kind of callback registration function is also following this convention.

Now you see how to register or unregister, enable or disable callbacks. As to the interfaces exported by DECAF to instrument the system,please see [DECAF interfaces](https://github.com/sycurelab/DECAF/blob/master/decaf/documents/Interfaces.md).

##  Hook API

For malware analysis, api trace is essential to understand the behavior of malware. Traditional analysis tool implements api trace by hooking api at different levels which is easy to bypass especially for Rootkit. DECAF can trace api more reliably outside VM. You can hook any api in your plugins or just hook some EIP. In the plugin [hookapitest](https://github.com/sycurelab/DECAF/tree/master/decaf/plugins/hookapitests), it shows how to hook a api and how to retrieve api parameters.

Now, we modify the above sample code to hook api NtCreateFile and retrieve it's parameter from the stack. First we register the api hook by `hookapi_hook_function_byname` when target process starts(when `my_loadmainmodule_callback` is called). 

In the following code, NtCreateFile_call will be invoked when guest os call NtCreateFile. For the parameters marked "**IN**", you can retrieve them from stack at this place. But for the "**OUT**" parameters, it's filled when NtCreateFile returns. In order to deal with this case, we hook the api return by using `hookapi_hook_return`. When NtCreateFile_call is invoked, the return address of NtCreateFile is stored on EBP. We should pass this return value to `hookapi_hook_return` function. Also, you can pass data to NtCreateFile_ret using 3th and 4th parameters of `hookapi_hook_return`.

In NtCreateFile_ret , we retrieve FileHandle form the stack. At this return point, EBP stores the address of first parameters FileHandle. We can use `DECAF_read_mem` to read this FileHandle from EBP. This is what we do in the following code. You can find more complex parameter retrieving function in **hookapitests/custom_handlers.c**. But the basic idea is same.

The key to retrieve parameters correctly is to understand "**address**" correctly. There are three kind of address--guest os's virtual address, guest os's physical address and host os's virtual address. The value of EBP is the guest os's virtual address. `DECAF_read_mem` get the content of specified virtual address stored in guest OS memory. Sometimes, the parameters of API is a pointer, you need to read this pointer's value first, and then read the memory pointed to by this pointer using DECAF_read_mem. Another thing you need to take care of is the character set problem. Windows uses Unicode string internally. If you get some unreadable code, you may need to convert it to readable character set. Keep that in mind!!

```c
DECAF_handle ntcreatefile_handle;

typedef struct { uint32_t call_stack[12]; //paramters and return address 
                DECAF_Handle hook_handle; 
               } 
NtCreateFile_hook_context_t;

/* NTSTATUS 
 * NtCreateFile( Out PHANDLE FileHandle, In ACCESS_MASK DesiredAccess,
 * In POBJECT_ATTRIBUTES ObjectAttributes, Out PIO_STATUS_BLOCK IoStatusBlock,
 * _In_opt_ PLARGE_INTEGER AllocationSize, In ULONG FileAttributes, 
 * In ULONG ShareAccess, In ULONG CreateDisposition, 
 * In ULONG CreateOptions, In PVOID EaBuffer, In ULONG EaLength ); 
 */ 
static void NtCreateFile_ret(void *param) {
    NtCreateFile_hook_context_t *ctx = (NtCreateFile_hook_context_t *)param;
    DECAF_printf("NtCreateFile exit:");
	hookapi_remove_hook(ctx->hook_handle);
	uint32_t out_handle;
	DECAF_read_mem(NULL, ctx->call_stack[1], 4, &out_handle);
	DECAF_printf("out_handle=%08x\n", out_handle);
	free(ctx);
}

static void NtCreateFile_call(void *opaque) {
	DECAF_printf("NtCreateFile entry\n"); 
    NtCreateFile_hook_context_t ctx = 
        (NtCreateFile_hook_context_t)malloc(sizeof(NtCreateFile_hook_context_t)); 
    if(!ctx) //run out of memory 
        return;
	DECAF_read_mem(NULL, cpu_single_env->regs[R_ESP], 12*4, ctx->call_stack);
	ctx->hook_handle = hookapi_hook_return(ctx->call_stack[0],
                                           NtCreateFile_ret, 
                                           ctx, 
                                           sizeof(*ctx));
}

static void my_loadmainmodule_callback(VMI_Callback_Params* params) {
    if(strcmp(params->cp.name,targetname)==0){
        DECAF_printf("Process %s you spcecified starts \n",params->cp.name);
        target_cr3=params->cp.cr3;
        
    /* @ingroup hookapi 
    * install a hook at the function entry by specifying module name and function name 
    * @param mod module name that this function is located in 
    * @param func function name 
    * @param is_global flag specifies if this hook should be invoked globally or only in certain execution context (when should_monitor is true) 
    * @param cr3 the memory space that this hook is installed. 0 for all memory spaces. 
    * @param fnhook address of function hook 
    * @param opaque address of an opaque structure provided by caller (has to be globally allocated) 
    * @param sizeof_opaque size of the opaque structure (if opaque is an integer, not a pointer to a structure, sizeof_opaque must be zero) 
    * @return a handle that uniquely identifies this hook 
    
    * Note that the handle that is returned, might not 
    * actually be active yet - you can check the eip value 
    * of the handle to find out the default value is 0. 
    */
	ntcreatefile_handle = hookapi_hook_function_byname( "ntdll.dll",
                                                       "NtCreateFile", 
                                                   	   1, 
                                                       target_cr3, 
                                                       NtCreateFile_call, 
                                                       NULL, 
                                                       0); 
    }
}
```

## Tainting

DECAF has **bitwise** tainting functionality built in. It’s implemented by inserting taint propagation opcodes into the stream of Tiny Code Generator (TCG) opcodes being executed by the emulator. Because of this, taint propagation within DECAF incurs a much smaller runtime performance penalty than other whole-system dynamic analysis platforms. It’s very easy to develop data flow tracking tools using this functionality.

To enable tainting, you should compile DECAF with `--enable-tcg-taint`. After you start DECAF, the tainting is enabled by default. You can disable it to clear the taint data using command “disable-taint”. Also, you can check the taint information using command `tainted_bytes`. One thing you can configure tainting is the pointer tainting. The command `taint_pointers on/off on/off` turns on/off the pointers read/store tainting.



```c
taintcheck_check_virtmem(uint32_t vaddr, uint32_t size, uint8_t * taint);
taintcheck_register_check(int regid,int offset,int size,CPUState *env);
taint_mem(uint32_t addr,int size,uint8_t *taint);
```

Using the above apis exported by DECAF, you can taint a memory or check the taint status of specific memoery or register. To taint a register, just directly set the shadow register taint_regs defined in the struct CPUState. For example, to taint eax, we can using the following statement.

```c
cpu_single_env->taint_regs[R_EAX] = taint_value;//bitwise tainting.
```

DECAF also supports keystroke and network tainting. We include a [KeyLogger](https://github.com/sycurelab/DECAF/tree/master/decaf/plugins/keylogger) in the plugins. It’s similar to taint network input/output. In the future, we will provide other tainting interface for other hardwares(e.g., disk).



## Configure & Makefile

We suggest you use configure/Makefile in the sample plugin we published in [decaf plugins](https://github.com/sycurelab/DECAF/tree/master/decaf/plugins) as a template to write your own configure/makefile.