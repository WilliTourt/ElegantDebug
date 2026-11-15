/*******************************************************************************
 * @file    ElegantDebug.c
 * @version 1.6
 * @brief   C implementation for ANSI-colored debug logging — Specially used for 8051 (STC89C52RC).
 *
 * Make sure to call `debug_init()` with a valid UART instance before using
 * other functions in this file.
 *
 * @changelog:
 * - (See header file)
 ******************************************************************************/

#include "ElegantDebug.h"

/* Debug/logging functions never nest, so all of them safely share these
 * static buffers instead of declaring large local arrays (which overflow
 * the 256-byte pdata page in the Compact memory model on 8051).
 *
 * They are placed with _at_ at 0x0100 and above, OUTSIDE the pdata page
 * (the first 256 bytes of XDATA): the printf library needs the pdata page
 * for its internal group, and if our buffers fill page 0 the linker
 * reports "L121 IMPROPER FIXUP". Verified with Keil C51 V9.54. */
#define DBG_BUF_BASE  0x0100

static char xdata _dbg_send_buf[DEBUG_BUFFER_LEN * 2] _at_ DBG_BUF_BASE;
static char xdata _dbg_msg_buf[DEBUG_BUFFER_LEN] _at_ (DBG_BUF_BASE + DEBUG_BUFFER_LEN * 2);
static char xdata _dbg_comb_buf[DEBUG_BUFFER_LEN + 96] _at_ (DBG_BUF_BASE + DEBUG_BUFFER_LEN * 3);

volatile uint32_t _debug_tick_ms = 0;

static bool _timestamp_enabled = true;
static bool _color_enabled = true;
static bool _filename_line_enabled = false;



/* Keil C51 has no (v)snprintf: bounded-signature bridges over sprintf.
 * The size argument is accepted (and ignored) to keep the same call
 * signatures as the upstream code. */
int vsnprintf(char *buf, unsigned int size, const char *fmt, va_list ap) {
    size = size;    /* Keil C51 idiom: silence "unreferenced parameter" */
    return vsprintf(buf, fmt, ap);
}

int snprintf(char *buf, unsigned int size, const char *fmt, ...) {
    int r;
    va_list ap;
    va_start(ap, fmt);
    r = vsprintf(buf, fmt, ap);
    va_end(ap);
    return r;
}

void debug_init(unsigned long baud, bool enable_timestamp, bool enable_color, bool enable_filename_line) {
    /* UART mode 1 (10-bit async), baud rate from Timer 1 mode 2.
     * Assumes 11.0592 MHz crystal: reload = 256 - 11059200/(32*12*baud)
     * = 256 - 28800/baud. For 9600 -> 0xFD (exact), 4800 -> 0xFA (exact). */
    SCON = 0x50;                       /* mode 1, REN = 1 */
    TMOD &= 0x0F;
    TMOD |= 0x20;                      /* Timer 1: mode 2, 8-bit auto-reload */
    TH1 = (unsigned char)(256UL - 28800UL / baud);
    TL1 = TH1;
    ET1 = 0;
    TR1 = 1;
    TI  = 0;
    RI  = 0;

    /* Timer 0: mode 1, ~1 ms interrupt at 11.0592 MHz (65536 - 922 = 0xFC66).
     * The user ISR must reload TH0/TL0 and call debug_tick(). */
    TMOD &= 0xF0;
    TMOD |= 0x01;                      /* Timer 0: mode 1, 16-bit */
    TH0 = 0xFC;
    TL0 = 0x66;
    ET0 = 1;
    TR0 = 1;
    EA  = 1;

    _timestamp_enabled = enable_timestamp;
    _color_enabled = enable_color;
    _filename_line_enabled = enable_filename_line;
}

static uint32_t _getTick(void) {
    return _debug_tick_ms;
}



static void _send(const char* text) {

    char *out = _dbg_send_buf;
    size_t pos = 0;

    if (_timestamp_enabled) {
        uint32_t ms = _getTick();
        uint32_t s = ms / 1000U;
        uint32_t hours = s / 3600U;
        uint32_t minutes = (s % 3600U) / 60U;
        uint32_t seconds = s % 60U;
        pos = snprintf(out, (DEBUG_BUFFER_LEN * 2), "[%02lu:%02lu:%02lu.%03lu] ",
                       (unsigned long)hours, (unsigned long)minutes,
                       (unsigned long)seconds, (unsigned long)(ms % 1000U));
        if (pos >= (DEBUG_BUFFER_LEN * 2)) pos = 0; // guard
    }

    /* append text safely */
    if (pos < (DEBUG_BUFFER_LEN * 2)) {
        size_t remain = (DEBUG_BUFFER_LEN * 2) - pos;
        /* leave room for null terminator */
        strncpy(out + pos, text, (remain > 0) ? (remain - 1) : 0);
        out[(DEBUG_BUFFER_LEN * 2) - 1] = '\0';
    }

    /* transmit data to SBUF (polling, TI flag marks byte completed) */
    for (pos = 0; out[pos] != '\0'; pos++) {
        SBUF = out[pos];
        while (!TI)
            ;
        TI = 0;
    }
}



void debug_log(const char* format, ...) {
    char *msg = _dbg_msg_buf;
    va_list args;
    va_start(args, format);
    vsnprintf(msg, DEBUG_BUFFER_LEN, format, args);
    va_end(args);
    _send(msg);
}

void debug_logWithType(const char* type, const char* style, const char* format, ...) {
    char *msg = _dbg_msg_buf;
    char *combined = _dbg_comb_buf;
    va_list args;
    va_start(args, format);
    vsnprintf(msg, DEBUG_BUFFER_LEN, format, args);
    va_end(args);

    snprintf(combined, (DEBUG_BUFFER_LEN + 32), "\033[1m%s[%s]\033[0m %s", style, type, msg);
    _send(combined);
}

// void debug_logWithType_fileline(const char* file, int line, const char* type, const char* format, ...) {
//     char msg[DEBUG_BUFFER_LEN];
//     va_list args;
//     va_start(args, format);
//     vsnprintf(msg, sizeof(msg), format, args);
//     va_end(args);
//
//     char combined[DEBUG_BUFFER_LEN + 96];
//     if (_filename_line_enabled) {
//         snprintf(combined, sizeof(combined), "\033[1m[%s]\033[0m [%s:%ld] %s", type, file, line, msg);
//     } else {
//         snprintf(combined, sizeof(combined), "\033[1m[%s]\033[0m %s", type, msg);
//     }
//     _send(combined);
// }

// NOTE: debug_error_fileline / debug_warning_fileline from the upstream
// skeleton are intentionally NOT implemented on 8051: without __VA_ARGS__
// they cannot be reached through macros, and their local frames overflow
// the 256-byte pdata page in Compact model. debug_error()/debug_warning()
// below provide the same logging without file:line.

void debug_ok(const char* format, ...) {
    char *msg = _dbg_msg_buf;
    char *combined = _dbg_comb_buf;
    const char* prefix;
    va_list args;
    va_start(args, format);
    vsnprintf(msg, DEBUG_BUFFER_LEN, format, args);
    va_end(args);

    prefix = _color_enabled ? OK_TYPE : OK_TYPE_PLAIN;
    snprintf(combined, (DEBUG_BUFFER_LEN + 32), "%s%s", prefix, msg);
    _send(combined);
}

void debug_success(const char* format, ...) {
    char *msg = _dbg_msg_buf;
    char *combined = _dbg_comb_buf;
    const char* prefix;
    va_list args;
    va_start(args, format);
    vsnprintf(msg, DEBUG_BUFFER_LEN, format, args);
    va_end(args);

    prefix = _color_enabled ? SUCCESS_TYPE : SUCCESS_TYPE_PLAIN;
    snprintf(combined, (DEBUG_BUFFER_LEN + 32), "%s%s", prefix, msg);
    _send(combined);
}

void debug_info(const char* format, ...) {
    char *msg = _dbg_msg_buf;
    char *combined = _dbg_comb_buf;
    const char* prefix;
    va_list args;
    va_start(args, format);
    vsnprintf(msg, DEBUG_BUFFER_LEN, format, args);
    va_end(args);

    prefix = _color_enabled ? INFO_TYPE : INFO_TYPE_PLAIN;
    snprintf(combined, (DEBUG_BUFFER_LEN + 32), "%s%s", prefix, msg);
    _send(combined);
}

/* Keil C51 has no __VA_ARGS__, so debug_error/debug_warning are plain
 * functions without automatic file:line (behavior difference vs. other
 * platforms; documented in the header). */
void debug_error(const char* format, ...) {
    char *msg = _dbg_msg_buf;
    char *combined = _dbg_comb_buf;
    const char* prefix;
    va_list args;
    va_start(args, format);
    vsnprintf(msg, DEBUG_BUFFER_LEN, format, args);
    va_end(args);

    prefix = _color_enabled ? ERROR_TYPE : ERROR_TYPE_PLAIN;
    snprintf(combined, (DEBUG_BUFFER_LEN + 96), "%s%s", prefix, msg);
    _send(combined);
}

void debug_warning(const char* format, ...) {
    char *msg = _dbg_msg_buf;
    char *combined = _dbg_comb_buf;
    const char* prefix;
    va_list args;
    va_start(args, format);
    vsnprintf(msg, DEBUG_BUFFER_LEN, format, args);
    va_end(args);

    prefix = _color_enabled ? WARNING_TYPE : WARNING_TYPE_PLAIN;
    snprintf(combined, (DEBUG_BUFFER_LEN + 96), "%s%s", prefix, msg);
    _send(combined);
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
    static char ansi[24];
    snprintf(ansi, sizeof(ansi), "\033[38;2;%u;%u;%um", (unsigned)r, (unsigned)g, (unsigned)b);
    return ansi;
}

const char* customBgColor(uint8_t r, uint8_t g, uint8_t b) {
    static char ansi[24];
    snprintf(ansi, sizeof(ansi), "\033[48;2;%u;%u;%um", (unsigned)r, (unsigned)g, (unsigned)b);
    return ansi;
}
