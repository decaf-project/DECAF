# Elastic DECAF
A fork of DECAF Optimized with elastic taint propagation and elastic shadow memory access idea.
configure with --enable-2nd-ccache and then compile.

DECAF primarily aims to provide transparent dynamic malware analysis functionality. To this end, DECAF provides the following systematic mechanisms for binary analysis:
DECAF primarily aims to provide transparent dynamic malware analysis functionality. To this end, DECAF provides the following systematic mechanisms for binary analysis:
* **Virtual Machine Introspection (VMI) functionality**: In general system analysis, gathering information about the system status is crucial. VMI provides such information such as the active processes and kernel modules. Since malware analysis is also a type of system analysis, we are interested to gather system wide information. As an example, finding an unusual process or kernel module helps us find the malware in execution.
* **Binary instrumentation through hooking and callback**: Hooking allows implementation of different analysis functionalities. As an example, assume we are interested in detecting keyloggers. Such detection needs analyzing the keys propagating from the keyboard to an application. To track the propagation of the keys, we need to hook the keyboard driver through a callback function.
* **Dynamic tainting**: Dynamic tainting allows tracking the propagation of information while execution. In the above paragraph, we mentioned keylogger and the detection mechanism. Detection involves also dynamic tainting i.e. observing whether the key ends in a malicious application e.g. a keylogger.
* **Instruction logging**: Logging the instructions that are executed at runtime provides a powerful tool for dynamic system analysis. Through this functionality, we record the instructions that are executed at runtime. One use of this functionality is finding the trace to a keylogger malware.
 In the Wiki pages, we elaborate in detail how each mechanism is implemented by DECAF.
