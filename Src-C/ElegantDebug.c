/*******************************************************************************
 * @file    ElegantDebug.c
 * @version 1.6
 * @brief   C implementation for ANSI-colored debug logging — Specially used for 8051 (STC89C52RC).
 *
 * Implements streaming output and a lightweight formatter for 8051.
 *
 * Make sure to call `debug_init()` with a valid UART instance before using
 * other functions in this file.
 *
 * @author:    Jian0635
 * @date:      2026-11-15
 *
 * @changelog:
 * - (See header file)
 ******************************************************************************/

#include "ElegantDebug.h"



volatile uint32_t _debug_tick_ms = 0;

static bool _timestamp_enabled = true;
static bool _color_enabled = true;
static bool _filename_line_enabled = false;

#ifdef __C51__
// [C51 Adaptation] Hand-written streaming parser and shared XDATA buffers to completely avoid memory overflow.
static char DBG_BUF_MEM g_msg[DEBUG_BUFFER_LEN];
static char DBG_BUF_MEM g_ansi[20];

typedef struct { char *buf; size_t size; size_t len; } _dbg_out_t;

static void _dbg_out_ch(_dbg_out_t *out, char c) {
    if (out->size > 0 && out->len < (out->size - 1)) out->buf[out->len] = c;
    out->len++;
}
static void _dbg_out_repeat(_dbg_out_t *out, char c, int n) {
    while (n > 0) { _dbg_out_ch(out, c); n--; }
}
static void _dbg_out_uint(_dbg_out_t *out, unsigned long value, unsigned char base,
                          bool upper, int width, int prec, bool left, bool zero, char sign) {
    char tmp[12]; int digits = 0, total, pad;
    if (!(prec == 0 && value == 0)) {
        do {
            unsigned char d = (unsigned char)(value % (unsigned long)base);
            if (d < 10) tmp[digits++] = (char)('0' + d);
            else tmp[digits++] = (char)((upper ? 'A' : 'a') + (d - 10));
            value /= (unsigned long)base;
        } while (value != 0);
    }
    total = (digits > prec) ? digits : prec;
    if (sign != '\0') total++;
    pad = width - total; if (pad < 0) pad = 0;
    if (!left && !(zero && prec < 0)) _dbg_out_repeat(out, ' ', pad);
    if (sign != '\0') _dbg_out_ch(out, sign);
    if (!left && zero && prec < 0) _dbg_out_repeat(out, '0', pad);
    _dbg_out_repeat(out, '0', total - digits - ((sign != '\0') ? 1 : 0));
    while (digits > 0) _dbg_out_ch(out, tmp[--digits]);
    if (left) _dbg_out_repeat(out, ' ', pad);
}
static void _dbg_out_strn(_dbg_out_t *out, const char *s, int width, int prec, bool left) {
    int len = 0, pad, i;
    if (s == NULL) s = "(null)";
    while (s[len] != '\0' && (prec < 0 || len < prec)) len++;
    pad = width - len; if (pad < 0) pad = 0;
    if (!left) _dbg_out_repeat(out, ' ', pad);
    for (i = 0; i < len; i++) _dbg_out_ch(out, s[i]);
    if (left) _dbg_out_repeat(out, ' ', pad);
}
int debug_vsnprintf(char *buf, size_t size, const char *format, va_list args) {
    _dbg_out_t out; char c;
    out.buf = buf; out.size = size; out.len = 0;
    while (format != NULL && *format != '\0') {
        c = *format++;
        if (c != '%') { _dbg_out_ch(&out, c); continue; }
        if (*format == '%') { _dbg_out_ch(&out, '%'); format++; continue; }
        {
            bool left = false, zero = false, plus = false, space = false, lng = false;
            int  width = 0, prec = -1;
            for (;;) {
                c = *format;
                if (c == '-') left = true;
                else if (c == '0') zero = true;
                else if (c == '+') plus = true;
                else if (c == ' ') space = true;
                else if (c == '#') {}
                else break;
                format++;
            }
            if (*format == '*') { width = va_arg(args, int); format++; if (width < 0) { left = true; width = -width; } }
            else { while (*format >= '0' && *format <= '9') { width = width * 10 + (*format - '0'); format++; } }
            if (*format == '.') {
                format++; prec = 0;
                if (*format == '*') { prec = va_arg(args, int); format++; }
                else { while (*format >= '0' && *format <= '9') { prec = prec * 10 + (*format - '0'); format++; } }
            }
            if (*format == 'l') { lng = true; format++; } else if (*format == 'h') format++;
            c = *format; if (c == '\0') break; format++;
            switch (c) {
            case 'd': case 'i': {
                long v = lng ? va_arg(args, long) : (long)va_arg(args, int);
                char sign = '\0'; unsigned long mag;
                if (v < 0) { sign = '-'; mag = (unsigned long)(-(v + 1L)) + 1UL; }
                else { mag = (unsigned long)v; if (plus) sign = '+'; else if (space) sign = ' '; }
                _dbg_out_uint(&out, mag, 10, false, width, prec, left, zero, sign); break;
            }
            case 'u': case 'o': case 'x': case 'X': {
                unsigned long mag = lng ? va_arg(args, unsigned long) : (unsigned long)va_arg(args, unsigned int);
                unsigned char base = (unsigned char)((c == 'o') ? 8 : ((c == 'u') ? 10 : 16));
                _dbg_out_uint(&out, mag, base, (bool)(c == 'X'), width, prec, left, zero, '\0'); break;
            }
            case 'c': {
                char ch = (char)va_arg(args, int); int pad = width - 1; if (pad < 0) pad = 0;
                if (!left) _dbg_out_repeat(&out, ' ', pad);
                _dbg_out_ch(&out, ch);
                if (left) _dbg_out_repeat(&out, ' ', pad); break;
            }
            case 's': { const char *s = va_arg(args, const char *); _dbg_out_strn(&out, s, width, prec, left); break; }
            default: _dbg_out_ch(&out, '%'); _dbg_out_ch(&out, c); break;
            }
        }
    }
    if (out.size > 0) { size_t end = (out.len < out.size) ? out.len : (out.size - 1); out.buf[end] = '\0'; }
    return (int)out.len;
}
int debug_snprintf(char *buf, size_t size, const char *format, ...) {
    va_list args; int n; va_start(args, format); n = debug_vsnprintf(buf, size, format, args); va_end(args); return n;
}
#define snprintf debug_snprintf
#define vsnprintf debug_vsnprintf

static void _putc(char c) { SBUF = c; while (!TI); TI = 0; }
static void _puts(const char* s) { while (*s) { _putc(*s); s++; } }
static void _put_uint(unsigned int v) {
    char buf[6]; char n = 0;
    if (v == 0) { _putc('0'); return; }
    while (v) { buf[n++] = (char)('0' + (v % 10U)); v /= 10U; }
    while (n) _putc(buf[--n]);
}
static void _sendTimestamp(void) {
    uint32_t ms = _debug_tick_ms;
    uint32_t s = ms / 1000U;
    _putc('[');
    _putc((char)('0' + (s / 36000U) % 10U)); _putc((char)('0' + (s / 3600U) % 10U)); _putc(':');
    _putc((char)('0' + (s / 600U) % 10U)); _putc((char)('0' + (s / 60U) % 10U)); _putc(':');
    _putc((char)('0' + ((s % 60U) / 10U))); _putc((char)('0' + (s % 60U) % 10U)); _putc('.');
    _putc((char)('0' + (ms % 1000U) / 100U)); _putc((char)('0' + ((ms % 1000U) / 10U) % 10U)); _putc((char)('0' + (ms % 1000U) % 10U));
    _putc(']'); _putc(' ');
}
#endif



// [C51 Adaptation] Complete 8051 UART initialization (11.0592MHz, 9600bps)
void debug_init(void) {
#ifdef __C51__
    TMOD &= 0x0F;       
    TMOD |= 0x20;       
    TH1 = 0xFD;         
    TL1 = 0xFD;
    TR1 = 1;            
    SCON = 0x50;        
    ES = 0;             
#endif
}

#ifndef __C51__
static uint32_t _getTick(void) {
    return _debug_tick_ms;
}
#endif



static void _send(const char* text) {

#ifdef __C51__
    // [C51 Adaptation] Streaming output
    if (_timestamp_enabled) _sendTimestamp();
    _puts(text);
#else
    // Original author's code
    char out[DEBUG_BUFFER_LEN * 2];
    size_t pos = 0;

    if (_timestamp_enabled) {
        uint32_t ms = _getTick();
        uint32_t s = ms / 1000U;
        uint32_t hours = s / 3600U;
        uint32_t minutes = (s % 3600U) / 60U;
        uint32_t seconds = s % 60U;
        pos = snprintf(out, sizeof(out), "[%02lu:%02lu:%02lu.%03lu] ",
                       (unsigned long)hours, (unsigned long)minutes,
                       (unsigned long)seconds, (unsigned long)(ms % 1000U));
        if (pos >= sizeof(out)) pos = 0; // guard
    }

    /* append text safely */
    if (pos < sizeof(out)) {
        size_t remain = sizeof(out) - pos;
        /* leave room for null terminator */
        strncpy(out + pos, text, (remain > 0) ? (remain - 1) : 0);
        out[sizeof(out) - 1] = '\0';
    }

    // TODO HERE: Transmit data to SBUF (Send TX)
    // [C51 Adaptation] Transmit data via SBUF in polling mode
    {
        const char *p = out;
        while (*p != '\0') {
            SBUF = *p++;
            while (!TI); 
            TI = 0;      
        }
    }
#endif
}



void debug_log(const char* format, ...) {
#ifdef __C51__
    va_list args; va_start(args, format); vsnprintf(g_msg, sizeof(g_msg), format, args); va_end(args);
    _send(g_msg);
#else
    // Original author's code
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);
    _send(msg);
#endif
}

void debug_logWithType(const char* type, const char* style, const char* format, ...) {
#ifdef __C51__
    va_list args; va_start(args, format); vsnprintf(g_msg, sizeof(g_msg), format, args); va_end(args);
    if (_timestamp_enabled) _sendTimestamp();
    _puts(BOLD); _puts(style); _putc('['); _puts(type); _putc(']'); _puts(CLR); _putc(' ');
    _puts(g_msg);
#else
    // Original author's code
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

    char combined[DEBUG_BUFFER_LEN + 32];
    snprintf(combined, sizeof(combined), "\033[1m%s[%s]\033[0m %s", style, type, msg);
    _send(combined);
#endif
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

void debug_error_fileline(const char* file, int line, const char* format, ...) {
#ifdef __C51__
    va_list args; va_start(args, format); vsnprintf(g_msg, sizeof(g_msg), format, args); va_end(args);
    if (_timestamp_enabled) _sendTimestamp();
    _puts(_color_enabled ? ERROR_TYPE : ERROR_TYPE_PLAIN);
    if (_filename_line_enabled) {
        _putc('['); _puts(file); _putc(':'); _put_uint((unsigned int)line); _puts("] ");
    }
    _puts(g_msg);
#else
    // Original author's code
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

    const char* prefix = _color_enabled ? ERROR_TYPE : ERROR_TYPE_PLAIN;
    char combined[DEBUG_BUFFER_LEN + 96];
    if (_filename_line_enabled) {
        snprintf(combined, sizeof(combined), "%s[%s:%d] %s", prefix, file, line, msg);
    } else {
        snprintf(combined, sizeof(combined), "%s%s", prefix, msg);
    }
    _send(combined);
#endif
}

void debug_warning_fileline(const char* file, int line, const char* format, ...) {
#ifdef __C51__
    va_list args; va_start(args, format); vsnprintf(g_msg, sizeof(g_msg), format, args); va_end(args);
    if (_timestamp_enabled) _sendTimestamp();
    _puts(_color_enabled ? WARNING_TYPE : WARNING_TYPE_PLAIN);
    if (_filename_line_enabled) {
        _putc('['); _puts(file); _putc(':'); _put_uint((unsigned int)line); _puts("] ");
    }
    _puts(g_msg);
#else
    // Original author's code
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

    const char* prefix = _color_enabled ? WARNING_TYPE : WARNING_TYPE_PLAIN;
    char combined[DEBUG_BUFFER_LEN + 96];
    if (_filename_line_enabled) {
        snprintf(combined, sizeof(combined), "%s[%s:%d] %s", prefix, file, line, msg);
    } else {
        snprintf(combined, sizeof(combined), "%s%s", prefix, msg);
    }
    _send(combined);
#endif
}

void debug_ok(const char* format, ...) {
#ifdef __C51__
    va_list args; va_start(args, format); vsnprintf(g_msg, sizeof(g_msg), format, args); va_end(args);
    if (_timestamp_enabled) _sendTimestamp();
    _puts(_color_enabled ? OK_TYPE : OK_TYPE_PLAIN);
    _puts(g_msg);
#else
    // Original author's code
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

    const char* prefix = _color_enabled ? OK_TYPE : OK_TYPE_PLAIN;
    char combined[DEBUG_BUFFER_LEN + 32];
    snprintf(combined, sizeof(combined), "%s%s", prefix, msg);
    _send(combined);
#endif
}

void debug_success(const char* format, ...) {
#ifdef __C51__
    va_list args; va_start(args, format); vsnprintf(g_msg, sizeof(g_msg), format, args); va_end(args);
    if (_timestamp_enabled) _sendTimestamp();
    _puts(_color_enabled ? SUCCESS_TYPE : SUCCESS_TYPE_PLAIN);
    _puts(g_msg);
#else
    // Original author's code
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

    const char* prefix = _color_enabled ? SUCCESS_TYPE : SUCCESS_TYPE_PLAIN;
    char combined[DEBUG_BUFFER_LEN + 32];
    snprintf(combined, sizeof(combined), "%s%s", prefix, msg);
    _send(combined);
#endif
}

void debug_info(const char* format, ...) {
#ifdef __C51__
    va_list args; va_start(args, format); vsnprintf(g_msg, sizeof(g_msg), format, args); va_end(args);
    if (_timestamp_enabled) _sendTimestamp();
    _puts(_color_enabled ? INFO_TYPE : INFO_TYPE_PLAIN);
    _puts(g_msg);
#else
    // Original author's code
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

    const char* prefix = _color_enabled ? INFO_TYPE : INFO_TYPE_PLAIN;
    char combined[DEBUG_BUFFER_LEN + 32];
    snprintf(combined, sizeof(combined), "%s%s", prefix, msg);
    _send(combined);
#endif
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
const char* customTextColor(uint8_t r, uint8_t g, uint8_t b) {
#ifdef __C51__
    snprintf(g_ansi, sizeof(g_ansi), "\033[38;2;%u;%u;%um", (unsigned)r, (unsigned)g, (unsigned)b);
    return g_ansi;
#else
    // Original author's code
    static char ansi[24];
    snprintf(ansi, sizeof(ansi), "\033[38;2;%u;%u;%um", (unsigned)r, (unsigned)g, (unsigned)b);
    return ansi;
#endif
}

const char* customBgColor(uint8_t r, uint8_t g, uint8_t b) {
#ifdef __C51__
    snprintf(g_ansi, sizeof(g_ansi), "\033[48;2;%u;%u;%um", (unsigned)r, (unsigned)g, (unsigned)b);
    return g_ansi;
#else
    // Original author's code
    static char ansi[24];
    snprintf(ansi, sizeof(ansi), "\033[48;2;%u;%u;%um", (unsigned)r, (unsigned)g, (unsigned)b);
    return ansi;
#endif
}