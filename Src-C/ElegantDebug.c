/*******************************************************************************
 * @file    ElegantDebug.c
 * @version 1.6
 * @brief   C implementation for ANSI-colored debug logging — STM32 HAL, Renesas RA FSP, TI MSPM0 DL, and 8051.
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
 * @changelog: 2026-11-14: Added 8051/STC89C52/STC89C52RC support (USE_8051).
 * - (See header file)
 ******************************************************************************/

#include "ElegantDebug.h"

#if DEBUG_PLATFORM_8051
#include <reg52.h>          /* EA / ES / SBUF / TI */
#endif

// RA FSP / TI MSPM0 / 8051 tick provider (User must feed from timer ISR)
#if (DEBUG_PLATFORM_RA || DEBUG_PLATFORM_TI || DEBUG_PLATFORM_8051)
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
/* ---- Keil C51 support ----------------------------------------------
 * Keil C51 has no (v)snprintf, and variadic functions must be declared
 * `reentrant`. Because C51's va_list is a based pointer that must not
 * cross a function-call boundary, ED_VSNPRINTF is implemented as a
 * MACRO: vsprintf() runs inside the variadic debug function itself,
 * staging into the shared `combined` buffer, and the result is then
 * clamped into the destination buffer.
 *
 * Instead of per-call buffers (which would need far more XDATA than the
 * 256 bytes of on-chip XDATA that a 512-byte-RAM part like the
 * STC89C52RC provides), all large buffers are shared file-scope
 * `static xdata` variables:
 *
 *   msg       DEBUG_BUFFER_LEN             64 bytes
 *   combined  DEBUG_BUFFER_LEN + 48       112 bytes (also staging)
 *   tsbuf     24 bytes (inside _send)
 *   ansi      2 x 24 bytes (color helpers)
 *   ----------------------------------  ~248 bytes of XDATA in total
 *
 * Notes:
 * - Keep the formatted message shorter than sizeof(combined) bytes;
 *   vsprintf() is unbounded, just like any sprintf()-based C51 code.
 * - The debug functions use shared buffers and must therefore not be
 *   called from ISRs.
 * --------------------------------------------------------------------- */

static xdata char msg[DEBUG_BUFFER_LEN];            /* shared formatted message      */
static xdata char combined[DEBUG_BUFFER_LEN + 48];  /* shared prefix text / staging  */

/* Keil C51 lacks (v)snprintf. Implement a bounded wrapper: vsprintf into
   the shared staging buffer, then copy with a length limit. */
#define ED_VSNPRINTF(buf, size, fmt, ap)                                \
    do {                                                                \
        int ed_n = vsprintf(combined, (fmt), (ap));                     \
        unsigned int ed_i;                                              \
        if (ed_n < 0) ed_n = 0;                                         \
        if ((unsigned int)ed_n > (unsigned int)(size) - 1)              \
            ed_n = (int)((unsigned int)(size) - 1);                     \
        for (ed_i = 0; ed_i < (unsigned int)ed_n; ed_i++)               \
            (buf)[ed_i] = combined[ed_i];                               \
        (buf)[ed_n] = '\0';                                             \
    } while (0)

/* Bounded snprintf replacement for the library's own fixed, bounded
   format strings (variadic => must be reentrant on C51). */
static int ED_SNPRINTF(char *buf, unsigned int size, const char *fmt, ...) reentrant
{
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsprintf(buf, fmt, ap);
    va_end(ap);
    if (n < 0) { buf[0] = '\0'; return 0; }
    if ((unsigned int)n >= size) { buf[size - 1] = '\0'; n = (int)size - 1; }
    return n;
}

#define ED_REENTRANT reentrant    /* C51 requires reentrant for va_args */
#else
#define ED_SNPRINTF  snprintf
#define ED_VSNPRINTF vsnprintf
#define ED_REENTRANT
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
// 8051 has no UART handle concept: the user configures UART0 (mode 1, chosen
// baud rate, TI initial state) in main(); the library only writes SBUF / polls TI.
void debug_init(bool enable_timestamp, bool enable_color, bool enable_filename_line)
{
    _timestamp_enabled = enable_timestamp;
    _color_enabled = enable_color;
    _filename_line_enabled = enable_filename_line;
}
#endif

static uint32_t _getTick(void)
{
#if DEBUG_PLATFORM_STM32
    return HAL_GetTick();
#elif (DEBUG_PLATFORM_RA || DEBUG_PLATFORM_TI || DEBUG_PLATFORM_8051)
#if DEBUG_PLATFORM_8051
    {   /* 32-bit read is not atomic on 8051; guard against ISR tearing */
        uint32_t t;
        bit ea_save = EA;      /* restore the previous interrupt state */
        EA = 0;
        t = _debug_tick_ms;
        EA = ea_save;
        return t;
    }
#else
    return _debug_tick_ms;
#endif
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
// 8051 tick provider (user must call from a 1 ms timer ISR)
void debug_tick(void) { _debug_tick_ms++; }

/* Polled transmit over UART0. ES is masked while polling TI so that the
   serial ISR (which does not clear TI) cannot storm the CPU in the middle
   of a transfer. */
static void _tx8051(const char *s) {
    unsigned int i;
    bit es_save = ES;
    ES = 0;
    for (i = 0; s[i] != '\0'; i++) {
        SBUF = s[i];
        while (!TI);
        TI = 0;
    }
    ES = es_save;
}
#endif

static void _send(const char* text)
{
#if DEBUG_PLATFORM_8051
    /* 8051: keep XDATA usage small — the timestamp and the text are sent
       as two chunks instead of being copied into one big out[] buffer. */
    if (text == NULL) return;

    if (_timestamp_enabled) {
        static xdata char tsbuf[32];
        uint32_t ms = _getTick();
        uint32_t s = ms / 1000U;
        uint32_t hours = s / 3600U;
        uint32_t minutes = (s % 3600U) / 60U;
        uint32_t seconds = s % 60U;
        ED_SNPRINTF(tsbuf, sizeof(tsbuf), "[%02lu:%02lu:%02lu.%03lu] ",
                    (unsigned long)hours, (unsigned long)minutes,
                    (unsigned long)seconds, (unsigned long)(ms % 1000U));
        _tx8051(tsbuf);
    }
    _tx8051(text);
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
        pos = ED_SNPRINTF(out, sizeof(out), "[%02lu:%02lu:%02lu.%03lu] ",
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
#endif
}

void debug_log(const char* format, ...) ED_REENTRANT {
#if DEBUG_PLATFORM_8051
    /* msg is a shared file-scope xdata buffer on 8051 (see top of file) */
#else
    char msg[DEBUG_BUFFER_LEN];
#endif
    va_list args;
    va_start(args, format);
    ED_VSNPRINTF(msg, sizeof(msg), format, args);
    va_end(args);
    _send(msg);
}

void debug_logWithType(const char* type, const char* style, const char* format, ...) ED_REENTRANT {
#if DEBUG_PLATFORM_8051
    /* msg and combined are shared file-scope xdata buffers on 8051 */
#else
    char msg[DEBUG_BUFFER_LEN];
    char combined[DEBUG_BUFFER_LEN + 32];
#endif
    va_list args;
    va_start(args, format);
    ED_VSNPRINTF(msg, sizeof(msg), format, args);
    va_end(args);

    ED_SNPRINTF(combined, sizeof(combined), "\033[1m%s[%s]\033[0m %s", style, type, msg);
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

void debug_error_fileline(const char* file, int line, const char* format, ...) ED_REENTRANT {
#if DEBUG_PLATFORM_8051
    /* msg and combined are shared file-scope xdata buffers on 8051 */
#else
    char msg[DEBUG_BUFFER_LEN];
    char combined[DEBUG_BUFFER_LEN + 96];
#endif
    const char* prefix;
    va_list args;
    va_start(args, format);
    ED_VSNPRINTF(msg, sizeof(msg), format, args);   /* va_list must not go through a variadic snprintf */
    va_end(args);

    prefix = _color_enabled ? ERROR_TYPE : ERROR_TYPE_PLAIN;
    if (_filename_line_enabled) {
        ED_SNPRINTF(combined, sizeof(combined), "%s[%s:%d] %s", prefix, file, line, msg);
    } else {
        ED_SNPRINTF(combined, sizeof(combined), "%s%s", prefix, msg);
    }
    _send(combined);
}

void debug_warning_fileline(const char* file, int line, const char* format, ...) ED_REENTRANT {
#if DEBUG_PLATFORM_8051
    /* msg and combined are shared file-scope xdata buffers on 8051 */
#else
    char msg[DEBUG_BUFFER_LEN];
    char combined[DEBUG_BUFFER_LEN + 96];
#endif
    const char* prefix;
    va_list args;
    va_start(args, format);
    ED_VSNPRINTF(msg, sizeof(msg), format, args);
    va_end(args);

    prefix = _color_enabled ? WARNING_TYPE : WARNING_TYPE_PLAIN;
    if (_filename_line_enabled) {
        ED_SNPRINTF(combined, sizeof(combined), "%s[%s:%d] %s", prefix, file, line, msg);
    } else {
        ED_SNPRINTF(combined, sizeof(combined), "%s%s", prefix, msg);
    }
    _send(combined);
}

#if DEBUG_PLATFORM_8051
/* Keil C51's preprocessor does not support __VA_ARGS__, so the
   debug_error()/debug_warning() convenience macros cannot capture
   __FILE__/__LINE__. Plain variadic functions are provided instead;
   call debug_error_fileline()/debug_warning_fileline() directly when
   the calling location is needed. */
void debug_error(const char* format, ...) reentrant {
    const char* prefix;
    va_list args;
    va_start(args, format);
    ED_VSNPRINTF(msg, sizeof(msg), format, args);
    va_end(args);

    prefix = _color_enabled ? ERROR_TYPE : ERROR_TYPE_PLAIN;
    ED_SNPRINTF(combined, sizeof(combined), "%s%s", prefix, msg);
    _send(combined);
}

void debug_warning(const char* format, ...) reentrant {
    const char* prefix;
    va_list args;
    va_start(args, format);
    ED_VSNPRINTF(msg, sizeof(msg), format, args);
    va_end(args);

    prefix = _color_enabled ? WARNING_TYPE : WARNING_TYPE_PLAIN;
    ED_SNPRINTF(combined, sizeof(combined), "%s%s", prefix, msg);
    _send(combined);
}
#endif

void debug_ok(const char* format, ...) ED_REENTRANT {
#if DEBUG_PLATFORM_8051
    /* msg and combined are shared file-scope xdata buffers on 8051 */
#else
    char msg[DEBUG_BUFFER_LEN];
    char combined[DEBUG_BUFFER_LEN + 32];
#endif
    const char* prefix;
    va_list args;
    va_start(args, format);
    ED_VSNPRINTF(msg, sizeof(msg), format, args);
    va_end(args);

    prefix = _color_enabled ? OK_TYPE : OK_TYPE_PLAIN;
    ED_SNPRINTF(combined, sizeof(combined), "%s%s", prefix, msg);
    _send(combined);
}

void debug_success(const char* format, ...) ED_REENTRANT {
#if DEBUG_PLATFORM_8051
    /* msg and combined are shared file-scope xdata buffers on 8051 */
#else
    char msg[DEBUG_BUFFER_LEN];
    char combined[DEBUG_BUFFER_LEN + 32];
#endif
    const char* prefix;
    va_list args;
    va_start(args, format);
    ED_VSNPRINTF(msg, sizeof(msg), format, args);
    va_end(args);

    prefix = _color_enabled ? SUCCESS_TYPE : SUCCESS_TYPE_PLAIN;
    ED_SNPRINTF(combined, sizeof(combined), "%s%s", prefix, msg);
    _send(combined);
}

void debug_info(const char* format, ...) ED_REENTRANT {
#if DEBUG_PLATFORM_8051
    /* msg and combined are shared file-scope xdata buffers on 8051 */
#else
    char msg[DEBUG_BUFFER_LEN];
    char combined[DEBUG_BUFFER_LEN + 32];
#endif
    const char* prefix;
    va_list args;
    va_start(args, format);
    ED_VSNPRINTF(msg, sizeof(msg), format, args);
    va_end(args);

    prefix = _color_enabled ? INFO_TYPE : INFO_TYPE_PLAIN;
    ED_SNPRINTF(combined, sizeof(combined), "%s%s", prefix, msg);
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
#if DEBUG_PLATFORM_8051
    static xdata char ansi[24];     /* keep the buffer out of the small DATA space */
#else
    static char ansi[24];
#endif
    ED_SNPRINTF(ansi, sizeof(ansi), "\033[38;2;%u;%u;%um", (unsigned)r, (unsigned)g, (unsigned)b);
    return ansi;
}

const char* customBgColor(uint8_t r, uint8_t g, uint8_t b) {
#if DEBUG_PLATFORM_8051
    static xdata char ansi[24];     /* keep the buffer out of the small DATA space */
#else
    static char ansi[24];
#endif
    ED_SNPRINTF(ansi, sizeof(ansi), "\033[48;2;%u;%u;%um", (unsigned)r, (unsigned)g, (unsigned)b);
    return ansi;
}
