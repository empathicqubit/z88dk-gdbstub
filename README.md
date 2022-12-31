# z88dk-gdbstub

A GDB stub library for z88dk z80 projects. You must implement the following
functions in your program for it to compile:

* unsigned char gdb_getDebugChar (void)
* void gdb_putDebugChar (unsigned char ch)

Other functions will also be needed to do anything useful.

See [The template project for TI 8x calculators](https://github.com/empathicqubit/z88dk-ti8xp-template) for an example implementation.

# License

This project is licensed under GPLv3, in compliance with the original stub code.
In many cases this means if you distribute a binary file produced by this code,
you must provide the user the sources of all code which produced that binary file.
Please see [the license](LICENSE.md), and if you're not sure, don't use it.
It's quite large and slow, so you probably shouldn't be distributing it anyway,
only using it for debugging purposes.