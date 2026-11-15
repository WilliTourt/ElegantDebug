/*******************************************************************************
 * @file    ElegantDebug_8051.h
 * @version 1.0
 * @brief   ANSI-colored debug logging for 8051/8052 (Keil C51).
 *
 * This is the 8051 port of ElegantDebug. It is a self-contained module that
 * lives next to Src-C/ElegantDebug.c and Src-C/ElegantDebug.h; those two files
 * are NOT modified, so the original implementation and its platform selection
 * stay exactly as the author wrote them. On a dedicated 8051 branch there is
 * nothing to select at compile time, so this file contains no #if platform
 * blocks and only uses C90 constructs (Keil C51 defaults to C90).
 *
 * Why a separate module instead of extending ElegantDebug.h:
 *   Keil C51 allocates a permanent static argument area in DATA for every
 *   variadic function (about 48 bytes each). The original library has six of
 *   them, which needs 316 bytes of DATA - more than the 128 bytes of direct
 *   RAM an 8051 has, so BL51 stops with:
 *       ERROR L107: ADDRESS SPACE OVERFLOW
 *   This port therefore exposes fixed-signature calls only ("string", or
 *   "string + one number"), which costs ~47 bytes of DATA in total.
 *
 * Usage:
 *   debug_8051_init(ED8051_TRUE, ED8051_TRUE, ED8051_FALSE);
 *   ...call debug_tick() from a 1 ms timer ISR...
 *   debug_8051_info("hello\r\n");
 *   debug_8051_log_u32("tick(ms) = ", tick);
 *
 *   debug_8051_info/ok/success/warning/error do NOT append a newline
 *   (same convention as the original API); write "\r\n" yourself.
 *   debug_8051_log_u32() is the only convenience that appends CRLF.
 *
 * UART output:
 *   By default a polled transmitter is used (write SBUF, wait for TI).
 *   If your project already has an interrupt-driven TX queue, define
 *   ED8051_PUTC(c) to your own one-byte routine before including this file:
 *       #define ED8051_PUTC(c)  my_uart_put((unsigned char)(c))
 *
 * Requirements:
 *   - 8052 (256 bytes RAM) recommended; 128-byte parts do not fit.
 *   - UART mode 1, 8N1 (e.g. 9600 baud with an 11.0592 MHz crystal).
 *
 * @author:    <your name>
 * @date:      2026-11-15
 ******************************************************************************/
#ifndef __ELEGANT_DEBUG_8051_H
#define __ELEGANT_DEBUG_8051_H

#include <reg52.h>
#include <intrins.h>

/*** Basic types (C51 has no stdbool.h / stdint.h) *****************************/

typedef unsigned char  ed8051_bool;
typedef unsigned char  ed8051_u8;
typedef unsigned int   ed8051_u16;
typedef unsigned long  ed8051_u32;

#define ED8051_TRUE   1
#define ED8051_FALSE  0

/*** Type prefixes - identical strings to ElegantDebug.h ***********************/

#define ED8051_ERROR_TYPE      "\033[91m\033[1m[ERROR]\033[0m "
#define ED8051_WARNING_TYPE    "\033[93m\033[1m[WARNING]\033[0m "
#define ED8051_INFO_TYPE       "\033[94m\033[1m[INFO]\033[0m "
#define ED8051_OK_TYPE         "\033[92m\033[1m[OK]\033[0m "
#define ED8051_SUCCESS_TYPE    "\033[92m\033[1m[SUCCESS]\033[0m "

#define ED8051_ERROR_TYPE_PLAIN     "[ERROR] "
#define ED8051_WARNING_TYPE_PLAIN   "[WARNING] "
#define ED8051_INFO_TYPE_PLAIN      "[INFO] "
#define ED8051_OK_TYPE_PLAIN        "[OK] "
#define ED8051_SUCCESS_TYPE_PLAIN   "[SUCCESS] "

/*** Default byte transmitter (polled UART mode 1) ****************************/

void ed8051_putc_polling(unsigned char c);

#ifndef ED8051_PUTC
#define ED8051_PUTC(c)   ed8051_putc_polling((unsigned char)(c))
#endif

/*** Milliseconds counter ******************************************************
 * Defined in ElegantDebug_8051.c; incremented by debug_8051_tick().
 * Kept under its own name so that this file can be linked together with the
 * original Src-C/ElegantDebug.c without a duplicate-symbol error.           */

extern volatile ed8051_u32 ed8051_tick_ms;

/*** API **********************************************************************/

/* timestamp: prefix every line with "[hh:mm:ss.mmm] "
 * color    : use ANSI colored type prefixes
 * fileline : include "[file:line] " in warning/error messages
 * (a pointer to a UART handle is meaningless on an 8051 - the chip has one
 *  fixed UART - so debug_init here takes no handle argument) */
void debug_8051_init(ed8051_bool timestamp, ed8051_bool color,
                     ed8051_bool fileline);

/* Call from the 1 ms timer interrupt to drive timestamps. */
void debug_8051_tick(void);

/* Raw text (timestamp only, no type prefix, no newline). */
void debug_8051_log(const char *text);

/* Typed messages: prefix + text. No implicit newline. */
void debug_8051_info(const char *text);
void debug_8051_ok(const char *text);
void debug_8051_success(const char *text);
void debug_8051_warning(const char *file, ed8051_u16 line, const char *text);
void debug_8051_error(const char *file, ed8051_u16 line, const char *text);

/* Convenience: "label" + number + CRLF. */
void debug_8051_log_u32(const char *label, ed8051_u32 value);

/* Runtime switches, same semantics as the original setters. */
void debug_8051_setTimestamp(ed8051_bool enabled);
void debug_8051_setColor(ed8051_bool enabled);

/*** Shortcuts so call sites look like the original ones **********************/

#define DEBUG_8051_WARNING(text)   debug_8051_warning(__FILE__, __LINE__, (text))
#define DEBUG_8051_ERROR(text)     debug_8051_error(__FILE__, __LINE__, (text))

#endif /* __ELEGANT_DEBUG_8051_H */
