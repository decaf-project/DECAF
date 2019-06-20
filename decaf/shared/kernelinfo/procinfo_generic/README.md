# Instructions

The data structures inside the kernel often change and subsequently affect the successful compilation of the procinfo kernel module.  Based on the kernel version you are using, you may need to update the data structures being accessed by `procinfo.c`.  Three different `procinfo.c` files are provided as a reference.  The original `procinfo.c` works on older kernels, `procinfo_4.1.17.c` works on kernel version 4.1.17, and `procinfo_4.4.c` works on kernel version 4.4.  Each version of `procinfo.c` may work on multiple kernels, it just depends on when the data structures were changed in the kernel.

# Usage

Only one `procinfo.c` should be compiled.  Pick one appropriate for your kernel (or modify one to work) and name it `procinfo.c`.  Then delete the other two before building the module.

# Example

To use `procinfo_4.1.17.c`:
```
$ mv procinfo_4.1.17.c procinfo.c
$ rm procinfo_4.4.c
$ make
```
