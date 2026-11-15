/*******************************************************************************
 * @file    ElegantDebug.c
 * @version 1.7
 * @brief   C implementation for ANSI-colored debug logging â€?STM32 HAL,
 *          Renesas RA FSP, TI MSPM0 DL, 8051 (MCS-51).
 *
 * Implements the C API declared in `Src-C/ElegantDebug.h`. Provides formatted
 * logging functions that send output over a HAL UART interface (STM32),
 * SCI UART (Renesas RA), DL UART (TI MSPM0), or the built-in UART of the
 * 8051 through the user-provided putchar(); USB-CDC is also supported
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



#if DEBUG_PLATFORM_8051
/* 8051: keep static data in idata (high 128 bytes) â€?the 128-byte DATA
   segment is too small once the user program is linked in. */
volatile uint32_t DEBUG_IDATA _debug_tick_ms = 0;
#elif (DEBUG_PLATFORM_RA || DEBUG_PLATFORM_TI)
volatile uint32_t _debug_tick_ms = 0;
#endif



#if DEBUG_PLATFORM_STM32
static UART_HandleTypeDef *_huart = NULL;
#elif DEBUG_PLATFORM_RA
static uart_instance_t const *_uart = NULL;
#elif DEBUG_PLATFORM_TI
static UART_Regs *_uart_inst = NULL;
#endif

#if DEBUG_PLATFORM_8051
static bool DEBUG_IDATA _timestamp_enabled = true;
static bool DEBUG_IDATA _color_enabled = true;
static bool DEBUG_IDATA _filename_line_enabled = false;
#else
static bool _timestamp_enabled = true;
static bool _color_enabled = true;
static bool _filename_line_enabled = false;
#endif



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
/* ---- 8051 helpers -------------------------------------------------------
 * 8051 devices have only a few hundred bytes of RAM, so nothing is buffered:
 * every log function formats straight to putchar()/vprintf().
 * Requires: user-provided putchar() and an already-initialized UART.
 *
 * IMPORTANT (Keil C51): the C51 compiler overlays the argument area of
 * variadic functions with other function calls. Calling printf()/sprintf()
 * between va_start() and vprintf() ¡ª or even before va_start() in the same
 * function ¡ª corrupts the vprintf argument list (observed as shifted values,
 * e.g. 42 printed as 10752). Therefore these helpers avoid printf entirely:
 * timestamps and file:line prefixes are emitted with putchar() and manual
 * decimal conversion. The ONLY variadic call in each log function is the
 * final vprintf(), which runs immediately after va_start().
 * ------------------------------------------------------------------------ */

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
#endif



static void _send(const char* text) {

#if DEBUG_PLATFORM_8051
    /* 8051: unbuffered output (tiny RAM, no snprintf on C51) */
    if (text == NULL) return;
    _send_timestamp();
    _putstr(text);
#else

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

#endif /* !DEBUG_PLATFORM_8051 */
}



void debug_log(const char* format, ...) {
#if DEBUG_PLATFORM_8051
    /* 8051: no local buffer (tiny RAM) â€?format straight to UART */
    va_list args;
    _send_timestamp();
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
#else
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);
    _send(msg);
#endif
}

void debug_logWithType(const char* type, const char* style, const char* format, ...) {
#if DEBUG_PLATFORM_8051
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
#else
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
#if DEBUG_PLATFORM_8051
    va_list args;
    _send_timestamp();
    _putstr(_color_enabled ? ERROR_TYPE : ERROR_TYPE_PLAIN);
    _send_fileline(file, line);
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
#else
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
#if DEBUG_PLATFORM_8051
    va_list args;
    _send_timestamp();
    _putstr(_color_enabled ? WARNING_TYPE : WARNING_TYPE_PLAIN);
    _send_fileline(file, line);
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
#else
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
#if DEBUG_PLATFORM_8051
    va_list args;
    _send_timestamp();
    _putstr(_color_enabled ? OK_TYPE : OK_TYPE_PLAIN);
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
#else
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
#if DEBUG_PLATFORM_8051
    va_list args;
    _send_timestamp();
    _putstr(_color_enabled ? SUCCESS_TYPE : SUCCESS_TYPE_PLAIN);
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
#else
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
#if DEBUG_PLATFORM_8051
    va_list args;
    _send_timestamp();
    _putstr(_color_enabled ? INFO_TYPE : INFO_TYPE_PLAIN);
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
#else
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



// runtime helpers for 24â€‘bit ANSI colours; return a pointer to a static buffer
const char* customTextColor(uint8_t r, uint8_t g, uint8_t b) {
#if DEBUG_PLATFORM_8051
    static char DEBUG_IDATA ansi[24];   /* idata: DATA segment is only 128 bytes */
#else
    static char ansi[24];
#endif
#if DEBUG_PLATFORM_8051
    /* 8051 has no snprintf; the target buffer (24 bytes) always fits */
    sprintf(ansi, "\033[38;2;%u;%u;%um", (unsigned)r, (unsigned)g, (unsigned)b);
#else
    snprintf(ansi, sizeof(ansi), "\033[38;2;%u;%u;%um", (unsigned)r, (unsigned)g, (unsigned)b);
#endif
    return ansi;
}

const char* customBgColor(uint8_t r, uint8_t g, uint8_t b) {
#if DEBUG_PLATFORM_8051
    static char DEBUG_IDATA ansi[24];
#else
    static char ansi[24];
#endif
#if DEBUG_PLATFORM_8051
    sprintf(ansi, "\033[48;2;%u;%u;%um", (unsigned)r, (unsigned)g, (unsigned)b);
#else
    snprintf(ansi, sizeof(ansi), "\033[48;2;%u;%u;%um", (unsigned)r, (unsigned)g, (unsigned)b);
#endif
    return ansi;
}

