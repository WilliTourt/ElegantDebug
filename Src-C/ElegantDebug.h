/*******************************************************************************
 * @file    ElegantDebug.h
 * @version 1.6 - 8051
 * @brief   ANSI-colored debug logging — Specially used for 8051 (STC89C52RC).
 *
 * Requirements:
 *   - Keil C51 (C90), SMALL memory model. Tested with C51 V9.54 / BL51 V6.22.
 *   - A UART already initialized by the application (9600 8N1 at 11.0592 MHz
 *     is the usual setup). The library does not configure the baud rate.
 *   - A TX path: by default ElegantDebug.c implements `char putchar(char c)`
 *     with SBUF/TI polling. If the project already redirects `putchar`,
 *     define DEBUG_EXTERNAL_PUTCHAR in the project's C51 preprocessor symbols.
 *
 * Usage:
 *   - debug_init(true, true, false);  // timestamp / color / file:line
 *   - call debug_tick() from a 1 ms timer ISR for timestamps.
 *   - See README-zh-CN.md / README.md for examples and integration instructions.
 *
 * Notes:
 *   - C51 has no <stdbool.h>, no <stdint.h>, no `inline` and no variadic
 *     macros; the types and the tick helper are provided here.
 *   - Output is streamed through putchar()/vprintf(), so no message buffer is
 *     allocated and DEBUG_BUFFER_LEN does not reserve RAM.
 *
 * @author:    Fan Yaoyang
 * @date:      2026-11-15
 *
 * @changelog:
 * - This version is a branch of main ElegantDebug library, branch date:
 *   2026-11-15
 *******************************************************************************/

#ifndef __ELEGANT_DEBUG_H
#define __ELEGANT_DEBUG_H



/*** Buffer size settings ***********************************************/

/* Kept for source compatibility with the main branch. This port formats and
 * transmits on the fly, so no buffer of this size is allocated. */
#define DEBUG_BUFFER_LEN 256

/************************************************************************/

/*** Platform-specific includes *****************************************/

/* Keil C51 (C90) has neither <stdbool.h> nor <stdint.h>.
 * C51 data sizes: char = 8 bit, int = 16 bit, long = 32 bit. */
typedef unsigned char bool;
typedef unsigned char uint8_t;
typedef unsigned long uint32_t;

#ifndef true
#define true  1
#endif
#ifndef false
#define false 0
#endif

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ANSI escape codes for output *****************************************/

// Colors for texts
#define COLOR_DARK_RED      "\033[31m"
#define COLOR_DARK_GREEN    "\033[32m"
#define COLOR_DARK_YELLOW   "\033[33m"
#define COLOR_DARK_BLUE     "\033[34m"
#define COLOR_DARK_MAGENTA  "\033[35m"
#define COLOR_DARK_CYAN     "\033[36m"
#define COLOR_DARK_WHITE    "\033[37m"

#define COLOR_RED           "\033[91m"
#define COLOR_GREEN         "\033[92m"
#define COLOR_YELLOW        "\033[93m"
#define COLOR_BLUE          "\033[94m"
#define COLOR_MAGENTA       "\033[95m"
#define COLOR_CYAN          "\033[96m"
#define COLOR_WHITE         "\033[97m"

// Custom 24-bit colors for text.
#define COLOR_CUSTOM(r,g,b) customTextColor(r,g,b)

// Background colors
#define BG_RED              "\033[41m"
#define BG_GREEN            "\033[42m"
#define BG_YELLOW           "\033[43m"
#define BG_BLUE             "\033[44m"
#define BG_MAGENTA          "\033[45m"
#define BG_CYAN             "\033[46m"
#define BG_WHITE            "\033[47m"

// #define BG_COLOR_CUSTOM(r,g,b) "\033[48;2;" #r ";" #g ";" #b "m" // Custom 24-bit background colors
#define BG_COLOR_CUSTOM(r,g,b) customBgColor(r,g,b)

// Styles
#define BOLD                "\033[1m"
#define DIM                 "\033[2m"
#define ITALIC              "\033[3m"
#define UNDERLINE           "\033[4m"
#define BLINK               "\033[5m"
#define REVERSE             "\033[7m"
#define CONCEAL             "\033[8m"

// Clear specific color/styles
#define CLR                 "\033[0m"   // Clear color and style of text before this macro
#define CLR_TEXT_COLOR      "\033[39m"
#define CLR_BG_COLOR        "\033[49m"
#define CLR_BOLD            "\033[21m"
#define CLR_DIM             "\033[22m"
#define CLR_ITALIC          "\033[23m"
#define CLR_UNDERLINE       "\033[24m"
#define CLR_BLINK           "\033[25m"
#define CLR_REVERSE         "\033[27m"
#define CLR_CONCEAL         "\033[28m"

// Prefixes
#define ERROR_TYPE          "\033[91m\033[1m[ERROR]\033[0m "
#define WARNING_TYPE        "\033[93m\033[1m[WARNING]\033[0m "
#define INFO_TYPE           "\033[94m\033[1m[INFO]\033[0m "
#define OK_TYPE             "\033[92m\033[1m[OK]\033[0m "
#define SUCCESS_TYPE        "\033[92m\033[1m[SUCCESS]\033[0m "

#define ERROR_TYPE_PLAIN    "[ERROR] "
#define WARNING_TYPE_PLAIN  "[WARNING] "
#define INFO_TYPE_PLAIN     "[INFO] "
#define OK_TYPE_PLAIN       "[OK] "
#define SUCCESS_TYPE_PLAIN  "[SUCCESS] "

/************************************************************************/



// tick provider (User must feed from timer ISR).
// Explicitly `data`: it is incremented from the 1 ms ISR and must stay
// directly addressable, independent of the project's memory model. The
// declaration and the definition in ElegantDebug.c must use the same memory
// type, otherwise BL51 reports L103.
extern volatile uint32_t data _debug_tick_ms;

// Initialize the library; MUST be called before other functions.
// The UART itself must be initialized by the application beforehand.
void debug_init(bool enable_timestamp, bool enable_color, bool enable_filename_line);

// C51 has no `inline`; a macro keeps the ISR cost down to one 32-bit
// increment. Call it from a 1 ms timer ISR:
//     void Timer0_ISR(void) interrupt 1 { ... debug_tick(); }
#define debug_tick()        (_debug_tick_ms++)

// Basic formatted log
void debug_log(const char* format, ...);

// Log with a type prefix
void debug_logWithType(const char* type, const char* style, const char* format, ...);

// Convenience helpers
void debug_ok(const char* format, ...);
void debug_success(const char* format, ...);
void debug_info(const char* format, ...);

// Convenience helpers with the type prefix but without file/line
void debug_error(const char* format, ...);
void debug_warning(const char* format, ...);

// Variants that accept file and line
// void debug_logWithType_fileline(const char* file, int line, const char* type, const char* format, ...);
void debug_error_fileline(const char* file, int line, const char* format, ...);
void debug_warning_fileline(const char* file, int line, const char* format, ...);

// Runtime setters
void debug_setTimestampEnabled(bool enabled);
void debug_setColorEnabled(bool enabled);
// Enable/disable showing filename:line when using the file/line variants
void debug_setFilenameLineEnabled(bool enabled);

// helper functions generating ANSI escape sequences for variable 24-bit colors
// they return a pointer to a static string which remains valid until the
// next call. This allows using them inside printf/format strings just like
// the `COLOR_CUSTOM`/`BG_COLOR_CUSTOM` macros but with runtime values.
const char* customTextColor(uint8_t r, uint8_t g, uint8_t b);
const char* customBgColor(uint8_t r, uint8_t g, uint8_t b);

// C51 (C90) has no variadic macros, so debug_error() and debug_warning()
// cannot capture __FILE__/__LINE__ automatically as they do on the main
// branch. Call debug_error_fileline(__FILE__, __LINE__, ...) /
// debug_warning_fileline(__FILE__, __LINE__, ...) when the location is needed.



#endif // __ELEGANT_DEBUG_H
