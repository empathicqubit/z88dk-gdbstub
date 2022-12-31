# z88dk-gdbstub

A GDB stub library for z88dk z80 projects. You must implement the following
functions in your program for it to compile:

* unsigned char gdb_getDebugChar (void)
* void gdb_putDebugChar (unsigned char ch)

Other functions will also be needed to do anything useful.

See [The template project for TI 8x calculators](https://github.com/empathicqubit/z88dk-ti8xp-template) for an example implementation.
