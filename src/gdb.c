#include "lib.h"
#include "gdb.h"

// All publicly exported functions go here

void gdb_exception (int ex) __naked {
  __asm
	ld (__gdb_state+R_SP), sp
	LOAD_SP
	call	__gdb_save_cpu_state
	ld	hl, 0
	push	hl
#ifdef __SDCC_gbz80
	ld	hl, __gdb_state + R_SP
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a
#else
	ld	hl, (__gdb_state + R_SP)
#endif
	inc	hl
	inc	hl
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	push	de
	call	__gdb_stub_main
  __endasm;
  (void)ex;
}

#ifdef DBG_HWBREAK
#ifndef DBG_HWBREAK_SIZE
#define DBG_HWBREAK_SIZE 0
#endif /* DBG_HWBREAK_SIZE */
void
gdb_hwbreak (void) __naked
{
  __asm
	ld	(__gdb_state + R_SP), sp
	LOAD_SP
	call	__gdb_save_cpu_state
	ld	hl, -DBG_HWBREAK_SIZE
	push	hl
	ld	hl, EX_HWBREAK
	push	hl
	call	__gdb_stub_main
  __endasm;
}
#endif /* DBG_HWBREAK_SET */

void gdb_int(void) __naked {
  __asm
	ld	(__gdb_state + R_SP), sp
	LOAD_SP
	call	__gdb_save_cpu_state
	ld	hl, 0	;pc_adj
	push	hl
	ld	hl, DBG_INT_EX
	push	hl
	ld	hl, __gdb_stub_main
	push	hl
	push	hl
	ei
	reti
  __endasm;
}

#ifndef __SDCC_gbz80
void gdb_nmi(void) __naked {
  __asm
	ld	(__gdb_state + R_SP), sp
	LOAD_SP
	call	__gdb_save_cpu_state
	ld	hl, 0	;pc_adj
	push	hl
	ld	hl, DBG_NMI_EX
	push	hl
	ld	hl, __gdb_stub_main
	push	hl
	push	hl
	retn
  __endasm;
}
#endif

#ifdef DBG_SWBREAK
#ifdef DBG_SWBREAK_RST
#define DBG_SWBREAK_SIZE 1
#else
#define DBG_SWBREAK_SIZE 3
#endif

void gdb_swbreak (void) __naked {
  __asm
	ld (__gdb_state + R_SP), sp
	LOAD_SP
	call	__gdb_save_cpu_state
	ld	hl, -DBG_SWBREAK_SIZE
	push	hl
	ld	hl, EX_SWBREAK
	push	hl
	call	__gdb_stub_main
    ;.globl _break_handler
#ifdef DBG_SWBREAK_RST
_break_handler = DBG_SWBREAK_RST
#else
_break_handler = _gdb_swbreak
#endif
  __endasm;
}
#endif /* DBG_SWBREAK */

void gdb_set_enter(void (*func)(void)) {
    _gdb_enter_func = func;
}

void gdb_set_hwbreak_toggle(int (*func)(int set, void *addr)) {
    _gdb_toggle_hwbreak = func;
}

#ifdef DBG_TOGGLESTEP
void gdb_set_step_toggle(int (*func)(int set)) {
    _gdb_toggle_step = func;
}
#endif

#ifdef DGB_SWBREAK
void gdb_set_swbreak_toggle(int (*func)(int set, void *addr)) {
    _gdb_toggle_swbreak = func;
}
#endif

// Set calls to here in the holes, then remove them when we
// reenter
#if defined(DBG_TOGGLESTEP) && defined(DBG_SWBREAK)
void gdb_step (void) __naked {
  __asm
  jp _gdb_swbreak
  __endasm
}
#endif
