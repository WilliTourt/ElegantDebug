/*******************************************************************************
 * @file    ElegantDebug.c
 * @version 1.6 - 8051
 * @brief   C implementation for ANSI-colored debug logging — Specially used for
 *          8051 (STC89C52RC).
 *
 * xxx
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



volatile uint32_t _debug_tick_ms = 0;

static bool _timestamp_enabled = true;
static bool _color_enabled = true;
static bool _filename_line_enabled = false;

static char xdata _dbg_msg[DEBUG_BUFFER_LEN];
static char xdata _ansi[24];



void debug_init(bool enable_timestamp, bool enable_color, bool enable_filename_line) {
    _timestamp_enabled = enable_timestamp;
    _color_enabled = enable_color;
    _filename_line_enabled = enable_filename_line;
}

static uint32_t _getTick(void) {
    return _debug_tick_ms;
}



static void _putc(char c) {
    SBUF = c;
    while (!TI);
    TI = 0;
}

static void _puts(const char* s) {
    while (*s) {
        _putc(*s);
        s++;
    }
}

static void _put_uint(uint16_t v) {
    char tmp[5];
    uint8_t n = 0;

    if (v == 0) {
        tmp[n] = '0';
        n++;
    }
    while (v) {
        tmp[n] = (char)('0' + (v % 10U));
        n++;
        v = (uint16_t)(v / 10U);
    }
    while (n) {
        n--;
        _putc(tmp[n]);
    }
}

static void _timestamp(void) {
    uint32_t ms;
    uint32_t s;
    uint32_t frac;

    if (!_timestamp_enabled) return;

    ms = _getTick();
    s = ms / 1000U;
    frac = ms % 1000U;

    _putc('[');
    _putc((char)('0' + (s / 36000U) % 10U));
    _putc((char)('0' + (s / 3600U) % 10U));
    _putc(':');
    _putc((char)('0' + (s / 600U) % 10U));
    _putc((char)('0' + (s / 60U) % 10U));
    _putc(':');
    _putc((char)('0' + (s / 10U) % 10U));
    _putc((char)('0' + s % 10U));
    _putc('.');
    _putc((char)('0' + frac / 100U));
    _putc((char)('0' + (frac / 10U) % 10U));
    _putc((char)('0' + frac % 10U));
    _putc(']');
    _putc(' ');
}



static void _send(const char* text) {

    if (text == NULL) return;

    _timestamp();
    _puts(text);
}

static void _send_type(const char* prefix, const char* text) {

    if (text == NULL) return;

    _timestamp();
    _puts(prefix);
    _puts(text);
}



void debug_log(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsprintf(_dbg_msg, format, args);
    va_end(args);
    _send(_dbg_msg);
}

void debug_logWithType(const char* type, const char* style, const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsprintf(_dbg_msg, format, args);
    va_end(args);

    _timestamp();
    _puts(BOLD);
    _puts(style);
    _putc('[');
    _puts(type);
    _putc(']');
    _puts(CLR);
    _putc(' ');
    _puts(_dbg_msg);
}

void debug_error_fileline(const char* file, int line, const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsprintf(_dbg_msg, format, args);
    va_end(args);

    _timestamp();
    _puts(_color_enabled ? ERROR_TYPE : ERROR_TYPE_PLAIN);
    if (_filename_line_enabled) {
        _putc('[');
        _puts(file);
        _putc(':');
        _put_uint((uint16_t)line);
        _puts("] ");
    }
    _puts(_dbg_msg);
}

void debug_warning_fileline(const char* file, int line, const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsprintf(_dbg_msg, format, args);
    va_end(args);

    _timestamp();
    _puts(_color_enabled ? WARNING_TYPE : WARNING_TYPE_PLAIN);
    if (_filename_line_enabled) {
        _putc('[');
        _puts(file);
        _putc(':');
        _put_uint((uint16_t)line);
        _puts("] ");
    }
    _puts(_dbg_msg);
}

void debug_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsprintf(_dbg_msg, format, args);
    va_end(args);

    _send_type(_color_enabled ? ERROR_TYPE : ERROR_TYPE_PLAIN, _dbg_msg);
}

void debug_warning(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsprintf(_dbg_msg, format, args);
    va_end(args);

    _send_type(_color_enabled ? WARNING_TYPE : WARNING_TYPE_PLAIN, _dbg_msg);
}

void debug_ok(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsprintf(_dbg_msg, format, args);
    va_end(args);

    _send_type(_color_enabled ? OK_TYPE : OK_TYPE_PLAIN, _dbg_msg);
}

void debug_success(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsprintf(_dbg_msg, format, args);
    va_end(args);

    _send_type(_color_enabled ? SUCCESS_TYPE : SUCCESS_TYPE_PLAIN, _dbg_msg);
}

void debug_info(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsprintf(_dbg_msg, format, args);
    va_end(args);

    _send_type(_color_enabled ? INFO_TYPE : INFO_TYPE_PLAIN, _dbg_msg);
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



// runtime helpers for 24-bit ANSI colours; return a pointer to a static buffer
const char* customTextColor(uint8_t r, uint8_t g, uint8_t b) {
    sprintf(_ansi, "\033[38;2;%u;%u;%um", (unsigned)r, (unsigned)g, (unsigned)b);
    return _ansi;
}

const char* customBgColor(uint8_t r, uint8_t g, uint8_t b) {
    sprintf(_ansi, "\033[48;2;%u;%u;%um", (unsigned)r, (unsigned)g, (unsigned)b);
    return _ansi;
}
