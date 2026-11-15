/*******************************************************************************
 * @file    ElegantDebug.h
 * @version 1.6 - 8051
 * @brief   ANSI-colored debug logging ¡ª Specially used for 8051 (STC89C52RC).
 *
 * Requirements:
 *   - A user-provided putchar() implementation (standard printf retargeting).
 *     The UART itself must be initialized by the user, e.g. SCON/TMOD/TH1
 *     for 9600 baud @ 11.0592 MHz.
 *   - Keil C51 (C89): no stdbool.h / stdint.h / snprintf / variadic macros
 *     are available, so this branch defines its own minimal types and avoids
 *     those features entirely.
 *
 * Usage:
 *   - Call debug_init(enable_timestamp, enable_color, enable_filename_line)
 *     before any other debug_* function.
 *   - Call debug_tick() from a 1 ms timer ISR to enable timestamps.
 *   - debug_error/debug_warning macros are NOT available (C89 preprocessor
 *     has no variadic macros); call debug_error_fileline(__FILE__, __LINE__,
 *     ...) / debug_warning_fileline(...) directly instead.
 *   - Avoid %f in format strings: floating-point formatting is very
 *     expensive on 8051.
 *
 * Notes:
 *   - call `debug_tick()` from a 1 ms timer ISR for timestamps.
 *   - See repository README for examples and integration instructions.
 *
 * @author:
 * @date:      2026-XX-XX
 *
 * @changelog:
 * - This version is a branch of main ElegantDebug library, branch date:
 *   2026-11-15
 * - Implemented the 8051 port: unbuffered output via vprintf/putchar
 *   (the 256-byte stack buffers of the main branch do not fit in 8051 RAM
 *   and C51 has no snprintf/vsnprintf).
 *******************************************************************************/

#ifndef __ELEGANT_DEBUG_H
#define __ELEGANT_DEBUG_H



/*** Buffer size settings ***********************************************/

#define DEBUG_BUFFER_LEN 256

/************************************************************************/

/*** Platform-specific includes *****************************************/

/* Keil C51 is C89: no stdbool.h / stdint.h. Provide minimal types. */
typedef unsigned char  bool;
typedef unsigned char  uint8_t;
typedef unsigned int   uint16_t;
typedef unsigned long  uint32_t;
#define true  1
#define false 0

/* Keil C51 uses `idata`; SDCC uses `__idata`. Library statics go to idata
   so the tiny 128-byte DATA segment stays free for the user program. */
#ifdef __SDCC
#define DEBUG_IDATA __idata
#else
#define DEBUG_IDATA idata
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



// 8051 tick provider (User must feed from timer ISR)
extern volatile uint32_t DEBUG_IDATA _debug_tick_ms;

/* 8051 tick provider (User must feed from timer ISR).
   Not `inline`: Keil C51 is C89 and has no inline keyword. */
static void debug_tick() {
    _debug_tick_ms++;
}



#ifdef __cplusplus
extern "C" {
#endif



/* Initialize the library; MUST be called before other functions.
   The 8051 UART must already be initialized by the user, and a putchar()
   implementation must be provided for output. */
void debug_init(bool enable_timestamp, bool enable_color, bool enable_filename_line);

// Basic formatted log
void debug_log(const char* format, ...);

// Log with a type prefix
void debug_logWithType(const char* type, const char* style, const char* format, ...);

// Convenience helpers
void debug_ok(const char* format, ...);
void debug_success(const char* format, ...);
void debug_info(const char* format, ...);

// Variants that accept file and line (use these directly on 8051: the C89
// preprocessor has no variadic macros, so debug_error/debug_warning macros
// are not available in this branch)
void debug_error_fileline(const char* file, int line, const char* format, ...);
void debug_warning_fileline(const char* file, int line, const char* format, ...);

// Runtime setters
void debug_setTimestampEnabled(bool enabled);
void debug_setColorEnabled(bool enabled);
// Enable/disable showing filename:line when using the file/line variants or macros
void debug_setFilenameLineEnabled(bool enabled);

// helper functions generating ANSI escape sequences for variable 24-bit colors
// they return a pointer to a static string which remains valid until the
// next call. This allows using them inside printf/format strings just like
// the `COLOR_CUSTOM`/`BG_COLOR_CUSTOM` macros but with runtime values.
const char* customTextColor(uint8_t r, uint8_t g, uint8_t b);
const char* customBgColor(uint8_t r, uint8_t g, uint8_t b);

#ifdef __cplusplus
}
#endif

#endif // __ELEGANT_DEBUG_H