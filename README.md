DECAF
=====

DECAF(short for Dynamic Executable Code Analysis Framework) is a binary analysis platform based on QEMU.  This is also the home of the DroidScope dynamic Android malware analysis platform. DroidScope is now an extension to DECAF.

1. Introduction
======

DECAF(Dynamic Executable Code Analysis Framework) is the successor to the binary analysis techniques developed for TEMU ( dynamic analysis component of BitBlaze ) as part of Heng Yin's work on BitBlaze project headed up by Dawn Song. DECAF builds upon TEMU. We appreciate all that worked with us on that project.


![alt tag](http://sycurelab.ecs.syr.edu/image/overall_decaf.jpg)
Fig 1 the overall architecture of DECAF

Fig 1 illustrates the overall architecture of DECAF. DECAF is a platform-agnostic whole-system dynamic binary analysis framework. It provides the following key features.

**Right-on-Time Virtual Machine Introspection**

Different with TEMU, DECAF doesn’t use guest driver to retrieve os-level semantics. The VMI component of DECAF is able to reconstruct a fresh OS-level view of the virtual machine, including processes, threads, code modules, and symbols to support binary analysis. Further, in order to support multiple architectures and operating systems, it follows as a platform-agnostic design. The workflow for extracting OS-level semantic information is common across multiple architectures and operating systems. The only platform- specific handling lies in what kernel data structures and what fields to extract information from.

**Support for Multiple Platforms**

Ideally, we would like to have the same analysis code (with minimum platform-specific code) to work for different CPU architectures (e.g, x86 and ARM) and different operating systems (e.g., Windows and Linux). It requires that the analysis framework hide the architecture and operating system specific details from the analysis plugins. Further, to make the analysis framework itself maintainable and extensible to new architectures and operat-ing systems, the platform-specific code within the framework should also be minimized. DECAF can provide support for both multiple architectures and multiple operating systems. Currently, DECAF supports 32 bit Windows xp/Windows 7/linux and X86/arm.

**Precise and Lossless Tainting**

DECAF ensures precise tainting by maintaining bit-level precision for CPU registers and memory, and inlining precise tainting rules in the translated code blocks. Thus, the taint status for each CPU register and memory location is processed and updated synchronously during the code execution of the virtual machine. The propagation of taint labels is done in an asynchronous manner . By implementing such a tainting logic mainly in the intermediate representation level (more concretely, TCG IR level), it becomes easy to extend tainting support to a new CPU architecture.

**Event-driven programming interfaces**

DECAF provides an event-driven programming interface. It means that the paradigm of ”instrument” in the translation phase and then analyze in the execution phase” is invisible to the analysis plugins. The analysis plugins only need to register for interested events and implement corresponding event handling functions. The details of code instrumentation are taken care of by the framework.

**Dynamic instrumentation management**

To reduce runtime overhead, the instrumentation code is inserted into the translated code only when necessary. For example, when a plugin registers a function hook at a function’s entry point, the instrumentation code for this hook is only placed at the function entry point. When the plugin unregisters this function hook, the instrumentation code will also be removed from the translated code accordingly. To ease the development of plugins, the management of dynamic code instrumentation is completely taken care of in the framework, and thus invisible to the plugins.

2. Help Documents
======

Please refer to https://code.google.com/p/decaf-platform/wiki/DECAF for help document.

