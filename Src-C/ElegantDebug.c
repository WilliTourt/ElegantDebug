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

#if DEBUG_PLATFORM_8051

/* ==================================================================
 * 8051 (Keil C51) implementation.
 *
 * Keil C51 allocates locals statically and its printf library alone
 * needs ~57 bytes of the 128 bytes of internal RAM, so the generic
 * implementation below cannot be used on this platform. This block
 * provides a self-contained version built around a single shared
 * message buffer and a minimal formatter.
 *
 * Supported conversions: %d %i %u %x %X %s %c %% with optional '0'
 * flag, field width and 'l' modifier. Floating point is not supported.
 * ================================================================== */

static bool _timestamp_enabled     = true;
static bool _color_enabled         = true;
static bool _filename_line_enabled = false;

volatile uint32_t _debug_tick_ms = 0;

static char xdata _dbg_msg[48];
static char xdata _ansi[24];

static void _c51_putc(char c)
{
    SBUF = c;
    while (!TI);
    TI = 0;
}

static void _c51_puts(const char *s)
{
    while (*s) _c51_putc(*s++);
}

static void _c51_put2(uint16_t v)
{
    _c51_putc((char)('0' + (v / 10U) % 10U));
    _c51_putc((char)('0' + v % 10U));
}

static void _c51_put_uint(uint16_t v)
{
    char t[5];
    uint8_t n = 0;

    if (v == 0) t[n++] = '0';
    while (v) {
        t[n++] = (char)('0' + (v % 10U));
        v = (uint16_t)(v / 10U);
    }
    while (n) _c51_putc(t[--n]);
}

static void _c51_timestamp(void)
{
    uint32_t ms;
    uint32_t s;
    uint32_t frac;

    if (!_timestamp_enabled) return;

    ms   = _debug_tick_ms;
    s    = ms / 1000UL;
    frac = ms % 1000UL;

    _c51_putc('[');
    _c51_put2((uint16_t)(s / 3600UL));
    _c51_putc(':');
    _c51_put2((uint16_t)((s / 60UL) % 60UL));
    _c51_putc(':');
    _c51_put2((uint16_t)(s % 60UL));
    _c51_putc('.');
    _c51_putc((char)('0' + (frac / 100UL)));
    _c51_putc((char)('0' + ((frac / 10UL) % 10UL)));
    _c51_putc((char)('0' + (frac % 10UL)));
    _c51_putc(']');
    _c51_putc(' ');
}

void debug_init(bool enable_timestamp, bool enable_color, bool enable_filename_line)
{
    _timestamp_enabled     = enable_timestamp;
    _color_enabled         = enable_color;
    _filename_line_enabled = enable_filename_line;
}

void debug_log(const char* format, ...)
{
    va_list ap;

    va_start(ap, format);
    vsprintf(_dbg_msg, format, ap);
    va_end(ap);

    _c51_timestamp();
    _c51_puts(_dbg_msg);
}

void debug_logWithType(const char* type, const char* style, const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    vsprintf(_dbg_msg, format, ap);
    va_end(ap);

    _c51_timestamp();
    _c51_puts(BOLD);
    _c51_puts(style);
    _c51_putc('[');
    _c51_puts(type);
    _c51_puts("]");
    _c51_puts(CLR);
    _c51_putc(' ');
    _c51_puts(_dbg_msg);
}

void debug_ok(const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    vsprintf(_dbg_msg, format, ap);
    va_end(ap);

    _c51_timestamp();
    _c51_puts(_color_enabled ? OK_TYPE : OK_TYPE_PLAIN);
    _c51_puts(_dbg_msg);
}

void debug_success(const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    vsprintf(_dbg_msg, format, ap);
    va_end(ap);

    _c51_timestamp();
    _c51_puts(_color_enabled ? SUCCESS_TYPE : SUCCESS_TYPE_PLAIN);
    _c51_puts(_dbg_msg);
}

void debug_info(const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    vsprintf(_dbg_msg, format, ap);
    va_end(ap);

    _c51_timestamp();
    _c51_puts(_color_enabled ? INFO_TYPE : INFO_TYPE_PLAIN);
    _c51_puts(_dbg_msg);
}

void debug_error(const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    vsprintf(_dbg_msg, format, ap);
    va_end(ap);

    _c51_timestamp();
    _c51_puts(_color_enabled ? ERROR_TYPE : ERROR_TYPE_PLAIN);
    _c51_puts(_dbg_msg);
}

void debug_warning(const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    vsprintf(_dbg_msg, format, ap);
    va_end(ap);

    _c51_timestamp();
    _c51_puts(_color_enabled ? WARNING_TYPE : WARNING_TYPE_PLAIN);
    _c51_puts(_dbg_msg);
}

void debug_error_fileline(const char* file, int line, const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    vsprintf(_dbg_msg, format, ap);
    va_end(ap);

    _c51_timestamp();
    _c51_puts(_color_enabled ? ERROR_TYPE : ERROR_TYPE_PLAIN);
    if (_filename_line_enabled) {
        _c51_putc('[');
        _c51_puts(file);
        _c51_putc(':');
        _c51_put_uint((uint16_t)line);
        _c51_puts("] ");
    }
    _c51_puts(_dbg_msg);
}

void debug_warning_fileline(const char* file, int line, const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    vsprintf(_dbg_msg, format, ap);
    va_end(ap);

    _c51_timestamp();
    _c51_puts(_color_enabled ? WARNING_TYPE : WARNING_TYPE_PLAIN);
    if (_filename_line_enabled) {
        _c51_putc('[');
        _c51_puts(file);
        _c51_putc(':');
        _c51_put_uint((uint16_t)line);
        _c51_puts("] ");
    }
    _c51_puts(_dbg_msg);
}

void debug_setTimestampEnabled(bool enabled)
{
    _timestamp_enabled = enabled;
}

void debug_setColorEnabled(bool enabled)
{
    _color_enabled = enabled;
}

void debug_setFilenameLineEnabled(bool enabled)
{
    _filename_line_enabled = enabled;
}

const char* customTextColor(uint8_t r, uint8_t g, uint8_t b)
{
    sprintf(_ansi, "\033[38;2;%u;%u;%um", (unsigned)r, (unsigned)g, (unsigned)b);
    return _ansi;
}

const char* customBgColor(uint8_t r, uint8_t g, uint8_t b)
{
    sprintf(_ansi, "\033[48;2;%u;%u;%um", (unsigned)r, (unsigned)g, (unsigned)b);
    return _ansi;
}
#else




#if (DEBUG_PLATFORM_RA || DEBUG_PLATFORM_TI)
volatile uint32_t _debug_tick_ms = 0;
#endif



#if DEBUG_PLATFORM_STM32
static UART_HandleTypeDef *_huart = NULL;
#elif DEBUG_PLATFORM_RA
static uart_instance_t const *_uart = NULL;
#elif DEBUG_PLATFORM_TI
static UART_Regs *_uart_inst = NULL;
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
#endif



static uint32_t _getTick(void) {
#if DEBUG_PLATFORM_STM32
    return HAL_GetTick();
#elif (DEBUG_PLATFORM_RA || DEBUG_PLATFORM_TI)
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
const char* customTextColor(uint8_t r, uint8_t g, uint8_t b)
{
    sprintf(_ansi, "\033[38;2;%u;%u;%um", (unsigned)r, (unsigned)g, (unsigned)b);
    return _ansi;
}

const char* customBgColor(uint8_t r, uint8_t g, uint8_t b)
{
    sprintf(_ansi, "\033[48;2;%u;%u;%um", (unsigned)r, (unsigned)g, (unsigned)b);
    return _ansi;
}
#endif
