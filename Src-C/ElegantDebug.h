/*******************************************************************************
 * @file    ElegantDebug.h
 * @version 1.6
 * @brief   ANSI-colored debug logging — STM32 HAL, Renesas RA FSP, TI MSPM0 DL.
 *******************************************************************************/

#ifndef __ELEGANT_DEBUG_H
#define __ELEGANT_DEBUG_H

#ifdef __C51__
    #include <reg52.h>
    typedef unsigned char bool;
    typedef unsigned char uint8_t;
    typedef unsigned long uint32_t;
    #ifndef true
        #define true 1
        #define false 0
    #endif
#else
    #include <stdbool.h>
    #include <stdarg.h>
    #include <stdint.h>
#endif

#include <stdio.h>
#include <string.h>

#define DEBUG_BUFFER_LEN 256

/* ANSI escape codes for output *****************************************/
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

#define COLOR_CUSTOM(r,g,b) customTextColor(r,g,b)

#define BG_RED              "\033[41m"
#define BG_GREEN            "\033[42m"
#define BG_YELLOW           "\033[43m"
#define BG_BLUE             "\033[44m"
#define BG_MAGENTA          "\033[45m"
#define BG_CYAN             "\033[46m"
#define BG_WHITE            "\033[47m"

#define BG_COLOR_CUSTOM(r,g,b) customBgColor(r,g,b)

#define BOLD                "\033[1m"
#define DIM                 "\033[2m"
#define ITALIC              "\033[3m"
#define UNDERLINE           "\033[4m"
#define BLINK               "\033[5m"
#define REVERSE             "\033[7m"
#define CONCEAL             "\033[8m"

#define CLR                 "\033[0m"
#define CLR_TEXT_COLOR      "\033[39m"
#define CLR_BG_COLOR        "\033[49m"
#define CLR_BOLD            "\033[21m"
#define CLR_DIM             "\033[22m"
#define CLR_ITALIC          "\033[23m"
#define CLR_UNDERLINE       "\033[24m"
#define CLR_BLINK           "\033[25m"
#define CLR_REVERSE         "\033[27m"
#define CLR_CONCEAL         "\033[28m"

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

extern volatile uint32_t _debug_tick_ms;

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __C51__
void debug_init(bool enable_timestamp, bool enable_color, bool enable_filename_line);
#else
// 原版的声明原封不动
void debug_init(/*TODO: Params to be added*/);
#endif

static inline void debug_tick(void) {
    _debug_tick_ms++;
}

#ifdef __C51__
// 51 不支持变长参数，这里单独声明
void debug_log(const char* text);
void debug_logWithType(const char* type, const char* style, const char* text);
void debug_ok(const char* text);
void debug_success(const char* text);
void debug_info(const char* text);
void debug_error_fileline(const char* file, int line, const char* text);
void debug_warning_fileline(const char* file, int line, const char* text);
#else
// 原版的声明原封不动
void debug_log(const char* format, ...);
void debug_logWithType(const char* type, const char* style, const char* format, ...);
void debug_ok(const char* format, ...);
void debug_success(const char* format, ...);
void debug_info(const char* format, ...);
void debug_error_fileline(const char* file, int line, const char* format, ...);
void debug_warning_fileline(const char* file, int line, const char* format, ...);
#define debug_error(...)                debug_error_fileline(__FILE__, __LINE__, __VA_ARGS__)
#define debug_warning(...)              debug_warning_fileline(__FILE__, __LINE__, __VA_ARGS__)
#endif

void debug_setTimestampEnabled(bool enabled);
void debug_setColorEnabled(bool enabled);
void debug_setFilenameLineEnabled(bool enabled);

const char* customTextColor(uint8_t r, uint8_t g, uint8_t b);
const char* customBgColor(uint8_t r, uint8_t g, uint8_t b);

#ifdef __cplusplus
}
#endif

#endif // __ELEGANT_DEBUG_H
