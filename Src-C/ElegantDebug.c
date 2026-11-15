/*******************************************************************************
 * @file    ElegantDebug.c
 * @version 1.6
 * @brief   C implementation for ANSI-colored debug logging ¡ª Specially used
 *          for 8051 (STC89C52RC).
 *
 * Implementation notes (8051 constraints):
 *   - 8051 devices have only a few hundred bytes of RAM, so nothing is
 *     buffered: every log function formats straight to vprintf()/putchar().
 *   - Keil C51 has no snprintf/vsnprintf, no variadic macros and its
 *     preprocessor/compiler are C89. Timestamps and file:line prefixes are
 *     emitted with manual decimal conversion (printf-free) ¡ª see the
 *     va_list overlay note below.
 *
 *   IMPORTANT (Keil C51 va_list): the C51 compiler overlays the argument
 *   area of variadic functions with other function calls. Calling
 *   printf()/sprintf() between va_start() and vprintf() corrupts the
 *   argument list (observed as shifted values, e.g. 42 printed as 10752).
 *   The ONLY variadic call in each log function is therefore the final
 *   vprintf(), which runs immediately after va_start().
 *   Also: store int literals in variables before passing them as
 *   variadic arguments (C51 stores small literals in a single byte).
 *
 * Make sure to call `debug_init()` with a valid UART instance before using
 * other functions in this file.
 *
 * @author:
 * @date:
 *
 * @changelog:
 * - (See header file)
 ******************************************************************************/

#include "ElegantDebug.h"



volatile uint32_t DEBUG_IDATA _debug_tick_ms = 0;

static bool DEBUG_IDATA _timestamp_enabled = true;
static bool DEBUG_IDATA _color_enabled = true;
static bool DEBUG_IDATA _filename_line_enabled = false;



void debug_init(bool enable_timestamp, bool enable_color, bool enable_filename_line) {
    _timestamp_enabled = enable_timestamp;
    _color_enabled = enable_color;
    _filename_line_enabled = enable_filename_line;
}

static uint32_t _getTick(void) {
    return _debug_tick_ms;
}



/* ---- 8051 output helpers (printf-free, see header note) -------------- */

static void _putstr(const char *s) {
    while (*s) {
        putchar(*s++);
    }
}

/* decimal number, no padding */
static void _put_udec(uint32_t v) {
    char buf[10];
    unsigned char i = 0;
    do {
        buf[i++] = (char)('0' + (v % 10UL));
        v /= 10UL;
    } while (v != 0UL);
    while (i != 0) {
        putchar(buf[--i]);
    }
}

/* decimal number, zero-padded to `width` digits */
static void _put_udec_pad(uint32_t v, unsigned char width) {
    char buf[10];
    unsigned char i = 0;
    do {
        buf[i++] = (char)('0' + (v % 10UL));
        v /= 10UL;
    } while (v != 0UL);
    while (width > i) {
        putchar('0');
        width--;
    }
    while (i != 0) {
        putchar(buf[--i]);
    }
}

/* print "[hh:mm:ss.mmm] " prefix if timestamps are enabled */
static void _send_timestamp(void) {
    if (_timestamp_enabled) {
        uint32_t ms = _getTick();
        uint32_t s = ms / 1000U;
        putchar('[');
        _put_udec_pad(s / 3600U, 2);
        putchar(':');
        _put_udec_pad((s % 3600U) / 60U, 2);
        putchar(':');
        _put_udec_pad(s % 60U, 2);
        putchar('.');
        _put_udec_pad(ms % 1000U, 3);
        putchar(']');
        putchar(' ');
    }
}

/* print "[file:line] " prefix (also printf-free, see note above) */
static void _send_fileline(const char *file, int line) {
    if (_filename_line_enabled) {
        putchar('[');
        _putstr(file);
        putchar(':');
        _put_udec((uint32_t)line);
        putchar(']');
        putchar(' ');
    }
}



static void _send(const char* text) {

    if (text == NULL) return;

    /* 8051: unbuffered ¡ª send timestamp prefix, then the text byte by byte.
       (The main branch builds a 512-byte buffer here, which does not fit
       in 8051 RAM, and snprintf does not exist on Keil C51.) */
    _send_timestamp();
    _putstr(text);
}



void debug_log(const char* format, ...) {
    va_list args;
    _send_timestamp();
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void debug_logWithType(const char* type, const char* style, const char* format, ...) {
    va_list args;
    _send_timestamp();
    _putstr("\033[1m");
    _putstr(style);
    putchar('[');
    _putstr(type);
    _putstr("]\033[0m ");
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void debug_error_fileline(const char* file, int line, const char* format, ...) {
    va_list args;
    _send_timestamp();
    _putstr(_color_enabled ? ERROR_TYPE : ERROR_TYPE_PLAIN);
    _send_fileline(file, line);
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void debug_warning_fileline(const char* file, int line, const char* format, ...) {
    va_list args;
    _send_timestamp();
    _putstr(_color_enabled ? WARNING_TYPE : WARNING_TYPE_PLAIN);
    _send_fileline(file, line);
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void debug_ok(const char* format, ...) {
    va_list args;
    _send_timestamp();
    _putstr(_color_enabled ? OK_TYPE : OK_TYPE_PLAIN);
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void debug_success(const char* format, ...) {
    va_list args;
    _send_timestamp();
    _putstr(_color_enabled ? SUCCESS_TYPE : SUCCESS_TYPE_PLAIN);
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void debug_info(const char* format, ...) {
    va_list args;
    _send_timestamp();
    _putstr(_color_enabled ? INFO_TYPE : INFO_TYPE_PLAIN);
    va_start(args, format);
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



// runtime helpers for 24?bit ANSI colours; return a pointer to a static buffer
const char* customTextColor(uint8_t r, uint8_t g, uint8_t b) {
    static char DEBUG_IDATA ansi[24];
    /* C51 has no snprintf; the target buffer (24 bytes) always fits */
    sprintf(ansi, "\033[38;2;%u;%u;%um", (unsigned)r, (unsigned)g, (unsigned)b);
    return ansi;
}

const char* customBgColor(uint8_t r, uint8_t g, uint8_t b) {
    static char DEBUG_IDATA ansi[24];
    sprintf(ansi, "\033[48;2;%u;%u;%um", (unsigned)r, (unsigned)g, (unsigned)b);
    return ansi;
}