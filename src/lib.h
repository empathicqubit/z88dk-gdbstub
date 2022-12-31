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

#ifndef __GDB_LIB_H__
#define __GDB_LIB_H__

typedef unsigned char byte;
typedef unsigned short word;

/* This file contains stuff internal to the library */

/* Usage:
  1. Copy this file to project directory
  2. Configure it commenting/uncommenting macros below or define DBG_CONFIGURED
     and all required macros and then include this file to one of your C-source
     files.
  3. Implement gdb_getDebugChar() and gdb_putDebugChar(), functions must not return
     until data received or sent.
  4. Implement all optional functions used to toggle breakpoints/watchpoints,
     if supported. Do not write fuctions to toggle software breakpoints if
     you unsure (GDB will do itself).
  5. Implement serial port initialization routine called at program start.
  6. Add necessary debugger entry points to your program, for example:
	.org 0x08	;RST 8 handler
	jp _gdb_swbreak
	...
	.org	0x66	;NMI handler
	jp	_gdb_nmi
	...
	main_loop:
	halt
	call	isDbgInterrupt
	jr	z,101$
	ld	hl, 2	;EX_SIGINT
	push	hl
	call	_debug_exception
	101$:
	...
  7. Compile file using SDCC (supported ports are: z80, z180, z80n, gbz80 and
     ez80_z80), do not use --peep-asm option. For example:
	$ sdcc -mz80 --opt-code-size --max-allocs-per-node 50000 z80-stub.c
*/
/******************************************************************************\
			     Configuration
\******************************************************************************/
#ifndef DBG_CONFIGURED
/* Uncomment this line, if stub size is critical for you */
//#define DBG_MIN_SIZE

/* Comment this line out if software breakpoints are unsupported.
   If you have special function to toggle software breakpoints, then provide
   here name of these function. Expected prototype:
       int _gdb_toggle_swbreak(int set, void *addr);
   function must return 0 on success. */
#define DBG_SWBREAK _gdb_toggle_swbreak

/* Define the function to disable and enable software stepping
  int _gdb_toggle_step(int set);
*/
#define DBG_TOGGLESTEP _gdb_toggle_step

/* Define if one of standard RST handlers is used as software
   breakpoint entry point */
//#define DBG_SWBREAK_RST 0x08

/* if platform supports hardware breakpoints then define following macro
   by name of function. Fuction must have next prototype:
     int _gdb_toggle_hwbreak(int set, void *addr);
   function must return 0 on success. */
#define DBG_HWBREAK _gdb_toggle_hwbreak

/* if platform supports hardware watchpoints then define all or some of
   following macros by names of functions. Fuctions prototypes:
     int toggle_watch(int set, void *addr, size_t size);  // memory write watch
     int toggle_rwatch(int set, void *addr, size_t size); // memory read watch
     int toggle_awatch(int set, void *addr, size_t size); // memory access watch
   function must return 0 on success. */
//#define DBG_WWATCH toggle_watch
//#define DBG_RWATCH toggle_rwatch
//#define DBG_AWATCH toggle_awatch

/* Size of hardware breakpoint. Required to correct PC. */
#define DBG_HWBREAK_SIZE 0

/* Define following macro if you need custom memory read/write routine.
   Function should return non-zero on success, and zero on failure
   (for example, write to ROM area).
   Useful with overlays (bank switching).
   Do not forget to define:
   _ovly_table - overlay table
   _novlys - number of items in _ovly_table
   or
   _ovly_region_table - overlay regions table
   _novly_regions - number of items in _ovly_region_table

   _ovly_debug_prepare - function is called before overlay mapping
   _ovly_debug_event - function is called after overlay mapping
 */
//#define DBG_MEMCPY memcpy

/* define dedicated stack size if required */
//#define DBG_STACK_SIZE 256

/* max GDB packet size
   should be much less that DBG_STACK_SIZE because it will be allocated on stack
*/
#define DBG_PACKET_SIZE 800

/* Uncomment if required to use trampoline when resuming operation.
   Useful with dedicated stack when stack pointer do not point to the stack or
   stack is not writable */
//#define DBG_USE_TRAMPOLINE

/* Uncomment following macro to enable debug printing to debugger console */
#define DBG_PRINT

#define DBG_NMI_EX EX_HWBREAK
#define DBG_INT_EX EX_SIGINT

/* Define following macro to statement, which will be exectuted after entering to
   _gdb_stub_main function. Statement should include semicolon. */
#define DBG_ENTER if(_gdb_enter_func) { _gdb_enter_func(); };

/* Define following macro to instruction(s), which will be execute before return
   control to the program. It is useful when gdb-stub is placed in one of overlays.
   This procedure must not change any register. On top of stack before invocation
   will be return address of the program. */
//#define DBG_RESUME jp _restore_bank

/* Define following macro to the string containing memory map definition XML.
   GDB will use it to select proper breakpoint type (HW or SW). */
/*#define DBG_MEMORY_MAP "\
<memory-map>\
	<memory type=\"rom\" start=\"0x0000\" length=\"0x4000\"/>\
<!--	<memory type=\"flash\" start=\"0x4000\" length=\"0x4000\">\
		<property name=\"blocksize\">128</property>\
	</memory> -->\
	<memory type=\"ram\" start=\"0x8000\" length=\"0x8000\"/>\
</memory-map>\
"
*/

/* Define following macro to the string containing feature definition XML. */
#define DBG_FEATURE_STR "<target version=\"1.0\">"\
"<feature name=\"org.gnu.gdb.z80.cpu\">"\
"<reg name=\"af\" bitsize=\"16\" type=\"int\"/>"\
"<reg name=\"bc\" bitsize=\"16\" type=\"int\"/>"\
"<reg name=\"de\" bitsize=\"16\" type=\"int\"/>"\
"<reg name=\"hl\" bitsize=\"16\" type=\"int\"/>"\
"<reg name=\"sp\" bitsize=\"16\" type=\"data_ptr\"/>"\
"<reg name=\"pc\" bitsize=\"16\" type=\"code_ptr\"/>"\
"<reg name=\"ix\" bitsize=\"16\" type=\"int\"/>"\
"<reg name=\"iy\" bitsize=\"16\" type=\"int\"/>"\
"<reg name=\"af'\" bitsize=\"16\" type=\"int\"/>"\
"<reg name=\"bc'\" bitsize=\"16\" type=\"int\"/>"\
"<reg name=\"de'\" bitsize=\"16\" type=\"int\"/>"\
"<reg name=\"hl'\" bitsize=\"16\" type=\"int\"/>"\
"<reg name=\"ir\" bitsize=\"16\" type=\"int\"/>"\
"</feature>"\
"<architecture>z80</architecture>"\
"</target>"

#endif /* DBG_CONFIGURED */
/******************************************************************************\
			     Public Interface
\******************************************************************************/

#ifndef NULL
# define NULL ((void*)0)
#endif

#define EX_SWBREAK	0	/* sw breakpoint */
#define EX_HWBREAK	-1	/* hw breakpoint */
#define EX_WWATCH	-2	/* memory write watch */
#define EX_RWATCH	-3	/* memory read watch */
#define EX_AWATCH	-4	/* memory access watch */
#define EX_SIGINT	2
#define EX_SIGTRAP	5
#define EX_SIGABRT	6
#define EX_SIGBUS	10
#define EX_SIGSEGV	11
/* or any standard *nix signal value */

#endif // __GDB_LIB_H__

/******************************************************************************\
			       IMPLEMENTATION
\******************************************************************************/

/* CPU state */
#ifdef __SDCC_ez80_adl
# define REG_SIZE 3
#else
# define REG_SIZE 2
#endif /* __SDCC_ez80_adl */

#define R_AF (0*REG_SIZE)
#define R_BC (1*REG_SIZE)
#define R_DE (2*REG_SIZE)
#define R_HL (3*REG_SIZE)
#define R_SP (4*REG_SIZE)
#define R_PC (5*REG_SIZE)

#ifndef __SDCC_gbz80
#define R_IX    (6*REG_SIZE)
#define R_IY    (7*REG_SIZE)
#define R_AF_   (8*REG_SIZE)
#define R_BC_   (9*REG_SIZE)
#define R_DE_   (10*REG_SIZE)
#define R_HL_   (11*REG_SIZE)
#define R_IR    (12*REG_SIZE)

#ifdef __SDCC_ez80_adl
#define R_SPS   (13*REG_SIZE)
#define NUMREGBYTES (14*REG_SIZE)
#else
#define NUMREGBYTES (13*REG_SIZE)
#endif /* __SDCC_ez80_adl */
#else
#define NUMREGBYTES (6*REG_SIZE)
#define FASTCALL
#endif /*__SDCC_gbz80 */
extern byte _gdb_state[NUMREGBYTES];

void _gdb_save_cpu_state (void);
void _gdb_rest_cpu_state (void);
void _gdb_stub_main (int sigval, int pc_adj);

/* dedicated stack */
#ifdef DBG_STACK_SIZE

#define LOAD_SP	ld	sp, __gdb_stack + DBG_STACK_SIZE

static char _gdb_stack[DBG_STACK_SIZE];

#else

#undef DBG_USE_TRAMPOLINE
#define LOAD_SP

#endif

#ifdef DBG_HWBREAK
extern int (*_gdb_toggle_hwbreak)(int set, void *addr);
#endif

#if defined(DBG_TOGGLESTEP)
extern int (*_gdb_toggle_step)(int set);
#endif

#ifdef DBG_SWBREAK
extern int (*_gdb_toggle_swbreak)(int set, void *addr);
#endif

#ifdef DBG_ENTER
extern void (*_gdb_enter_func)(void);
#endif