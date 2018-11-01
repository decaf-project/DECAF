# Plugin Sample

To demonstrate how DECAF enables and solves various binary analysis problems, we showcase three plugins. By hooking the entries and exits of APIs specified in a configuration file, API Tracer is able to trace the API invocations of a specified process and the pro- cesses spawned from it. Keylogger Detector keeps track of tainted keystrokes propagating in the OS kernel and across user-level processes to detect keyloggers. Instruction Tracer logs instructions executing within a specified context (such as a user-level process, or a kernel module). These plugins are mostly platform agnostic.

## API tracer

API tracer is a simple plugin that given a configuration file containing a list of APIs and their arguments, monitors the execution of a program and prints the execution of such APIs to a trace file.

### To use apitracer

1. Compile DECAF with vmi enabled. 

```shell
./configure --enable-vmi
make
```

2. Compile the apitracer plugin.

```shell
# cd to the plugin's source folder
./configure --decaf-path=/path/to/decaf
make
```

3. Use DECAF to start guest OS and load apitracer plugin.

```shell
# Start virtual machine, change directory to /path/to/decaf/trunk/i386-softmmu/
./qemu-system-i386 -monitor stdio -m 512 -netdev user,id=mynet -device rtl8139,netdev=mynet /path/to/your/image

# Load plugins in decaf console
(qemu) load_plugin path/to/apitracer/plugin/apitracer.so
```

4. Set the program to trace.

```shell
# A sample config file is included. in the apitracer folder.
trace_by_name
```

5. Inside the VM, start the program to. be traced.
6. To stop tracing, use `trace_stop`.



## Keylogger Detector

Leveraging the VMI, tainting and event-driven programing features of DECAF, Keylogger Detector is capable of identifying keyloggers and analyzing their stealthy behaviors.

By sending tainted keystrokes into the guest system and observing if any untrusted code modules access the tainted data, we can detect keylogging behavior. The sample plugin can introduce tainted keystrokes into the guest system and identify which modules read the tainted keystroke by registering to receive `DECAF_READ_TAINTMEM_CB` and `DECAF_KEYSTROKE_CB` callback events. To capture the detailed stealthy behaviors, Keylogger Detector implements a shadow call stack by registering for the `DECAF_BLOCK_END` callback. Whenever the callback is triggered, we check the current instruction. If it is a call instruction, we retrieve the function information using VMI and push the current program counter onto the shadow call stack. If it is a ret instruction and pairs with the entry on the top of the shadow call stack, we pop it from the stack. When the `DECAF_READ_TAINTMEM_CB` callback is invoked, we retrieve information about which process, module, and function read the tainted keystroke data from the function call stack.

### To use keylogger detector.

1. Compile DECAF with tcg tainting and vmi enabled. 

```shell
./configure --enable-tcg-taint --enable-vmi make
```

2. Compile keylogger detector plugin.

```shell
# cd to the plugin's source folder 
./configure --decaf-path=/path/to/decaf
make
```

3. Use DECAF to start guest OS and load keylogger detector plugin.

```shell
# start virtual machine, change directory to /path/to/decaf/trunk/i386-softmmu/ 
./qemu-system-i386 -monitor stdio -m 512 -netdev user,id=mynet -device rtl8139,netdev=mynet /path/to/your/image

# load plugins 
(qemu) load_plugin path/to/keylogger/plugin/keylogger.so
```

4. Enable keylogger detector. you can use "help" command to check the command supported by DECAF and keylogger detector.

```shell
(qemu) enable_keylogger_check LOCATION_OF_LOG_FILE
```

5. Turn on pointers tainting. Because guest os needs to translate scan code to ASCII code for every keystroke. This translation is table lookup operation. So we need to turn on pointers tainting. To avoid overtainting problem, we just turn on pointers read tainting.

```shell
(qemu) enable_tainting taint_pointers on off
```

6. Start your suspicious program and introduce a tainted keystroke into notepad.exe

```shell
(qemu) taint_sendkey c
```

7. When you see ‘c’ in notepad.exe,you can disable taintmodule check.

```shell
(qemu) disable_keylogger_check
```

8. Now check the log to see if keystroke is fetched by your suspicious program. If yes, it's a keylogger malware.



## Instruction tracer

- [ ] TODO

