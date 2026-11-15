/*******************************************************************************
 * @file    ElegantDebug.c
 * @version 1.6
 * @brief   C implementation for ANSI-colored debug logging — Specially used for 8051 (STC89C52RC).
 *
 * Output is formatted with the Keil C51 `vprintf()` and sent one byte at a
 * time through `putchar()`. The skeleton's `char msg[DEBUG_BUFFER_LEN]` and
 * `char out[DEBUG_BUFFER_LEN * 2]` buffers are deliberately not used: they
 * would need 512 bytes, and an STC89C52RC has 256 bytes of RAM in total.
 *
 * C51 also reserves an 18..26 byte static parameter block (?BYTE) for every
 * variadic function. In the SMALL model those blocks live in DATA and are
 * allocated even for functions the application never calls. _debug_keep_all()
 * below keeps every entry point in the BL51 call tree so the blocks overlay;
 * without it a library of this size fails to link with
 * `L107: ADDRESS SPACE OVERFLOW`.
 *
 * Make sure to call `debug_init()` and to initialize the UART before using
 * other functions in this file.
 *
 * @author:    Fan Yaoyang
 * @date:      2026-11-15
 *
 * @changelog:
 * - (See header file)
 ******************************************************************************/

#include "ElegantDebug.h"



/* ------------------------------------------------------------------------- */
/* TX path                                                                    */
/* ------------------------------------------------------------------------- */

/* Default 8051 transmitter: poll TI after writing SBUF.
 *
 * NOTE: this loop waits for TI and then clears it. If the project's UART ISR
 * also clears TI, the two will fight and the polling loop can hang. In that
 * case define DEBUG_EXTERNAL_PUTCHAR and provide your own `char putchar(char c)`;
 * every byte of this library goes through putchar(), including the formatted
 * output produced by vprintf(). */
#ifndef DEBUG_EXTERNAL_PUTCHAR

#include <REG52.H>

char putchar(char c)
{
    SBUF = c;
    while (!TI);
    TI = 0;
    return c;
}

#endif



volatile uint32_t data _debug_tick_ms = 0;

static bool _timestamp_enabled = true;
static bool _color_enabled = true;
static bool _filename_line_enabled = false;



/* Keep every variadic entry point in the BL51 call tree.
 *
 * BL51 only overlays parameter blocks of functions that are part of the call
 * tree; a function the application never calls keeps its own block and can
 * fill the 128-byte DATA space on its own (L107). The reference below is
 * never executed (the flag stays false), but it makes all entry points
 * reachable, so their blocks overlay and an unused function no longer costs
 * DATA. Remove it together with the entry points it references. */
static volatile bool _debug_link_all = false;

static void _debug_keep_all(void) {
    debug_log("");
    debug_logWithType("", "", "");
    debug_error_fileline("", 0, "");
    debug_warning_fileline("", 0, "");
    debug_error("");
    debug_warning("");
    debug_ok("");
    debug_success("");
    debug_info("");
}

void debug_init(bool enable_timestamp, bool enable_color, bool enable_filename_line) {

    _timestamp_enabled = enable_timestamp;
    _color_enabled = enable_color;
    _filename_line_enabled = enable_filename_line;

    if (_debug_link_all) {
        _debug_keep_all();
    }
}

/* Read the 32-bit tick without tearing it. The timer ISR increments
 * _debug_tick_ms at an arbitrary point, so a single 4-byte read can mix the
 * old and the new value. Reading twice and comparing guarantees the returned
 * value was not modified between the two reads. */
static uint32_t _getTick(void) {
    uint32_t a;
    uint32_t b;

    do {
        a = _debug_tick_ms;
        b = _debug_tick_ms;
    } while (a != b);

    return a;
}

/* Decimal output without printf(): every printf() call site costs a parameter
 * block in the 128-byte DATA area, and on 8051 that runs out. */
static void _sendUInt(unsigned int v) {
    char buf[6];
    char n = 0;

    if (v == 0U) {
        putchar('0');
        return;
    }

    while (v != 0U) {
        buf[n] = (char)('0' + (v % 10U));
        n++;
        v = v / 10U;
    }

    while (n != 0) {
        n--;
        putchar(buf[n]);
    }
}

/* Two / three digits, zero padded ("00" .. "99", "000" .. "999"). */
static void _sendUInt2(unsigned int v) {
    putchar((char)('0' + ((v / 10U) % 10U)));
    putchar((char)('0' + (v % 10U)));
}

static void _sendUInt3(unsigned int v) {
    putchar((char)('0' + ((v / 100U) % 10U)));
    putchar((char)('0' + ((v / 10U) % 10U)));
    putchar((char)('0' + (v % 10U)));
}

/* Send the "[hh:mm:ss.mmm] " prefix (if enabled).
 * 16-bit arithmetic on purpose: formatting a 32-bit value with printf is
 * much bigger, and 18 hours of uptime still fit in 16 bits of seconds. */
static void _sendTimestamp(void) {
    uint32_t ms;
    unsigned int s;
    unsigned int hours;

    if (!_timestamp_enabled) {
        return;
    }

    ms = _getTick();
    s = (unsigned int)(ms / 1000UL);
    hours = s / 3600U;

    putchar('[');
    if (hours < 100U) {
        _sendUInt2(hours);
    } else {
        _sendUInt(hours);
    }
    putchar(':');
    _sendUInt2((s % 3600U) / 60U);
    putchar(':');
    _sendUInt2(s % 60U);
    putchar('.');
    _sendUInt3((unsigned int)(ms % 1000UL));
    putchar(']');
    putchar(' ');
}

/* Transmit a plain string, one byte at a time. The timestamp is emitted by
 * each entry point before the type prefix, so the field order is always
 * timestamp, prefix, message. */
static void _send(const char* text) {

    if (text == NULL) {
        return;
    }

    while (*text != '\0') {
        putchar(*text);
        text++;
    }
}



void debug_log(const char* format, ...) {
    va_list args;

    if (format == NULL) {
        return;
    }

    _sendTimestamp();

    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void debug_logWithType(const char* type, const char* style, const char* format, ...) {
    va_list args;

    if (format == NULL) {
        return;
    }

    _sendTimestamp();
    _send(BOLD);
    _send(style);
    putchar('[');
    _send(type);
    putchar(']');
    _send(CLR);
    putchar(' ');

    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

// void debug_logWithType_fileline(const char* file, int line, const char* type, const char* format, ...) {
//     ... same as main branch; see debug_error_fileline for the 8051 version
// }

void debug_error_fileline(const char* file, int line, const char* format, ...) {
    va_list args;

    if (format == NULL) {
        return;
    }

    _sendTimestamp();
    _send(_color_enabled ? ERROR_TYPE : ERROR_TYPE_PLAIN);
    if (_filename_line_enabled) {
        putchar('[');
        _send(file);
        putchar(':');
        _sendUInt((unsigned int)line);
        _send("] ");
    }

    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void debug_warning_fileline(const char* file, int line, const char* format, ...) {
    va_list args;

    if (format == NULL) {
        return;
    }

    _sendTimestamp();
    _send(_color_enabled ? WARNING_TYPE : WARNING_TYPE_PLAIN);
    if (_filename_line_enabled) {
        putchar('[');
        _send(file);
        putchar(':');
        _sendUInt((unsigned int)line);
        _send("] ");
    }

    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void debug_error(const char* format, ...) {
    va_list args;

    if (format == NULL) {
        return;
    }

    _sendTimestamp();
    _send(_color_enabled ? ERROR_TYPE : ERROR_TYPE_PLAIN);

    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void debug_warning(const char* format, ...) {
    va_list args;

    if (format == NULL) {
        return;
    }

    _sendTimestamp();
    _send(_color_enabled ? WARNING_TYPE : WARNING_TYPE_PLAIN);

    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void debug_ok(const char* format, ...) {
    va_list args;

    if (format == NULL) {
        return;
    }

    _sendTimestamp();
    _send(_color_enabled ? OK_TYPE : OK_TYPE_PLAIN);

    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void debug_success(const char* format, ...) {
    va_list args;

    if (format == NULL) {
        return;
    }

    _sendTimestamp();
    _send(_color_enabled ? SUCCESS_TYPE : SUCCESS_TYPE_PLAIN);

    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void debug_info(const char* format, ...) {
    va_list args;

    if (format == NULL) {
        return;
    }

    _sendTimestamp();
    _send(_color_enabled ? INFO_TYPE : INFO_TYPE_PLAIN);

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



// runtime helpers for 24‑bit ANSI colours; return a pointer to a static buffer
/* The static buffer lives in xdata (not data) so the library does not eat
 * into the 128-byte direct RAM area that most 8051 projects already fill. */
const char* customTextColor(uint8_t r, uint8_t g, uint8_t b) {
    static char xdata ansi[24];
    sprintf(ansi, "\033[38;2;%u;%u;%um",
            (unsigned int)r, (unsigned int)g, (unsigned int)b);
    return ansi;
}

const char* customBgColor(uint8_t r, uint8_t g, uint8_t b) {
    static char xdata ansi[24];
    sprintf(ansi, "\033[48;2;%u;%u;%um",
            (unsigned int)r, (unsigned int)g, (unsigned int)b);
    return ansi;
}
