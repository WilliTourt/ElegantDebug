/*******************************************************************************
 * @file    ElegantDebug.h
 * @version 1.6 - 8051
 * @brief   ANSI-colored debug logging — Specially used for 8051 (STC89C52RC).
 *
 * Requirements:
 *   - 
 *
 * Usage:
 *   - 
 *   - 
 *
 * Notes:
 *   - call `debug_tick()` from a 1 ms timer ISR for timestamps.
 *   - See repository README for examples and integration instructions.
 *
 * @author:    Jian
 * @date:      2026-11-15
 *
 * @changelog:
 * - This version is a branch of main ElegantDebug library, branch date:
 *   2026-11-15
 *******************************************************************************/

#ifndef __ELEGANT_DEBUG_H
#define __ELEGANT_DEBUG_H



/*** Buffer size settings ***********************************************/

// [C51 Adaptation] Reduced buffer size to fit the limited RAM of 8051.
#define DEBUG_BUFFER_LEN 32

/************************************************************************/

/*** Platform-specific includes *****************************************/

#ifdef __C51__
    #include <reg52.h>
    #include <stdarg.h>
    #include <stdio.h>
    #include <string.h>
    typedef unsigned char bool;
    #define true 1
    #define false 0
    typedef unsigned long uint32_t;
    typedef unsigned int uint16_t;
    typedef unsigned char uint8_t;
    #define inline
    #define DBG_BUF_MEM xdata // Force XDATA to prevent internal RAM overflow
#else
    #include <stdbool.h>
    #include <stdarg.h>
    #include <stdint.h>
    #include <stdio.h>
    #include <string.h>
    #define DBG_BUF_MEM
#endif

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



// tick provider (User must feed from timer ISR)
extern volatile uint32_t _debug_tick_ms;



// Initialize the library; MUST be called before other functions.
void debug_init(void);

// [C51 Adaptation] C51 does not support the 'inline' keyword, using macro instead.
#define debug_tick()  (_debug_tick_ms++)

// Basic formatted log
void debug_log(const char* format, ...);

// Log with a type prefix
void debug_logWithType(const char* type, const char* style, const char* format, ...);

// Convenience helpers
void debug_ok(const char* format, ...);
void debug_success(const char* format, ...);
void debug_info(const char* format, ...);

// Variants that accept file and line (used by macros to automatically pass __FILE__/__LINE__)
// void debug_logWithType_fileline(const char* file, int line, const char* type, const char* format, ...);
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

// Macros to automatically pass caller file/line
// [C51 Adaptation] C51 does not support variadic macros, so they are disabled for 8051.
#ifndef __C51__
#define debug_error(...)                debug_error_fileline(__FILE__, __LINE__, __VA_ARGS__)
#define debug_warning(...)              debug_warning_fileline(__FILE__, __LINE__, __VA_ARGS__)
// #define debug_logWithType(type, ...) debug_logWithType_fileline(__FILE__, __LINE__, (type), __VA_ARGS__)
#endif



#endif // __ELEGANT_DEBUG_H
