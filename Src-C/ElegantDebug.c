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

#if defined(__C51__)
    #include <REG52.H>
    #define DEBUG_8051_IDATA idata
#elif defined(__SDCC_mcs51)
    #include <8052.h>
    #define DEBUG_8051_IDATA __idata
#else
    #define DEBUG_8051_IDATA
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
#if defined(__C51__)
    uint32_t ms;
    bit saved_ea = EA;
    EA = 0; // A 32-bit read is not atomic on an 8-bit MCU.
    ms = _debug_tick_ms;
    EA = saved_ea;
    return ms;
#elif defined(__SDCC_mcs51)
    uint32_t ms;
    __critical {
        ms = _debug_tick_ms;
    }
    return ms;
#else
    return _debug_tick_ms;
#endif
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
        // Keep printf arguments within C51 SMALL model's default MAXARGS(15).
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

#else // Original STM32 / RA / TI implementation.


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

#endif // DEBUG_PLATFORM_8051
