/*******************************************************************************
 * @file    ElegantDebug.c
 * @version 1.6
 * @brief   C implementation for ANSI-colored debug logging — STM32 HAL, Renesas RA FSP, TI MSPM0 DL.
 *
 * Implements the C API declared in `Src-C/ElegantDebug.h`. Provides formatted
 * logging functions that send output over a HAL UART interface (STM32),
 * SCI UART (Renesas RA), or DL UART (TI MSPM0); USB-CDC is also supported
 * on STM32 when `USB_AS_DEBUG_PORT` is enabled. Supports optional timestamps and
 * ANSI color prefixes.
 *
 * Make sure to call `debug_init()` with a valid UART instance before using
 * other functions in this file.
 *
 * @author:    WilliTourt <willitourt@foxmail.com>
 * @date:      2026-08-09
 *
 * @changelog:
 * - (See header file)
 ******************************************************************************/

#include "ElegantDebug.h"




#if (DEBUG_PLATFORM_RA || DEBUG_PLATFORM_TI || DEBUG_PLATFORM_8051)
volatile uint32_t _debug_tick_ms = 0;
#endif



#if DEBUG_PLATFORM_STM32
static UART_HandleTypeDef *_huart = NULL;
#elif DEBUG_PLATFORM_RA
static uart_instance_t const *_uart = NULL;
#elif DEBUG_PLATFORM_TI
static UART_Regs *_uart_inst = NULL;
#elif DEBUG_PLATFORM_8051
// 8051 has no peripheral handle: the debug UART is the on-chip SBUF.  This
// flag only records that debug_init() ran, so logging before the UART is
// configured stays silent instead of emitting garbage.
static bool _uart_ready = false;
#endif

static bool _timestamp_enabled = true;
static bool _color_enabled = true;
static bool _filename_line_enabled = false;



#if DEBUG_PLATFORM_STM32
void debug_init(UART_HandleTypeDef *huart, bool enable_timestamp, bool enable_color, bool enable_filename_line) {
    _huart = huart;
    _timestamp_enabled = enable_timestamp;
    _color_enabled = enable_color;
    _filename_line_enabled = enable_filename_line;
}
#elif DEBUG_PLATFORM_RA
void debug_init(uart_instance_t const *uart, bool enable_timestamp, bool enable_color, bool enable_filename_line) {
    _uart = uart;
    _timestamp_enabled = enable_timestamp;
    _color_enabled = enable_color;
    _filename_line_enabled = enable_filename_line;
}
#elif DEBUG_PLATFORM_TI
void debug_init(UART_Regs *uart_inst, bool enable_timestamp, bool enable_color, bool enable_filename_line) {
    _uart_inst = uart_inst;
    _timestamp_enabled = enable_timestamp;
    _color_enabled = enable_color;
    _filename_line_enabled = enable_filename_line;
}
#elif DEBUG_PLATFORM_8051
void debug_init(bool enable_timestamp, bool enable_color, bool enable_filename_line) {
    _uart_ready = true;             // the UART itself is initialised by the application
    _timestamp_enabled = enable_timestamp;
    _color_enabled = enable_color;
    _filename_line_enabled = enable_filename_line;
}
#endif



static uint32_t _getTick(void) {
#if DEBUG_PLATFORM_STM32
    return HAL_GetTick();
#elif (DEBUG_PLATFORM_RA || DEBUG_PLATFORM_TI || DEBUG_PLATFORM_8051)
    return _debug_tick_ms;
#endif
}

#if DEBUG_PLATFORM_RA
static volatile bool _txDone = true;

void debug_onTxComplete(uart_callback_args_t *p_args) {
    if (p_args->event == UART_EVENT_TX_COMPLETE) {
        _txDone = true;
    }
}
#endif



#if DEBUG_PLATFORM_8051

/* ===================== 8051 implementation =============================
 * Keil C51 reserves a permanent static argument area in DATA space for
 * *every* variadic function, and an 8051/8052 has only 128 bytes of directly
 * addressed RAM.  A single variadic entry point together with the call frames
 * of the formatter already needs ~160 bytes, so the 8051 port does not expose
 * a printf-style API at all.  It provides:
 *
 *     debug_log_lit() / debug_info_lit() / debug_ok_lit() /
 *     debug_success_lit() / debug_error_lit() / debug_warning_lit()
 *         -- a fixed message with the usual type prefix;
 *     debug_log_u32(label, value)
 *         -- `label` followed by an unsigned decimal number, which covers the
 *            "counter / millisecond tick" logging case without formatting;
 * 24-bit colours are not available as helpers on 8051 (customTextColor() is
 * not built there: its two 24-byte static buffers plus the formatter do not
 * fit); put the escape sequence in the message instead.
 *
 * Nothing here uses varargs or the C library, so the whole port fits in the
 * 256 bytes of a 8052.
 * ==================================================================== */

static void _putText(const char* s) {
    while (*s != '\0') {
        ED8051_PUTC(*s);
        s++;
    }
}

static void _put2(unsigned int v) {
    /* two decimal digits, zero padded, wrapped at 100 (hours) */
    v = v % 100u;
    ED8051_PUTC((char)('0' + (char)(v / 10u)));
    ED8051_PUTC((char)('0' + (char)(v % 10u)));
}

static void _put3(unsigned int v) {
    /* three decimal digits, zero padded (milliseconds) */
    v = v % 1000u;
    ED8051_PUTC((char)('0' + (char)(v / 100u)));
    ED8051_PUTC((char)('0' + (char)((v / 10u) % 10u)));
    ED8051_PUTC((char)('0' + (char)(v % 10u)));
}

static void _putUInt(unsigned int v) {
    char          buf[6];
    unsigned char n = 0u;

    if (v == 0u) {
        _putText("0");
        return;
    }
    while (v > 0u && n < (unsigned char)sizeof(buf)) {
        buf[n] = (char)('0' + (v % 10u));
        v = v / 10u;
        n++;
    }
    while (n > 0u) {
        n--;
        ED8051_PUTC(buf[n]);
    }
}

static void _putULong(unsigned long v) {
    char          buf[11];
    unsigned char n = 0u;

    if (v == 0ul) {
        _putText("0");
        return;
    }
    while (v > 0ul && n < (unsigned char)sizeof(buf)) {
        buf[n] = (char)('0' + (char)(v % 10ul));
        v = v / 10ul;
        n++;
    }
    while (n > 0u) {
        n--;
        ED8051_PUTC(buf[n]);
    }
}

static void _putTimestamp(void) {
    /* "[hh:mm:ss.mmm] " built without printf (hours wrap at 100) */
    uint32_t ms = _getTick();
    uint32_t s  = ms / 1000U;

    _putText("[");
    _put2((unsigned int)(s / 3600U));
    _putText(":");
    _put2((unsigned int)((s % 3600U) / 60U));
    _putText(":");
    _put2((unsigned int)(s % 60U));
    _putText(".");
    _put3((unsigned int)(ms % 1000U));
    _putText("] ");
}

static void _logLine(const char* prefix, const char* file, int line, const char* text) {
    if (!_uart_ready || text == NULL) {
        return;
    }
    if (_timestamp_enabled) {
        _putTimestamp();
    }
    if (prefix != NULL) {
        _putText(prefix);
    }
    if (file != NULL && _filename_line_enabled) {
        _putText("[");
        _putText(file);
        _putText(":");
        _putUInt((unsigned int)line);
        _putText("] ");
    }
    _putText(text);
}

void debug_log_lit(const char* text) {
    _logLine(NULL, NULL, 0, text);
}

void debug_info_lit(const char* text) {
    _logLine(_color_enabled ? INFO_TYPE : INFO_TYPE_PLAIN, NULL, 0, text);
}

void debug_ok_lit(const char* text) {
    _logLine(_color_enabled ? OK_TYPE : OK_TYPE_PLAIN, NULL, 0, text);
}

void debug_success_lit(const char* text) {
    _logLine(_color_enabled ? SUCCESS_TYPE : SUCCESS_TYPE_PLAIN, NULL, 0, text);
}

void debug_error_lit(const char* file, int line, const char* text) {
    _logLine(_color_enabled ? ERROR_TYPE : ERROR_TYPE_PLAIN, file, line, text);
}

void debug_warning_lit(const char* file, int line, const char* text) {
    _logLine(_color_enabled ? WARNING_TYPE : WARNING_TYPE_PLAIN, file, line, text);
}

// One complete line: optional timestamp + label + decimal value + CRLF.
// (Convenience for "counter / millisecond tick" logging without any
// formatting; the *_lit() functions do not add a line break, like on the
// other platforms.)
void debug_log_u32(const char* label, unsigned long value) {
    if (!_uart_ready) {
        return;
    }
    if (_timestamp_enabled) {
        _putTimestamp();
    }
    if (label != NULL) {
        _putText(label);
    }
    _putULong(value);
    _putText("\r\n");
}

#else  /* !DEBUG_PLATFORM_8051 : generic implementation */

static void _send(const char* text) {

    #if DEBUG_PLATFORM_STM32
        #if (USB_AS_DEBUG_PORT == 1)
            if (text == NULL) return;
        #else
            if (_huart == NULL || text == NULL) return;
        #endif
    #elif DEBUG_PLATFORM_RA
        if (_uart == NULL || text == NULL) return;
    #elif DEBUG_PLATFORM_TI
        if (_uart_inst == NULL || text == NULL) return;
    #endif

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

    #if DEBUG_PLATFORM_STM32
        #if (USB_AS_DEBUG_PORT == 1)
        CDC_Transmit_FS((uint8_t*)out, (uint16_t)strlen(out));
        #else
        HAL_UART_Transmit(_huart, (uint8_t*)out, (uint16_t)strlen(out), HAL_MAX_DELAY);
        #endif
    #elif DEBUG_PLATFORM_RA
        while (!_txDone) {
            R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
        }
        _txDone = false;

        #ifdef R_SCI_UART_H
        R_SCI_UART_Write(((uart_instance_t*)_uart)->p_ctrl,
                         (uint8_t*)out,
                         (uint32_t)strlen(out));
        #else
        R_SCI_B_UART_Write(((uart_instance_t*)_uart)->p_ctrl,
                           (uint8_t*)out,
                           (uint32_t)strlen(out));
        #endif
    #elif DEBUG_PLATFORM_TI
        for (size_t i = 0; out[i] != '\0'; i++) {
            DL_UART_transmitDataBlocking(_uart_inst, (uint8_t)out[i]);
        }
    #endif
}



void debug_log(const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);
    _send(msg);
}

void debug_logWithType(const char* type, const char* style, const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

    char combined[DEBUG_BUFFER_LEN + 32];
    snprintf(combined, sizeof(combined), "\033[1m%s[%s]\033[0m %s", style, type, msg);
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

void debug_error_fileline(const char* file, int line, const char* format, ...) {
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
}

void debug_warning_fileline(const char* file, int line, const char* format, ...) {
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
}

void debug_ok(const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

    const char* prefix = _color_enabled ? OK_TYPE : OK_TYPE_PLAIN;
    char combined[DEBUG_BUFFER_LEN + 32];
    snprintf(combined, sizeof(combined), "%s%s", prefix, msg);
    _send(combined);
}

void debug_success(const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

    const char* prefix = _color_enabled ? SUCCESS_TYPE : SUCCESS_TYPE_PLAIN;
    char combined[DEBUG_BUFFER_LEN + 32];
    snprintf(combined, sizeof(combined), "%s%s", prefix, msg);
    _send(combined);
}

void debug_info(const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

    const char* prefix = _color_enabled ? INFO_TYPE : INFO_TYPE_PLAIN;
    char combined[DEBUG_BUFFER_LEN + 32];
    snprintf(combined, sizeof(combined), "%s%s", prefix, msg);
    _send(combined);
}

#endif /* !DEBUG_PLATFORM_8051 */



void debug_setTimestampEnabled(bool enabled) {
    _timestamp_enabled = enabled;
}

void debug_setColorEnabled(bool enabled) {
    _color_enabled = enabled;
}

void debug_setFilenameLineEnabled(bool enabled) {
    _filename_line_enabled = enabled;
}



#if !DEBUG_PLATFORM_8051
// runtime helpers for 24‑bit ANSI colours; return a pointer to a static buffer
// (not built on 8051: the 48 bytes of static buffers plus the formatter are
//  more than a 256-byte part can spare — write the escape sequence into the
//  message instead, e.g. debug_log_lit("\033[38;2;255;128;0mX\033[0m"))
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
#endif /* !DEBUG_PLATFORM_8051 */
