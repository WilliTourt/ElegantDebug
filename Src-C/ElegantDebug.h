/*******************************************************************************
 * @file    ElegantDebug.h
 * @version 1.7
 * @brief   ANSI-colored debug logging — STM32 HAL, Renesas RA FSP, TI MSPM0 DL, 8051.
 *
 * This header provides a small C API that mirrors the C++
 * `ElegantDebug` class.
 * Output over UART (or USB-CDC on STM32), with optional timestamps and ANSI
 * color prefixes.
 *
 * Requirements:
 *  - STM32: STM32Cube HAL UART driver and `HAL_UART_MODULE_ENABLED` required.
 *  - Renesas RA: RASC-generated `hal_data.h` with an SCI UART stack.
 *  - TI MSPM0: sysconfig-generated `ti_msp_dl_config.h` with a UART stack configured.
 *  - 8051: Keil C51 (C51.EXE) or any C90 compiler for 8051; no vendor HAL is
 *    needed, output goes through a user-supplied byte transmit function.
 * 
 * Usage:
 *   - Define USE_STM32_HAL, USE_RA_FSP, USE_TI_MSPM0_DL, or USE_8051_C51 before
 *     including this header.
 *   - STM32:  pass a `UART_HandleTypeDef*` or enable USB-CDC class.
 *   - RA FSP: pass a `uart_instance_t const*`.
 *   - TI MSPM0: pass a `UART_Regs*`.
 *   - 8051: pass a `void (*)(char)` function that pushes one byte to your UART.
 *   - Call `log`, `info`, `error`, etc. to print messages.
 *
 * Notes:
 *   - RA FSP / TI MSPM0 / 8051: call `debug_tick()` from a 1 ms timer ISR for timestamps.
 *   - 8051: `DEBUG_BUFFER_LEN` defaults to 32 because of the small internal RAM,
 *     and `%f` is converted by a small built-in formatter (see ElegantDebug.c).
 *   - See repository README for examples and integration instructions.
 *
 * @author:    WilliTourt <willitourt@foxmail.com>
 * @date:      2026-08-09
 *
 * @changelog:
 * - 2025-12-10: Initial release.
 * - 2025-12-11: Added support for filename and ln number in warning/error messages.
 *               Allowed custom styles for type prefix in logWithType().
 *               Added more ANSI escape codes for color/style.
 * - 2026-02-22: Added USB-CDC support; can redirect output to USB when
 *               USB_AS_DEBUG_PORT macro set to 1. See C++ counterpart for details.
 * - 2026-03-01: Replaced `COLOR_CUSTOM(r,g,b)` macro implementation with
 *               `customTextColor(r,g,b)` public method for runtime color values.
 *               Background colors too.
 * - 2026-07-16: Added Renesas RA FSP support (USE_RA_FSP).
 * - 2026-07-24: Added support for TI MSPM0 series.
 * - 2026-08-09: Optimized RA FSP tx function. Now you MUST call
 *               debug_onTxComplete(uart_callback_args_t *p_args) to set _txDone flag.
 * - 2026-11-13: Added 8051 (Keil C51) support (USE_8051_C51). Output is streamed
 *               byte by byte through a user supplied putchar-style function, and
 *               a small bounded formatter replaces the missing snprintf() so the
 *               library fits in the 8051 internal RAM.
 *
 *******************************************************************************/

#ifndef __ELEGANT_DEBUG_H
#define __ELEGANT_DEBUG_H



/*** Platform & Port selection ******************************************/

// UNCOMMENT one of these macros before including this header:

// #define USE_STM32_HAL    // STM32Cube HAL
// #define USE_RA_FSP       // Renesas RA FSP
// #define USE_TI_MSPM0_DL  // TI MSPM0 DL (Driver Library)
// #define USE_8051_C51     // 8051 (Keil C51 / 8052, e.g. AT89C52, STC89) 



// USB / UART choices:
// Set to 1 to use USB-CDC as debug port. 0 for UART
// USB FUNCTION ARE CURRENTLY ONLY AVAILABLE FOR STM32
#define USB_AS_DEBUG_PORT false

/************************************************************************/


/*** Buffer size settings ***********************************************/

#if defined(USE_8051_C51)
// 8051 devices only have 128 / 256 bytes of internal RAM. Keep the message
// buffer small by default; raise it if your device has RAM to spare (or define
// DEBUG_BUFFER_LEN yourself before including this header).
#ifndef DEBUG_BUFFER_LEN
#define DEBUG_BUFFER_LEN 32
#endif

// Memory space of the internal buffers. Empty = DATA (works on any 8051).
// On an 8052 / AT89S52 / STC89C52 (256 bytes of internal RAM) define
//     #define DEBUG_8051_BUF_MEM idata
// before including this header to move them into the upper 128 bytes, and on a
// part with external RAM define it as `xdata` instead. That frees the DATA area
// for your own variables and allows a much larger DEBUG_BUFFER_LEN.
#ifndef DEBUG_8051_BUF_MEM
#define DEBUG_8051_BUF_MEM
#endif

// Set to 0 to drop the %f / %e / %g conversions from the small 8051 formatter.
// Floating point support costs roughly 2 KB of code space, which matters on the
// 8 KB flash parts; with 1 (the default) the behaviour matches the other
// platforms.
#ifndef DEBUG_8051_ENABLE_FLOAT
#define DEBUG_8051_ENABLE_FLOAT 1
#endif
#else
#define DEBUG_BUFFER_LEN 256
#endif

/************************************************************************/



#if defined(USE_STM32_HAL)
    #define DEBUG_PLATFORM_STM32  1
    #define DEBUG_PLATFORM_RA     0
    #define DEBUG_PLATFORM_TI     0
    #define DEBUG_PLATFORM_8051   0
#elif defined(USE_RA_FSP)
    #define DEBUG_PLATFORM_STM32  0
    #define DEBUG_PLATFORM_RA     1
    #define DEBUG_PLATFORM_TI     0
    #define DEBUG_PLATFORM_8051   0
#elif defined(USE_TI_MSPM0_DL)
    #define DEBUG_PLATFORM_STM32  0
    #define DEBUG_PLATFORM_RA     0
    #define DEBUG_PLATFORM_TI     1
    #define DEBUG_PLATFORM_8051   0
#elif defined(USE_8051_C51)
    #define DEBUG_PLATFORM_STM32  0
    #define DEBUG_PLATFORM_RA     0
    #define DEBUG_PLATFORM_TI     0
    #define DEBUG_PLATFORM_8051   1
#else
    #error "Please define USE_STM32_HAL / USE_RA_FSP / USE_TI_MSPM0_DL / USE_8051_C51 macro before including ElegantDebug.h"
#endif

/*** Platform-specific includes *****************************************/

#if DEBUG_PLATFORM_STM32
    #include "main.h"

    #if defined(HAL_UART_MODULE_ENABLED)
        #include "usart.h"
    #else
        #error "At least one serial port should be opened"
    #endif

    #if (USB_AS_DEBUG_PORT == 1)
        #include "usbd_cdc_if.h"
    #endif

    #if !defined(__STM32F0xx_HAL_H) && \
        !defined(__STM32F1xx_HAL_H) && \
        !defined(__STM32F2xx_HAL_H) && \
        !defined(__STM32F3xx_HAL_H) && \
        !defined(__STM32F4xx_HAL_H) && \
        !defined(__STM32F7xx_HAL_H) && \
        !defined(__STM32H5xx_HAL_H) && \
        !defined(__STM32H7xx_HAL_H) && \
        !defined(__STM32L0xx_HAL_H) && \
        !defined(__STM32L1xx_HAL_H) && \
        !defined(__STM32L4xx_HAL_H) && \
        !defined(__STM32G0xx_HAL_H) && \
        !defined(__STM32G4xx_HAL_H) && \
        !defined(__STM32WBxx_HAL_H) && \
        !defined(__STM32WLxx_HAL_H) && \
        !defined(__STM32MP1xx_HAL_H) && \
        !defined(STM32C0xx_HAL_H)
    #error "This debugging library can only be used with STM32Cube HAL drivers"
    #endif
    // If you encounter this compilation error when including this library
    // in a normal HAL project, it might be because I haven't fully listed
    // all the macros of the STM32 series. If you encounter any, you are
    // welcome to submit a PR or issue!

#endif

#if DEBUG_PLATFORM_RA
    #include "hal_data.h"

    #if !defined(R_SCI_UART_H) && !defined(R_SCI_B_UART_H)
        #error "At least one SCI UART port should be configured in RASC"
    #endif

#endif

#if DEBUG_PLATFORM_TI
    #include "ti_msp_dl_config.h"

    #if !defined(DL_UART_Main_init)
        #error "At least one UART port should be configured in sysconfig"
    #endif
#endif

#if DEBUG_PLATFORM_8051
    // No vendor HAL is required on 8051: the byte output function is supplied by
    // the application through debug_init(). Include your MCU header (REG52.H,
    // stc8.h, N76E003.H, ...) in your own sources where you need SFR access.
#endif



#if (DEBUG_PLATFORM_8051 && defined(__C51__))
    // Keil C51 ships neither <stdbool.h> nor <stdint.h>; it has no 64-bit types
    // either. Provide the few standard types this library needs.
    #include <stdarg.h>
    #include <stdio.h>
    #include <string.h>
    #include <stddef.h>

    typedef unsigned char  uint8_t;
    typedef unsigned int   uint16_t;
    typedef unsigned long  uint32_t;
    typedef unsigned char  bool;

    #define true   1
    #define false  0
#else
    #include <stdbool.h>
    #include <stdarg.h>
    #include <stdint.h>
    #include <stdio.h>
    #include <string.h>
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



// RA FSP / TI MSPM0 / 8051 tick provider (User must feed from timer ISR)
#if (DEBUG_PLATFORM_RA || DEBUG_PLATFORM_TI || DEBUG_PLATFORM_8051)
extern volatile uint32_t _debug_tick_ms;
#endif



#if (DEBUG_PLATFORM_8051 && defined(__C51__))
    // Keil C51 keeps a fixed parameter block in DATA for every non-reentrant
    // variadic function. On a device with 128 bytes of RAM that alone would not
    // fit, so the API is declared `reentrant` on 8051: parameters and locals
    // then live on the stack and are shared between all call sites.
    #define DEBUG_REENTRANT  reentrant
#else
    #define DEBUG_REENTRANT
#endif

#ifdef __cplusplus
extern "C" {
#endif



#if DEBUG_PLATFORM_STM32
// Initialize the library; MUST be called before other functions.
//
// If `USB_AS_DEBUG_PORT` is set to 1 (USB-CDC output enabled at compile time),
// the `huart` pointer is ignored and may be passed as NULL. When using UART as
// the debug port, a valid `UART_HandleTypeDef *` must be provided or the
// functions will no-op.
void debug_init(UART_HandleTypeDef *huart, bool enable_timestamp, bool enable_color, bool enable_filename_line);
#elif DEBUG_PLATFORM_RA
// Initialize the library; MUST be called before other functions.
void debug_init(uart_instance_t const *uart, bool enable_timestamp, bool enable_color, bool enable_filename_line);
#elif DEBUG_PLATFORM_TI
// Initialize the library; MUST be called before other functions.
void debug_init(UART_Regs *uart_inst, bool enable_timestamp, bool enable_color, bool enable_filename_line);
#elif DEBUG_PLATFORM_8051
// Byte output function used by the 8051 port: called once per character and it
// must block until the character has been queued in the UART, e.g.
//   void my_putc(char c) { SBUF = c; while (!TI); TI = 0; }
typedef void (*debug_putc_t)(char c);

// Initialize the library; MUST be called before other functions.
// `uart_putc` may be NULL, in which case every log call does nothing.
void debug_init(debug_putc_t uart_putc, bool enable_timestamp, bool enable_color, bool enable_filename_line);
#endif

#if (DEBUG_PLATFORM_RA || DEBUG_PLATFORM_TI)
// RA FSP tick provider (User must feed from timer ISR)
static inline void debug_tick() {
    _debug_tick_ms++;
}
#elif DEBUG_PLATFORM_8051
// 8051 tick provider (User must feed from a 1 ms timer ISR).
// Keil C51 has no `inline` keyword, so a plain static function is used.
static void debug_tick(void) {
    _debug_tick_ms++;
}
#endif

#if DEBUG_PLATFORM_RA
void debug_onTxComplete(uart_callback_args_t *p_args);
#endif

// Basic formatted log
void debug_log(const char* format, ...) DEBUG_REENTRANT;

// Log with a type prefix
void debug_logWithType(const char* type, const char* style, const char* format, ...) DEBUG_REENTRANT;

// Convenience helpers
void debug_ok(const char* format, ...) DEBUG_REENTRANT;
void debug_success(const char* format, ...) DEBUG_REENTRANT;
void debug_info(const char* format, ...) DEBUG_REENTRANT;

// Variants that accept file and line (used by macros to automatically pass __FILE__/__LINE__)
// void debug_logWithType_fileline(const char* file, int line, const char* type, const char* format, ...);
void debug_error_fileline(const char* file, int line, const char* format, ...) DEBUG_REENTRANT;
void debug_warning_fileline(const char* file, int line, const char* format, ...) DEBUG_REENTRANT;

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
#if DEBUG_PLATFORM_8051
    // Errors / warnings without a location. Keil C51 is a C90 compiler and has
    // no `__VA_ARGS__`, so on 8051 the convenient macros below cannot append
    // __FILE__/__LINE__; debug_error() and debug_warning() are mapped onto these
    // variants. Call debug_error_fileline(__FILE__, __LINE__, ...) directly when
    // the file name and line number are wanted.
    void debug_error_plain(const char* format, ...) DEBUG_REENTRANT;
    void debug_warning_plain(const char* format, ...) DEBUG_REENTRANT;
#endif

#if (DEBUG_PLATFORM_8051 && defined(__C51__))
    #define debug_error                     debug_error_plain
    #define debug_warning                   debug_warning_plain
#else
    #define debug_error(...)                debug_error_fileline(__FILE__, __LINE__, __VA_ARGS__)
    #define debug_warning(...)              debug_warning_fileline(__FILE__, __LINE__, __VA_ARGS__)
#endif
// #define debug_logWithType(type, ...) debug_logWithType_fileline(__FILE__, __LINE__, (type), __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif // __ELEGANT_DEBUG_H
