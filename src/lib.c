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

/* This file contains stuff internal to the library */

#include "debug.h"
#include "lib.h"
#include "gdb.h"

#ifdef DBG_SWBREAK
#define DO_EXPAND(VAL)  VAL ## 123456
#define EXPAND(VAL)     DO_EXPAND(VAL)

#if EXPAND(DBG_SWBREAK) != 123456
#define DBG_SWBREAK_PROC DBG_SWBREAK
int (*_gdb_toggle_swbreak)(int set, void *addr) = NULL;
#endif

#undef EXPAND
#undef DO_EXPAND
#endif /* DBG_SWBREAK */

#ifdef DBG_HWBREAK
int (*_gdb_toggle_hwbreak)(int set, void *addr) = NULL;
#endif

#ifdef DBG_TOGGLESTEP
int (*_gdb_toggle_step)(int set) = NULL;
#endif

#ifdef DBG_MEMCPY
extern void* DBG_MEMCPY (void *dest, const void *src, unsigned n);
#endif

#ifdef DBG_WWATCH
extern int DBG_WWATCH(int set, void *addr, unsigned size);
#endif

#ifdef DBG_RWATCH
extern int DBG_RWATCH(int set, void *addr, unsigned size);
#endif

#ifdef DBG_AWATCH
extern int DBG_AWATCH(int set, void *addr, unsigned size);
#endif

#include <string.h>

byte _gdb_state[NUMREGBYTES];

#if DBG_PACKET_SIZE < (NUMREGBYTES*2+5)
#error "Too small DBG_PACKET_SIZE"
#endif

#ifndef FASTCALL
#define FASTCALL __z88dk_fastcall
#endif

#ifndef DBG_ENTER
#define DBG_ENTER
#else
void (*_gdb_enter_func)(void) = NULL;
#endif

#ifndef DBG_RESUME
#define DBG_RESUME ret
#endif

static signed char sigval;
static unsigned char first_entry = 0;

static char put_packet_info (const char *buffer) FASTCALL;

/************** UTILITY FUNCTIONS ********************/
static char low_hex (byte v) FASTCALL {
  v &= 0x0f;
  v += '0';
  if (v < '9'+1)
    return v;
  return v + 'a' - '0' - 10;
}

static char high_hex (byte v) FASTCALL {
  return low_hex(v >> 4);
}

static char * byte2hex (char *p, byte v) {
  *p++ = high_hex (v);
  *p++ = low_hex (v);
  return p;
}

/* convert the memory, pointed to by mem into hex, placing result in buf */
/* return a pointer to the last char put in buf (null) */
static char * mem2hex (char *buf, const byte *mem, unsigned bytes) {
  char *d = buf;
  if (bytes != 0)
    {
      do
	{
	  d = byte2hex (d, *mem++);
	}
      while (--bytes);
    }
  *d = 0;
  return d;
}

static signed char hex2val (unsigned char hex) FASTCALL {
  if (hex <= '9')
    return hex - '0';
  hex &= 0xdf; /* make uppercase */
  hex -= 'A' - 10;
  return (hex >= 10 && hex < 16) ? hex : -1;
}

static int hex2byte (const char *p) FASTCALL {
  signed char h = hex2val (p[0]);
  signed char l = hex2val (p[1]);
  if (h < 0 || l < 0)
    return -1;
  return (byte)((byte)h << 4) | (byte)l;
}

/* convert the hex array pointed to by buf into binary, to be placed in mem
   return a pointer to the character after the last byte written */
static char *hex2mem (byte *mem, char *buf, unsigned bytes) {
  if (bytes != 0)
    {
      do
	{
	  *mem++ = hex2byte (buf);
	  buf += 2;
	}
      while (--bytes);
    }
  return buf;
}

static int hex2int (const char **buf) FASTCALL {
  word r = 0;
  while(1)
    {
      signed char a = hex2val(**buf);
      if (a < 0)
	break;
      r <<= 4;
      r += (byte)a;
      (*buf)++;
    }
  return (int)r;
}

static char * int2hex (char *buf, int v) {
  buf = byte2hex(buf, (word)v >> 8);
  return byte2hex(buf, (byte)v);
}
/******************************************************************************/
static void store_pc_sp (int pc_adj) FASTCALL;
#define get_reg_value(mem) (*(void* const*)(mem))
#define set_reg_value(mem,val) do { (*(void**)(mem) = (val)); } while (0)
static void get_packet (char *buffer);
static void put_packet (const char *buffer);
static char process (char *buffer) FASTCALL;

#ifdef DBG_PRINT
void gdb_print(const char *str) {
    gdb_putDebugChar('$');
    gdb_putDebugChar('O');
    char csum = 'O';
    for (; *str != '\0'; ) {
        char c = high_hex (*str);
        csum += c;
        gdb_putDebugChar (c);
        c = low_hex (*str++);
        csum += c;
        gdb_putDebugChar (c);
    }
    gdb_putDebugChar('#');
    gdb_putDebugChar(high_hex (csum));
    gdb_putDebugChar(low_hex (csum));
}
#endif /* DBG_PRINT */

void _gdb_stub_main (int ex, int pc_adj) {
  char buffer[DBG_PACKET_SIZE+1];
  sigval = (signed char)ex;
  store_pc_sp (pc_adj);

  DBG_ENTER

  if(!gdb_getDebugChar || !gdb_putDebugChar) {
    _gdb_rest_cpu_state();
  }

  if(!first_entry) {
    // put some extra bytes on the line to fill the cable's buffer
    first_entry = 1;
    gdb_putDebugChar(0);
    gdb_putDebugChar(0);
  }

  #if defined(DBG_SWBREAK) && defined(DBG_TOGGLESTEP)
  if(DBG_TOGGLESTEP) {
    DBG_TOGGLESTEP(0);
  }
  #endif

  /* after starting gdb_stub must always return stop reason */
  *buffer = '?';
  for (; process (buffer);)
    {
      put_packet (buffer);
      get_packet (buffer);
    }
  put_packet (buffer);
  _gdb_rest_cpu_state ();
}

static void get_packet (char *buffer) {
  byte csum;
  char ch;
  char *p;
  byte esc;
#if DBG_PACKET_SIZE <= 256
  byte count; /* it is OK to use up to 256 here */
#else
  unsigned count;
#endif
  for (;; gdb_putDebugChar ('-'))
    {
      /* wait for packet start character */
      while((ch = gdb_getDebugChar()) != '$');
retry:
      csum = 0;
      esc = 0;
      p = buffer;
      count = DBG_PACKET_SIZE;
      do
	{
	  ch = gdb_getDebugChar();
	  switch (ch)
	    {
	    case '$':
	      goto retry;
	    case '#':
	      goto finish;
	    case '}':
	      esc = 0x20;
	      break;
	    default:
	      *p++ = ch ^ esc;
	      esc = 0;
	      --count;
	    }
	  csum += ch;
	}
      while (count != 0);
finish:
      *p = '\0';
      if (ch != '#') /* packet is too large */
	continue;
      ch = gdb_getDebugChar();
      if (ch != high_hex (csum))
	continue;
      ch = gdb_getDebugChar();
      if (ch != low_hex (csum))
	continue;
      break;
    }
  gdb_putDebugChar('+');
}

static void put_packet (const char *buffer) {
  /*  $<packet info>#<checksum>. */
  for (;;)
    {
      gdb_putDebugChar('$');
      char checksum = put_packet_info (buffer);
      gdb_putDebugChar('#');
      gdb_putDebugChar(high_hex(checksum));
      gdb_putDebugChar(low_hex(checksum));
      for (;;)
	{
	  char c = gdb_getDebugChar ();
	  switch (c)
	    {
	    case '+': return;
	    case '-': break;
	    default:
	      //gdb_putDebugChar(c);
	      continue;
	    }
	  break;
	}
    }
}

static char put_packet_info (const char *src) FASTCALL {
  char ch;
  char checksum = 0;
  for (;;)
    {
      ch = *src++;
      if (ch == '\0')
	break;
      if (ch == '}' || ch == '*' || ch == '#' || ch == '$')
	{
	  /* escape special characters */
	  gdb_putDebugChar('}');
	  checksum += '}';
	  ch ^= 0x20;
	}
      gdb_putDebugChar(ch);
      checksum += ch;
    }
  return checksum;
}

static void store_pc_sp (int pc_adj) FASTCALL {
  byte *sp = get_reg_value (&_gdb_state[R_SP]);
  byte *pc = get_reg_value (sp);
  pc += pc_adj;
  set_reg_value (&_gdb_state[R_PC], pc);
  set_reg_value (&_gdb_state[R_SP], sp + REG_SIZE);
}

static char *hex2mem (byte *mem, char *buf, unsigned bytes);

/* Command processors. Takes pointer to buffer (begins from command symbol),
   modifies buffer, returns: -1 - empty response (ignore), 0 - success,
   positive: error code. */

#ifdef DBG_MIN_SIZE
static signed char process_question (char *p) FASTCALL {
  signed char sig;
  *p++ = 'S';
  sig = sigval;
  if (sig <= 0)
    sig = EX_SIGTRAP;
  p = byte2hex (p, (byte)sig);
  *p = '\0';
  return 0;
}
#else /* DBG_MIN_SIZE */
static char *format_reg_value (char *p, unsigned reg_num, const byte *value);

static signed char process_question (char *p) FASTCALL {
  signed char sig;
  *p++ = 'T';
  sig = sigval;
  if (sig <= 0)
    sig = EX_SIGTRAP;
  p = byte2hex (p, (byte)sig);
  p = format_reg_value(p, R_AF/REG_SIZE, &_gdb_state[R_AF]);
  p = format_reg_value(p, R_SP/REG_SIZE, &_gdb_state[R_SP]);
  p = format_reg_value(p, R_PC/REG_SIZE, &_gdb_state[R_PC]);
#if defined(DBG_SWBREAK_PROC) || defined(DBG_HWBREAK) || defined(DBG_WWATCH) || defined(DBG_RWATCH) || defined(DBG_AWATCH)
  const char *reason;
  unsigned addr = 0;
  switch (sigval)
    {
#ifdef DBG_SWBREAK_PROC
    case EX_SWBREAK:
      reason = "swbreak";
      break;
#endif
#ifdef DBG_HWBREAK
    case EX_HWBREAK:
      reason = "hwbreak";
      break;
#endif
#ifdef DBG_WWATCH
    case EX_WWATCH:
      reason = "watch";
      addr = 1;
      break;
#endif
#ifdef DBG_RWATCH
    case EX_RWATCH:
      reason = "rwatch";
      addr = 1;
      break;
#endif
#ifdef DBG_AWATCH
    case EX_AWATCH:
      reason = "awatch";
      addr = 1;
      break;
#endif
    default:
      goto finish;
    }
  while ((*p++ = *reason++))
    ;
  --p;
  *p++ = ':';
  if (addr != 0)
    p = int2hex(p, addr);
  *p++ = ';';
finish:
#endif /* DBG_HWBREAK, DBG_WWATCH, DBG_RWATCH, DBG_AWATCH */
  *p++ = '\0';
  return 0;
}
#endif /* DBG_MINSIZE */

#define STRING2(x) #x
#define STRING1(x) STRING2(x)
#define STRING(x) STRING1(x)
#if defined(DBG_MEMORY_MAP) || defined(DBG_FEATURE_STR)
static void read_xml_document (char *buffer, unsigned offset, unsigned length, const char *doc);
#endif

static signed char process_q (char *buffer) FASTCALL {
    char *p;
    if (memcmp (buffer + 1, "Supported", 9) == 0) {
        memcpy (buffer, "PacketSize=", 11);
        p = int2hex (&buffer[11], DBG_PACKET_SIZE);
#ifndef DBG_MIN_SIZE
#ifdef DBG_SWBREAK_PROC
        if(DBG_SWBREAK_PROC) {
            memcpy (p, ";swbreak+", 9);
            p += 9;
        }
#endif
#ifdef DBG_HWBREAK
        if(DBG_HWBREAK) {
            memcpy (p, ";hwbreak+", 9);
            p += 9;
        }
#endif
#endif /* DBG_MIN_SIZE */

#ifdef DBG_MEMORY_MAP
        memcpy (p, ";qXfer:memory-map:read+", 23);
        p += 23;
#endif
#ifdef DBG_FEATURE_STR
        memcpy (p, ";qXfer:features:read+", 21);
        p += 21;
#endif
        *p = '\0';
        return 0;
    }
#ifdef DBG_FEATURE_STR
    if (memcmp (buffer + 1, "Xfer:features:read:", 19) == 0) {
        p = strchr (buffer + 1 + 19, ':');
        if (p == NULL) {
        return 1;
        }
        ++p;
        unsigned offset = hex2int (&p);
        if (*p++ != ',') {
        return 2;
        }
        unsigned length = hex2int (&p);
        if (length == 0) {
        return 3;
        }
        if (length > strlen(DBG_FEATURE_STR)) {
        length = strlen(DBG_FEATURE_STR);
        }
        if (length > DBG_PACKET_SIZE) {
        return 4;
        }
        read_xml_document (buffer, offset, length, DBG_FEATURE_STR);
        return 0;
    }
#endif
#ifdef DBG_MEMORY_MAP
    if (memcmp (buffer + 1, "Xfer:memory-map:read:", 21) == 0) {
        p = strchr (buffer + 1 + 21, ':');
        if (p == NULL)
        return 1;
        ++p;
        unsigned offset = hex2int (&p);
        if (*p++ != ',')
        return 2;
        unsigned length = hex2int (&p);
        if (length == 0)
        return 3;
        if (length > DBG_PACKET_SIZE)
        return 4;
        read_xml_document (buffer, offset, length, DBG_MEMORY_MAP);
        return 0;
    }
#endif
#ifndef DBG_MIN_SIZE
    if (memcmp (&buffer[1], "Attached", 9) == 0) {
        /* Just report that GDB attached to existing process
        if it is not applicable for you, then send patches */
        memcpy(buffer, "1", 2);
        return 0;
    }
#endif /* DBG_MIN_SIZE */
    *buffer = '\0';
    return -1;
}

static signed char process_g (char *buffer) FASTCALL {
  mem2hex (buffer, _gdb_state, NUMREGBYTES);
  return 0;
}

static signed char process_G (char *buffer) FASTCALL {
  hex2mem (_gdb_state, &buffer[1], NUMREGBYTES);
  /* OK response */
  *buffer = '\0';
  return 0;
}

static signed char process_m (char *buffer) FASTCALL {
    /* mAA..AA,LLLL  Read LLLL bytes at address AA..AA */
    char *p = &buffer[1];
    byte *addr = (void*)hex2int(&p);
    if (*p++ != ',')
        return 1;
    unsigned len = (unsigned)hex2int(&p);
    if (len == 0)
        return 2;
    if (len > DBG_PACKET_SIZE/2)
        return 3;
    p = buffer;
#ifdef DBG_MEMCPY
    do {
        byte tmp[16];
        unsigned tlen = sizeof(tmp);
        if (tlen > len)
            tlen = len;
        if (!DBG_MEMCPY(tmp, addr, tlen))
            return 4;
        p = mem2hex (p, tmp, tlen);
        addr += tlen;
        len -= tlen;
    }
    while (len);
#else
    p = mem2hex (p, addr, len);
#endif
    return 0;
}

static signed char process_M (char *buffer) FASTCALL {
    /* MAA..AA,LLLL: Write LLLL bytes at address AA.AA return OK */
  char *p = &buffer[1];
  byte *addr = (void*)hex2int(&p);
  if (*p != ',')
    return 1;
  ++p;
  unsigned len = (unsigned)hex2int(&p);
  if (*p++ != ':')
    return 2;
  if (len == 0)
    goto end;
  if (len*2 + (p - buffer) > DBG_PACKET_SIZE)
    return 3;
#ifdef DBG_MEMCPY
  do
    {
      byte tmp[16];
      unsigned tlen = sizeof(tmp);
      if (tlen > len)
	tlen = len;
      p = hex2mem (tmp, p, tlen);
      if (!DBG_MEMCPY(addr, tmp, tlen))
	return 4;
      addr += tlen;
	len -= tlen;
    }
  while (len);
#else
  hex2mem (addr, p, len);
#endif
end:
  /* OK response */
  *buffer = '\0';
  return 0;
}

#ifndef DBG_MIN_SIZE
static signed char process_X (char *buffer) FASTCALL {
    /* XAA..AA,LLLL: Write LLLL binary bytes at address AA.AA return OK */
  char *p = &buffer[1];
  byte *addr = (void*)hex2int(&p);
  if (*p != ',')
    return 1;
  ++p;
  unsigned len = (unsigned)hex2int(&p);
  if (*p++ != ':')
    return 2;
  if (len == 0)
    goto end;
  if (len + (p - buffer) > DBG_PACKET_SIZE)
    return 3;
#ifdef DBG_MEMCPY
  if (!DBG_MEMCPY(addr, p, len))
    return 4;
#else
  memcpy (addr, p, len);
#endif
end:
  /* OK response */
  *buffer = '\0';
  return 0;
}
#else /* DBG_MIN_SIZE */
static signed char
process_X (char *buffer) FASTCALL
{
  (void)buffer;
  return -1;
}
#endif /* DBG_MIN_SIZE */

static signed char process_c (char *buffer) FASTCALL {
    /* 'cAAAA' - Continue at address AAAA(optional) */
  const char *p = &buffer[1];
  if (*p != '\0')
    {
      void *addr = (void*)hex2int(&p);
      set_reg_value (&_gdb_state[R_PC], addr);
    }
  _gdb_rest_cpu_state ();
  return 0;
}

static signed char process_s (char *buffer) FASTCALL {
  /* 'sAAAA' - Step at address AAAA(optional) */
  #if defined(DBG_TOGGLESTEP) && defined(DBG_SWBREAK)
  if(!DBG_TOGGLESTEP) {
    return -1;
  }
  int err = DBG_TOGGLESTEP(1);
  if(err) {
    return err;
  }
  const char *p = &buffer[1];
  if (*p != '\0')
    {
      void *addr = (void*)hex2int(&p);
      set_reg_value (&_gdb_state[R_PC], addr);
    }
  _gdb_rest_cpu_state ();
  return 0;
  #else
  return -1;
  #endif
}

static signed char process_D (char *buffer) FASTCALL {
    /* 'D' - detach the program: continue execution */
    if(!DBG_SWBREAK_PROC) {
        return -1;
    }

    DBG_SWBREAK_PROC(0, NULL);
    _gdb_rest_cpu_state ();
    return 0;
}

static signed char
process_k (char *buffer) FASTCALL {
    /* 'k' - Kill the program */
  set_reg_value (&_gdb_state[R_PC], 0);
  _gdb_rest_cpu_state ();
  (void)buffer;
  return 0;
}

static signed char process_v (char *buffer) FASTCALL {
#ifndef DBG_MIN_SIZE
  if (memcmp (&buffer[1], "Cont", 4) == 0)
    {
      if (buffer[5] == '?')
	{
	  /* result response will be "vCont;c;C"; C action must be
	     supported too, because GDB reguires at lease both of them */
	  memcpy (&buffer[5], ";c;C", 5);
	  return 0;
	}
      buffer[0] = '\0';
      if (buffer[5] == ';' && (buffer[6] == 'c' || buffer[6] == 'C'))
	return -2; /* resume execution */
      return 1;
  }
#endif /* DBG_MIN_SIZE */
  return -1;
}

static signed char process_zZ (char *buffer) FASTCALL {
    /* insert/remove breakpoint */
#if defined(DBG_SWBREAK_PROC) || defined(DBG_HWBREAK) || \
    defined(DBG_WWATCH) || defined(DBG_RWATCH) || defined(DBG_AWATCH)
    const byte set = (*buffer == 'Z');
    const char *p = &buffer[3];
    void *addr = (void*)hex2int(&p);
    if (*p != ',')
        return 1;
    p++;
    int kind = hex2int(&p);
    *buffer = '\0';
    switch (buffer[1]) {
#ifdef DBG_SWBREAK_PROC
        case '0': /* sw break */
            if(!DBG_SWBREAK_PROC) {
                return -1;
            }
            return DBG_SWBREAK_PROC(set, addr);
#endif
#ifdef DBG_HWBREAK
        case '1': /* hw break */
            if(!DBG_HWBREAK) {
                return -1;
            }
            return DBG_HWBREAK(set, addr);
#endif
#ifdef DBG_WWATCH
        case '2': /* write watch */
            return DBG_WWATCH(set, addr, kind);
#endif
#ifdef DBG_RWATCH
        case '3': /* read watch */
            return DBG_RWATCH(set, addr, kind);
#endif
#ifdef DBG_AWATCH
        case '4': /* access watch */
            return DBG_AWATCH(set, addr, kind);
#endif
        default:; /* not supported */
    }
#endif
  (void)buffer;
  return -1;
}

static signed char do_process (char *buffer) FASTCALL {
    switch (*buffer) {
        case 's': return process_s(buffer);
        case '?': return process_question (buffer);
        case 'G': return process_G (buffer);
        case 'k': return process_k (buffer);
        case 'M': return process_M (buffer);
        case 'X': return process_X (buffer);
        case 'Z': return process_zZ (buffer);
        case 'c': return process_c (buffer);
        case 'D': return process_D (buffer);
        case 'g': return process_g (buffer);
        case 'm': return process_m (buffer);
        case 'q': return process_q (buffer);
        case 'v': return process_v (buffer);
        case 'z': return process_zZ (buffer);
        default:  return -1; /* empty response */
    }

    return -1;
}

static char process (char *buffer) FASTCALL {
  signed char err = do_process (buffer);
  char *p = buffer;
  char ret = 1;
  if (err == -2)
    {
      ret = 0;
      err = 0;
    }
  if (err > 0)
    {
      *p++ = 'E';
      p = byte2hex (p, err);
      *p = '\0';
    }
  else if (err < 0)
    {
      *p = '\0';
    }
  else if (*p == '\0')
    memcpy(p, "OK", 3);
  return ret;
}

#if defined(DBG_MEMORY_MAP) || defined(DBG_FEATURE_STR)
static void read_xml_document (char *buffer, unsigned offset, unsigned length, const char *doc) {
    const unsigned doc_sz = strlen(doc);
    if (offset >= doc_sz) {
        buffer[0] = 'l';
        buffer[1] = '\0';
        return;
    }
    if (offset + length >= doc_sz) {
        length = doc_sz - offset;
        buffer[0] = 'l';
    }
    else {
        buffer[0] = 'm';
    }
    memcpy (&buffer[1], &doc[offset], length);
    buffer[1+length] = '\0';
}
#endif

/* write string like " nn:0123" and return pointer after it */
#ifndef DBG_MIN_SIZE
static char * format_reg_value (char *p, unsigned reg_num, const byte *value) {
    char *d = p;
    unsigned char i;
    d = byte2hex(d, reg_num);
    *d++ = ':';
    value += REG_SIZE;
    i = REG_SIZE;
    do {
        d = byte2hex(d, *--value);
    }
    while (--i != 0);
    *d++ = ';';
    return d;
}
#endif /* DBG_MIN_SIZE */

#ifdef __SDCC_gbz80
/* saves all _gdb_state.except PC and SP */
void _gdb_save_cpu_state() __naked {
  __asm
	push	af
	ld	a, l
	ld	(__gdb_state + R_HL + 0), a
	ld	a, h
	ld	(__gdb_state + R_HL + 1), a
	ld	hl, __gdb_state + R_HL - 1
	ld	(hl), d
	dec	hl
	ld	(hl), e
	dec	hl
	ld	(hl), b
	dec	hl
	ld	(hl), c
	dec	hl
	pop	bc
	ld	(hl), b
	dec	hl
	ld	(hl), c
	ret
  __endasm;
}

/* restore CPU _gdb_state and continue execution */
void _gdb_rest_cpu_state() __naked {
  __asm
;restore SP
	ld	a, (__gdb_state + R_SP + 0)
	ld	l,a
	ld	a, (__gdb_state + R_SP + 1)
	ld	h,a
	ld	sp, hl
;push PC value as return address
	ld	a, (__gdb_state + R_PC + 0)
	ld	l, a
	ld	a, (__gdb_state + R_PC + 1)
	ld	h, a
	push	hl
;restore registers
	ld	hl, __gdb_state + R_AF
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	inc	hl
	push	bc
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	inc	hl
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	inc	hl
	ld	a, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, a
	pop	af
	ret
  __endasm;
}
#else
/* saves all _gdb_state.except PC and SP */
void _gdb_save_cpu_state() __naked {
  __asm
	ld	(__gdb_state + R_HL), hl
	ld	(__gdb_state + R_DE), de
	ld	(__gdb_state + R_BC), bc
	push	af
	pop	hl
	ld	(__gdb_state + R_AF), hl
	ld	a, r	;R is increased by 7 or by 8 if called via RST
	ld	l, a
	sub	a, 7
	xor	a, l
	and	a, 0x7f
	xor	a, l
#ifdef __SDCC_ez80_adl
	ld	hl, i
	ex	de, hl
	ld	hl, __gdb_state + R_IR
	ld	(hl), a
	inc	hl
	ld	(hl), e
	inc	hl
	ld	(hl), d
	ld	a, MB
	ld	(__gdb_state + R_AF+2), a
#else
	ld	l, a
	ld	a, i
	ld	h, a
	ld	(__gdb_state + R_IR), hl
#endif /* __SDCC_ez80_adl */
	ld	(__gdb_state + R_IX), ix
	ld	(__gdb_state + R_IY), iy
	ex	af, af'	;'
	exx
	ld	(__gdb_state + R_HL_), hl
	ld	(__gdb_state + R_DE_), de
	ld	(__gdb_state + R_BC_), bc
	push	af
	pop	hl
	ld	(__gdb_state + R_AF_), hl
	ret
  __endasm;
}

/* restore CPU _gdb_state and continue execution */
void _gdb_rest_cpu_state() __naked {
  __asm
#ifdef DBG_USE_TRAMPOLINE
	ld	sp, __gdb_stack + DBG_STACK_SIZE
	ld	hl, (__gdb_state + R_PC)
	push	hl	/* resume address */
#ifdef __SDCC_ez80_adl
	ld	hl, 0xc30000 ; use 0xc34000 for jp.s
#else
	ld	hl, 0xc300
#endif
	push	hl	/* JP opcode */
#endif /* DBG_USE_TRAMPOLINE */
	ld	hl, (__gdb_state + R_AF_)
	push	hl
	pop	af
	ld	bc, (__gdb_state + R_BC_)
	ld	de, (__gdb_state + R_DE_)
	ld	hl, (__gdb_state + R_HL_)
	exx
	ex	af, af'	;'
	ld	iy, (__gdb_state + R_IY)
	ld	ix, (__gdb_state + R_IX)
#ifdef __SDCC_ez80_adl
	ld	a, (__gdb_state + R_AF + 2)
	ld	MB, a
	ld	hl, (__gdb_state + R_IR + 1) ;I register
	ld	i, hl
	ld	a, (__gdb_state + R_IR + 0) ; R register
	ld	l, a
#else
	ld	hl, (__gdb_state + R_IR)
	ld	a, h
	ld	i, a
	ld	a, l
#endif /* __SDCC_ez80_adl */
	sub	a, 10	;number of M1 cycles after ld r,a
	xor	a, l
	and	a, 0x7f
	xor	a, l
	ld	r, a
	ld	de, (__gdb_state + R_DE)
	ld	bc, (__gdb_state + R_BC)
	ld	hl, (__gdb_state + R_AF)
	push	hl
	pop	af
	ld	sp, (__gdb_state + R_SP)
#ifndef DBG_USE_TRAMPOLINE
	ld	hl, (__gdb_state + R_PC)
	push	hl
	ld	hl, (__gdb_state + R_HL)
	DBG_RESUME
#else
	ld	hl, (__gdb_state + R_HL)
#ifdef __SDCC_ez80_adl
	jp	_gdb_stack + DBG_STACK_SIZE - 4
#else
	jp	_gdb_stack + DBG_STACK_SIZE - 3
#endif
#endif /* DBG_USE_TRAMPOLINE */
  __endasm;
}
#endif /* __SDCC_gbz80 */