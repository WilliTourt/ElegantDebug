/*******************************************************************************
 * @file    ElegantDebug.h
 * @version 1.6
 * @brief   ANSI-colored debug logging — STM32 HAL, Renesas RA FSP, TI MSPM0 DL.
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
 * 
 * Usage:
 *   - Define USE_STM32_HAL, USE_RA_FSP, or USE_TI_MSPM0 before including this header.
 *   - STM32:  pass a `UART_HandleTypeDef*` or enable USB-CDC class.
 *   - RA FSP: pass a `uart_instance_t const*`.
 *   - TI MSPM0: pass a `UART_Regs*`.
 *   - Call `log`, `info`, `error`, etc. to print messages.
 *
 * Notes:
 *   - RA FSP / TI MSPM0: call `debug_tick()` from a 1 ms timer ISR for timestamps.
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
 *
 *******************************************************************************/

#ifndef __ELEGANT_DEBUG_H
#define __ELEGANT_DEBUG_H



/*** Platform & Port selection ******************************************/

// UNCOMMENT one of these macros before including this header:

// #define USE_STM32_HAL    // STM32Cube HAL
// #define USE_RA_FSP       // Renesas RA FSP
// #define USE_TI_MSPM0_DL  // TI MSPM0 DL (Driver Library)
// #define USE_8051_C51     // 8051 (Keil C51)


// USB / UART choices:
// Set to 1 to use USB-CDC as debug port. 0 for UART
// USB FUNCTION ARE CURRENTLY ONLY AVAILABLE FOR STM32
#define USB_AS_DEBUG_PORT false

/************************************************************************/


/*** Buffer size settings ***********************************************/

#if defined(USE_8051_C51)
    #define DEBUG_BUFFER_LEN      16
    #define DEBUG_COMBINED_32     24
    #define DEBUG_COMBINED_96     32
#else
    #define DEBUG_BUFFER_LEN      256
    #define DEBUG_COMBINED_32     (DEBUG_BUFFER_LEN + 32)
    #define DEBUG_COMBINED_96     (DEBUG_BUFFER_LEN + 96)
#endif

/************************************************************************/



#if defined(USE_STM32_HAL)
    #define DEBUG_PLATFORM_STM32  1
    #define DEBUG_PLATFORM_RA     0
    #define DEBUG_PLATFORM_TI     0
#elif defined(USE_8051_C51)
    #define DEBUG_PLATFORM_STM32  0
    #define DEBUG_PLATFORM_RA     0
    #define DEBUG_PLATFORM_TI     0
    #define DEBUG_PLATFORM_8051   1
#elif defined(USE_RA_FSP)
    #define DEBUG_PLATFORM_STM32  0
    #define DEBUG_PLATFORM_RA     1
    #define DEBUG_PLATFORM_TI     0
#elif defined(USE_TI_MSPM0_DL)
    #define DEBUG_PLATFORM_STM32  0
    #define DEBUG_PLATFORM_RA     0
    #define DEBUG_PLATFORM_TI     1
#else
    #error "Please define USE_8051_C51 / USE_STM32_HAL / USE_RA_FSP / USE_TI_MSPM0_DL macro before including ElegantDebug.h"
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


#if defined(USE_8051_C51)
    #include <reg52.h>
    #include <stdarg.h>
    #include <stddef.h>
    #include <stdio.h>
    #include <string.h>
    
    #ifndef true
        #define true   1
        #define false  0
    #endif
    typedef unsigned char bool;

    typedef unsigned char uint8_t;
    typedef unsigned int uint16_t;
    typedef unsigned long uint32_t;
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



// RA FSP / TI MSPM0 tick provider (User must feed from timer ISR)
#if (DEBUG_PLATFORM_RA || DEBUG_PLATFORM_TI || DEBUG_PLATFORM_8051)
extern volatile uint32_t _debug_tick_ms;
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
void debug_init(bool enable_timestamp, bool enable_color, bool enable_filename_line);
#endif

#if (DEBUG_PLATFORM_RA || DEBUG_PLATFORM_TI)
// RA FSP tick provider (User must feed from timer ISR)
static inline void debug_tick() {
    _debug_tick_ms++;
}
#elif DEBUG_PLATFORM_8051
#define debug_tick()    (_debug_tick_ms++)
#endif

#if DEBUG_PLATFORM_RA
void debug_onTxComplete(uart_callback_args_t *p_args);
#endif

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

#if DEBUG_PLATFORM_8051
void debug_error(const char* format, ...);
void debug_warning(const char* format, ...);
#else
// Macros to automatically pass caller file/line
#define debug_error(...)                debug_error_fileline(__FILE__, __LINE__, __VA_ARGS__)
#define debug_warning(...)              debug_warning_fileline(__FILE__, __LINE__, __VA_ARGS__)
#endif
// #define debug_logWithType(type, ...) debug_logWithType_fileline(__FILE__, __LINE__, (type), __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif // __ELEGANT_DEBUG_H
