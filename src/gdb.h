/* Debug stub for Z80.

   Copyright (C) 2022 Empathic Qubit.
   Copyright (C) 2021-2022 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifndef __GDB_GDB_H__
#define __GDB_GDB_H__

#include "lib.h"

#define export(returntype, signature) { \
    returntype signature; \
    extern returntype __LIB__ signature; \
}

/* These functions must be defined by the application */
unsigned char gdb_getDebugChar(void);
void gdb_putDebugChar(unsigned char ch);

/* This file contains the library exports. */

/* Enter to debug mode from software or hardware breakpoint.
   Assume address of next instruction after breakpoint call is on top of stack.
   Do JP _gdb_swbreak or JP _gdb_hwbreak from RST handler, for example.
 */
export(void, gdb_swbreak(void) __naked);
export(void, gdb_hwbreak(void) __naked);
export(void, gdb_step(void) __naked);
export(void, gdb_step (void));
export(void, gdb_exception (int ex) __naked);

/* Jump to this function from NMI handler. Just replace RETN instruction by
   JP _gdb_nmi
   Use if NMI detects request to enter to debug mode.
 */
export(void, gdb_nmi (void) __naked);

/* Jump to this function from INT handler. Just replace EI+RETI instructions by
   JP _gdb_int
   Use if INT detects request to enter to debug mode.
 */
export(void, gdb_int (void) __naked);

/* Prints to debugger console. */
export(void, gdb_print(const char *str));

/* Set the function which gets a packet character. This is required. */
export(void, gdb_set_get_char(unsigned char (*getter)(void)));

/* Set the function which puts a packet character. This is required. */
export(void, gdb_set_put_char(void (*putter)(unsigned char)));

/* Set the function which turns a software break in a particular location on or off */
export(void, gdb_set_swbreak_toggle(int (*func)(int set, void *addr)));

/* Set the function which turns a hardware break in a particular location on or off */
export(void, gdb_set_hwbreak_toggle(int (*func)(int set, void *addr)));

/* Set the function which turns line stepping on or off */
export(void, gdb_set_step_toggle(int (*func)(int set)));

/* Set the function which is called when the debugger is entered */
export(void, gdb_set_enter(void (*func)(void)));

/* Enter to debug mode (after receiving BREAK from GDB, for example)
 * Assume:
 *   program PC in (SP+0)
 *   caught signal in (SP+2)
 *   program SP is SP+4
 */
export(void, gdb_exception (int ex));

#endif  // __GDB_GDB_H__