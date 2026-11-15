/*******************************************************************************
 * @file    ElegantDebug.c
 * @version 1.6
 * @brief   C implementation for ANSI-colored debug logging — Specially used for 8051 (STC89C52RC).
 *
 * xxx
 *
 * Make sure to initialize UART and call `debug_init()` before using
 * other functions in this file.
 *
 * @author:    daitycos
 * @date:      2026-11-15
 *
 * @changelog:
 * - (See header file)
 ******************************************************************************/

#include "ElegantDebug.h"

#if defined(__C51__)
#include <REG52.H>
#define DEBUG_8051_IDATA idata
#else
#include <8052.h>
#define DEBUG_8051_IDATA __idata
#endif


volatile uint32_t _debug_tick_ms = 0;

static bool _timestamp_enabled = true;
static bool _color_enabled = true;
static bool _filename_line_enabled = false;




void debug_init(bool enable_timestamp, bool enable_color, bool enable_filename_line) {
    _timestamp_enabled = enable_timestamp;
    _color_enabled = enable_color;
    _filename_line_enabled = enable_filename_line;
}

void debug_tick(void) {
    _debug_tick_ms++;
}

static uint32_t _getTick(void) {
    uint32_t ms;
    unsigned char saved_ea = EA;
    EA = 0;
    ms = _debug_tick_ms;
    EA = saved_ea;
    return ms;
}



static void _send(const char* text) {
    if (text == NULL) return;
    while (*text != '\0') {
        putchar(*text++);
    }
}

static void _begin(const char* prefix, const char* file, int line) {
    if (_timestamp_enabled) {
        uint32_t ms = _getTick();
        uint32_t s = ms / 1000UL;
        printf("[%02lu:%02u:%02u.%03u] ", s / 3600UL,
               (unsigned int)((s % 3600UL) / 60UL),
               (unsigned int)(s % 60UL), (unsigned int)(ms % 1000UL));
    }
    _send(prefix);
    if (_filename_line_enabled && file != NULL) {
        printf("[%s:%d] ", file, line);
    }
}



void debug_log(const char* format, ...) {
    va_list args;
    va_start(args, format);
    _begin(NULL, NULL, 0);
    vprintf(format, args);
    va_end(args);
}

void debug_logWithType(const char* type, const char* style, const char* format, ...) {
    va_list args;
    va_start(args, format);
    _begin(NULL, NULL, 0);
    if (_color_enabled) {
        _send(BOLD);
        _send(style);
    }
    _send("[");
    _send(type);
    _send("]");
    if (_color_enabled) _send(CLR);
    _send(" ");
    vprintf(format, args);
    va_end(args);
}

/* void debug_logWithType_fileline(const char* file, int line, const char* type, const char* format, ...) { */
/*     char msg[DEBUG_BUFFER_LEN]; */
/*     va_list args; */
/*     va_start(args, format); */
/*     vsnprintf(msg, sizeof(msg), format, args); */
/*     va_end(args); */
/* */
/*     char combined[DEBUG_BUFFER_LEN + 96]; */
/*     if (_filename_line_enabled) { */
/*         snprintf(combined, sizeof(combined), "\033[1m[%s]\033[0m [%s:%ld] %s", type, file, line, msg); */
/*     } else { */
/*         snprintf(combined, sizeof(combined), "\033[1m[%s]\033[0m %s", type, msg); */
/*     } */
/*     _send(combined); */
/* } */

void debug_error_fileline(const char* file, int line, const char* format, ...) {
    va_list args;
    va_start(args, format);
    _begin(_color_enabled ? ERROR_TYPE : ERROR_TYPE_PLAIN, file, line);
    vprintf(format, args);
    va_end(args);
}

void debug_warning_fileline(const char* file, int line, const char* format, ...) {
    va_list args;
    va_start(args, format);
    _begin(_color_enabled ? WARNING_TYPE : WARNING_TYPE_PLAIN, file, line);
    vprintf(format, args);
    va_end(args);
}

void debug_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    _begin(_color_enabled ? ERROR_TYPE : ERROR_TYPE_PLAIN, NULL, 0);
    vprintf(format, args);
    va_end(args);
}

void debug_warning(const char* format, ...) {
    va_list args;
    va_start(args, format);
    _begin(_color_enabled ? WARNING_TYPE : WARNING_TYPE_PLAIN, NULL, 0);
    vprintf(format, args);
    va_end(args);
}

void debug_ok(const char* format, ...) {
    va_list args;
    va_start(args, format);
    _begin(_color_enabled ? OK_TYPE : OK_TYPE_PLAIN, NULL, 0);
    vprintf(format, args);
    va_end(args);
}

void debug_success(const char* format, ...) {
    va_list args;
    va_start(args, format);
    _begin(_color_enabled ? SUCCESS_TYPE : SUCCESS_TYPE_PLAIN, NULL, 0);
    vprintf(format, args);
    va_end(args);
}

void debug_info(const char* format, ...) {
    va_list args;
    va_start(args, format);
    _begin(_color_enabled ? INFO_TYPE : INFO_TYPE_PLAIN, NULL, 0);
    vprintf(format, args);
    va_end(args);
}



void debug_setTimestampEnabled(bool enabled) {
    _timestamp_enabled = enabled;
}

void debug_setColorEnabled(bool enabled) {
    _color_enabled = enabled;
}

void debug_setFilenameLineEnabled(bool enabled) {
    _filename_line_enabled = enabled;
}



/* runtime helpers for 24‑bit ANSI colours; return a pointer to a static buffer */
const char* customTextColor(uint8_t r, uint8_t g, uint8_t b) {
    static char DEBUG_8051_IDATA ansi[24];
    sprintf(ansi, "\033[38;2;%u;%u;%um", (unsigned int)r, (unsigned int)g, (unsigned int)b);
    return ansi;
}

const char* customBgColor(uint8_t r, uint8_t g, uint8_t b) {
    static char DEBUG_8051_IDATA ansi[24];
    sprintf(ansi, "\033[48;2;%u;%u;%um", (unsigned int)r, (unsigned int)g, (unsigned int)b);
    return ansi;
}
