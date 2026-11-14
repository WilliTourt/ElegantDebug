/*******************************************************************************
 * @file    ElegantDebug.c
 * @version 1.7
 * @brief   C implementation for ANSI-colored debug logging — STM32 HAL, Renesas RA FSP, TI MSPM0 DL, 8051.
 *
 * Implements the C API declared in `Src-C/ElegantDebug.h`. Provides formatted
 * logging functions that send output over a HAL UART interface (STM32),
 * SCI UART (Renesas RA), or DL UART (TI MSPM0); USB-CDC is also supported
 * on STM32 when `USB_AS_DEBUG_PORT` is enabled. On 8051 (Keil C51) the output
 * is streamed byte by byte through a user supplied transmit function. Supports
 * optional timestamps and ANSI color prefixes.
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
// Keil C51 provides neither snprintf() nor vsnprintf(). The bounded
// implementations below are used instead, so every call site in this file stays
// unchanged (and stays memory safe on a device with 128 bytes of RAM).
#define snprintf  debug_snprintf
#define vsnprintf debug_vsnprintf

// The 8051 cannot afford a DEBUG_BUFFER_LEN byte frame in every log function
// (C51 is unable to overlay variadic functions), so all of them share one
// message buffer. The library is not reentrant on any platform anyway.
#define DEBUG_MSG_DECL  /* the shared `msg` buffer below is used instead */
#else
#define DEBUG_MSG_DECL  char msg[DEBUG_BUFFER_LEN];
#endif



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
static debug_putc_t _uart_putc = NULL;
#endif

static bool _timestamp_enabled = true;
static bool _color_enabled = true;
static bool _filename_line_enabled = false;

#if DEBUG_PLATFORM_8051
// One shared message buffer for every log function (see DEBUG_MSG_DECL above).
static char DEBUG_8051_BUF_MEM msg[DEBUG_BUFFER_LEN];
// Shared scratch buffers for the timestamp / line number / ANSI colour strings.
// A single set of them, instead of one per helper, saves ~70 bytes of RAM.
static char DEBUG_8051_BUF_MEM _dbg_ts[20];
static char DEBUG_8051_BUF_MEM _dbg_ansi[24];
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
void debug_init(debug_putc_t uart_putc, bool enable_timestamp, bool enable_color, bool enable_filename_line) {
    _uart_putc = uart_putc;
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
/* ===========================================================================
 * 8051 (Keil C51) support helpers
 *
 * An 8051 is quite different from a Cortex-M, so three things are handled here:
 *  1. C51 has no snprintf()/vsnprintf(); a small bounded formatter is provided
 *     below and mapped onto those names at the top of this file.
 *  2. Only 128/256 bytes of internal RAM are available; output is therefore
 *     streamed byte by byte through the user supplied function instead of being
 *     assembled in a large temporary buffer. The only per-call buffer that
 *     remains is the DEBUG_BUFFER_LEN message buffer itself.
 *  3. String literals live in CODE space; all helpers take generic
 *     `const char *` pointers, so CODE, DATA and XDATA strings all work.
 * =========================================================================*/

/* ---- bounded output stream used by the formatter ------------------------ */
typedef struct {
    char  *buf;
    size_t size;
    size_t len;         /* number of characters that *would* be written */
} _dbg_out_t;

static void _dbg_out_ch(_dbg_out_t *out, char c) {
    if (out->size > 0 && out->len < (out->size - 1)) {
        out->buf[out->len] = c;
    }
    out->len++;
}

static void _dbg_out_repeat(_dbg_out_t *out, char c, int n) {
    while (n > 0) {
        _dbg_out_ch(out, c);
        n--;
    }
}

/* Emit an unsigned value in the given base, honouring width / precision / flags. */
static void _dbg_out_uint(_dbg_out_t *out, unsigned long value, unsigned char base,
                          bool upper, int width, int prec, bool left, bool zero, char sign)
                          DEBUG_REENTRANT {
    char tmp[12];
    int  digits = 0;
    int  total;
    int  pad;

    if (!(prec == 0 && value == 0)) {
        do {
            unsigned char d = (unsigned char)(value % (unsigned long)base);
            if (d < 10) {
                tmp[digits++] = (char)('0' + d);
            } else {
                tmp[digits++] = (char)((upper ? 'A' : 'a') + (d - 10));
            }
            value /= (unsigned long)base;
        } while (value != 0);
    }

    total = (digits > prec) ? digits : prec;
    if (sign != '\0') {
        total++;
    }
    pad = width - total;
    if (pad < 0) {
        pad = 0;
    }

    if (!left && !(zero && prec < 0)) {
        _dbg_out_repeat(out, ' ', pad);
    }
    if (sign != '\0') {
        _dbg_out_ch(out, sign);
    }
    if (!left && zero && prec < 0) {
        _dbg_out_repeat(out, '0', pad);
    }
    _dbg_out_repeat(out, '0', total - digits - ((sign != '\0') ? 1 : 0));
    while (digits > 0) {
        _dbg_out_ch(out, tmp[--digits]);
    }
    if (left) {
        _dbg_out_repeat(out, ' ', pad);
    }
}

static void _dbg_out_strn(_dbg_out_t *out, const char *s, int width, int prec, bool left) DEBUG_REENTRANT {
    int len = 0;
    int pad;
    int i;

    if (s == NULL) {
        s = "(null)";
    }
    while (s[len] != '\0' && (prec < 0 || len < prec)) {
        len++;
    }
    pad = width - len;
    if (pad < 0) {
        pad = 0;
    }
    if (!left) {
        _dbg_out_repeat(out, ' ', pad);
    }
    for (i = 0; i < len; i++) {
        _dbg_out_ch(out, s[i]);
    }
    if (left) {
        _dbg_out_repeat(out, ' ', pad);
    }
}

/* Fixed point style float output: C51 has no floating point printf, and a
   32-bit float only carries ~7 significant digits anyway. */
#if DEBUG_8051_ENABLE_FLOAT
static void _dbg_out_float(_dbg_out_t *out, double value, int width, int prec,
                           bool left, bool zero, bool plus, bool space) DEBUG_REENTRANT {
    char          ibuf[11];
    char          fbuf[8];
    int           idig = 0;
    int           i;
    int           total;
    int           pad;
    bool          neg = false;
    unsigned long ip;
    unsigned long frac = 0;
    unsigned long scale = 1;

    if (prec < 0) {
        prec = 6;
    }
    if (prec > 6) {
        prec = 6;
    }

    if (value < 0) {
        neg = true;
        value = -value;
    }
    if (value > 4294967040.0) {          /* would not fit in unsigned long */
        value = 4294967040.0;
    }

    ip = (unsigned long)value;
    for (i = 0; i < prec; i++) {
        scale *= 10UL;
    }
    frac = (unsigned long)((value - (double)ip) * (double)scale + 0.5);
    if (frac >= scale) {
        frac -= scale;
        ip++;
    }

    do {
        ibuf[idig++] = (char)('0' + (char)(ip % 10UL));
        ip /= 10UL;
    } while (ip != 0);

    total = idig + ((prec > 0) ? (prec + 1) : 0) + ((neg || plus || space) ? 1 : 0);
    pad = width - total;
    if (pad < 0) {
        pad = 0;
    }

    if (!left && !zero) {
        _dbg_out_repeat(out, ' ', pad);
    }
    if (neg) {
        _dbg_out_ch(out, '-');
    } else if (plus) {
        _dbg_out_ch(out, '+');
    } else if (space) {
        _dbg_out_ch(out, ' ');
    }
    if (!left && zero) {
        _dbg_out_repeat(out, '0', pad);
    }
    while (idig > 0) {
        _dbg_out_ch(out, ibuf[--idig]);
    }
    if (prec > 0) {
        _dbg_out_ch(out, '.');
        for (i = prec - 1; i >= 0; i--) {
            fbuf[i] = (char)('0' + (char)(frac % 10UL));
            frac /= 10UL;
        }
        for (i = 0; i < prec; i++) {
            _dbg_out_ch(out, fbuf[i]);
        }
    }
    if (left) {
        _dbg_out_repeat(out, ' ', pad);
    }
}
#endif /* DEBUG_8051_ENABLE_FLOAT */

/* snprintf() replacement: same contract as C99, i.e. the return value is the
   length the output would have had, and the buffer is always NUL terminated. */
int debug_vsnprintf(char *buf, size_t size, const char *format, va_list args) DEBUG_REENTRANT {
    _dbg_out_t out;
    char       c;

    out.buf  = buf;
    out.size = size;
    out.len  = 0;

    while (format != NULL && *format != '\0') {
        c = *format++;
        if (c != '%') {
            _dbg_out_ch(&out, c);
            continue;
        }
        if (*format == '%') {
            _dbg_out_ch(&out, '%');
            format++;
            continue;
        }

        {
            bool left = false;
            bool zero = false;
            bool plus = false;
            bool space = false;
            bool lng = false;
            int  width = 0;
            int  prec = -1;

            /* flags */
            for (;;) {
                c = *format;
                if (c == '-') {
                    left = true;
                } else if (c == '0') {
                    zero = true;
                } else if (c == '+') {
                    plus = true;
                } else if (c == ' ') {
                    space = true;
                } else if (c == '#') {
                    /* accepted, but no alternative form is produced */
                } else {
                    break;
                }
                format++;
            }

            /* field width */
            if (*format == '*') {
                width = va_arg(args, int);
                format++;
                if (width < 0) {
                    left = true;
                    width = -width;
                }
            } else {
                while (*format >= '0' && *format <= '9') {
                    width = width * 10 + (*format - '0');
                    format++;
                }
            }

            /* precision */
            if (*format == '.') {
                format++;
                prec = 0;
                if (*format == '*') {
                    prec = va_arg(args, int);
                    format++;
                } else {
                    while (*format >= '0' && *format <= '9') {
                        prec = prec * 10 + (*format - '0');
                        format++;
                    }
                }
            }

            /* length modifier: 'l' is honoured, 'h' is accepted and ignored */
            if (*format == 'l') {
                lng = true;
                format++;
            } else if (*format == 'h') {
                format++;
            }

            c = *format;
            if (c == '\0') {
                break;
            }
            format++;

            switch (c) {
            case 'd':
            case 'i': {
                long          v = lng ? va_arg(args, long) : (long)va_arg(args, int);
                char          sign = '\0';
                unsigned long mag;

                if (v < 0) {
                    sign = '-';
                    /* avoid overflow on the most negative value */
                    mag = (unsigned long)(-(v + 1L)) + 1UL;
                } else {
                    mag = (unsigned long)v;
                    if (plus) {
                        sign = '+';
                    } else if (space) {
                        sign = ' ';
                    }
                }
                _dbg_out_uint(&out, mag, 10, false, width, prec, left, zero, sign);
                break;
            }
            case 'u':
            case 'o':
            case 'x':
            case 'X': {
                unsigned long mag = lng ? va_arg(args, unsigned long)
                                        : (unsigned long)va_arg(args, unsigned int);
                unsigned char base = (unsigned char)((c == 'o') ? 8 : ((c == 'u') ? 10 : 16));
                _dbg_out_uint(&out, mag, base, (bool)(c == 'X'), width, prec, left, zero, '\0');
                break;
            }
            case 'c': {
                char ch = (char)va_arg(args, int);
                int  pad = width - 1;

                if (pad < 0) {
                    pad = 0;
                }
                if (!left) {
                    _dbg_out_repeat(&out, ' ', pad);
                }
                _dbg_out_ch(&out, ch);
                if (left) {
                    _dbg_out_repeat(&out, ' ', pad);
                }
                break;
            }
            case 's': {
                const char *s = va_arg(args, const char *);
                _dbg_out_strn(&out, s, width, prec, left);
                break;
            }
            case 'f':
            case 'F':
            case 'g':
            case 'e': {
                /* The argument is consumed even when floating point support is
                   disabled, otherwise every following conversion would read the
                   wrong argument. */
                double v = va_arg(args, double);
#if DEBUG_8051_ENABLE_FLOAT
                _dbg_out_float(&out, v, width, prec, left, zero, plus, space);
#else
                (void)v;
                _dbg_out_strn(&out, "?", width, prec, left);
#endif
                break;
            }
            default:
                /* unsupported conversion: print it back unchanged */
                _dbg_out_ch(&out, '%');
                _dbg_out_ch(&out, c);
                break;
            }
        }
    }

    if (out.size > 0) {
        size_t end = (out.len < out.size) ? out.len : (out.size - 1);
        out.buf[end] = '\0';
    }
    return (int)out.len;
}

int debug_snprintf(char *buf, size_t size, const char *format, ...) DEBUG_REENTRANT {
    va_list args;
    int     n;

    va_start(args, format);
    n = debug_vsnprintf(buf, size, format, args);
    va_end(args);
    return n;
}

/* ---- streaming output through the user supplied byte function ----------- */

static void _8051_put(char c) {
    if (_uart_putc != NULL) {
        _uart_putc(c);
    }
}

static void _8051_puts(const char *s) {
    if (s == NULL) {
        return;
    }
    while (*s != '\0') {
        _8051_put(*s);
        s++;
    }
}

/* Decimal output of a 16-bit value (used for the line number). */
static void _8051_put_uint16(unsigned int v) DEBUG_REENTRANT {
    char nb[6];
    int  i = 0;

    if (v == 0) {
        _8051_put('0');
        return;
    }
    while (v != 0 && i < 5) {
        nb[i++] = (char)('0' + (char)(v % 10U));
        v /= 10U;
    }
    while (i > 0) {
        i--;
        _8051_put(nb[i]);
    }
}

/* Same "[HH:MM:SS.mmm] " timestamp as the other platforms. */
static void _8051_put_timestamp(void) DEBUG_REENTRANT {
    uint32_t ms;
    uint32_t s;
    uint32_t hours;
    uint32_t minutes;
    uint32_t seconds;

    if (!_timestamp_enabled || _uart_putc == NULL) {
        return;
    }

    ms = _getTick();
    s = ms / 1000U;
    hours = s / 3600U;
    minutes = (s % 3600U) / 60U;
    seconds = s % 60U;

    snprintf(_dbg_ts, sizeof(_dbg_ts), "[%02lu:%02lu:%02lu.%03lu] ",
             (unsigned long)hours, (unsigned long)minutes,
             (unsigned long)seconds, (unsigned long)(ms % 1000U));
    _8051_puts(_dbg_ts);
}

/* timestamp + prefix + optional "[file:line] " + message, streamed in order. */
static void _8051_send_prefixed(const char *prefix, const char *file, int line) DEBUG_REENTRANT {
    _8051_put_timestamp();
    _8051_puts(prefix);
    if (file != NULL) {
        _8051_put('[');
        _8051_puts(file);
        _8051_put(':');
        _8051_put_uint16((unsigned int)line);
        _8051_puts("] ");
    }
}
#endif /* DEBUG_PLATFORM_8051 */



static void _send(const char* text) DEBUG_REENTRANT {

#if DEBUG_PLATFORM_8051
    /* Stream through the user hook: no temporary buffer is needed, which keeps
       the stack frame small enough for an 8051 with 128 bytes of RAM. */
    if (_uart_putc == NULL || text == NULL) return;
    _8051_put_timestamp();
    _8051_puts(text);
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

#endif /* DEBUG_PLATFORM_8051 */
}



void debug_log(const char* format, ...) DEBUG_REENTRANT {
    va_list args;
    DEBUG_MSG_DECL
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);
    _send(msg);
}

void debug_logWithType(const char* type, const char* style, const char* format, ...) DEBUG_REENTRANT {
    va_list args;
    DEBUG_MSG_DECL
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

#if DEBUG_PLATFORM_8051
    /* Built up as a stream: no extra buffer is required on the 8051. */
    _8051_put_timestamp();
    _8051_puts("\033[1m");
    _8051_puts(style);
    _8051_put('[');
    _8051_puts(type);
    _8051_puts("]\033[0m ");
    _8051_puts(msg);
#else
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

void debug_error_fileline(const char* file, int line, const char* format, ...) DEBUG_REENTRANT {
    va_list args;
    DEBUG_MSG_DECL
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

#if DEBUG_PLATFORM_8051
    _8051_send_prefixed(_color_enabled ? ERROR_TYPE : ERROR_TYPE_PLAIN,
                        _filename_line_enabled ? file : NULL, line);
    _8051_puts(msg);
#else
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

void debug_warning_fileline(const char* file, int line, const char* format, ...) DEBUG_REENTRANT {
    va_list args;
    DEBUG_MSG_DECL
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

#if DEBUG_PLATFORM_8051
    _8051_send_prefixed(_color_enabled ? WARNING_TYPE : WARNING_TYPE_PLAIN,
                        _filename_line_enabled ? file : NULL, line);
    _8051_puts(msg);
#else
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

void debug_ok(const char* format, ...) DEBUG_REENTRANT {
    va_list args;
    DEBUG_MSG_DECL
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

#if DEBUG_PLATFORM_8051
    _8051_send_prefixed(_color_enabled ? OK_TYPE : OK_TYPE_PLAIN, NULL, 0);
    _8051_puts(msg);
#else
    const char* prefix = _color_enabled ? OK_TYPE : OK_TYPE_PLAIN;
    char combined[DEBUG_BUFFER_LEN + 32];
    snprintf(combined, sizeof(combined), "%s%s", prefix, msg);
    _send(combined);
#endif
}

void debug_success(const char* format, ...) DEBUG_REENTRANT {
    va_list args;
    DEBUG_MSG_DECL
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

#if DEBUG_PLATFORM_8051
    _8051_send_prefixed(_color_enabled ? SUCCESS_TYPE : SUCCESS_TYPE_PLAIN, NULL, 0);
    _8051_puts(msg);
#else
    const char* prefix = _color_enabled ? SUCCESS_TYPE : SUCCESS_TYPE_PLAIN;
    char combined[DEBUG_BUFFER_LEN + 32];
    snprintf(combined, sizeof(combined), "%s%s", prefix, msg);
    _send(combined);
#endif
}

void debug_info(const char* format, ...) DEBUG_REENTRANT {
    va_list args;
    DEBUG_MSG_DECL
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

#if DEBUG_PLATFORM_8051
    _8051_send_prefixed(_color_enabled ? INFO_TYPE : INFO_TYPE_PLAIN, NULL, 0);
    _8051_puts(msg);
#else
    const char* prefix = _color_enabled ? INFO_TYPE : INFO_TYPE_PLAIN;
    char combined[DEBUG_BUFFER_LEN + 32];
    snprintf(combined, sizeof(combined), "%s%s", prefix, msg);
    _send(combined);
#endif
}



#if DEBUG_PLATFORM_8051
/* Keil C51 has no variadic macros, so the header maps debug_error() and
   debug_warning() onto these location-less variants. Nothing is lost: use
   debug_error_fileline(__FILE__, __LINE__, ...) when the location is wanted. */
void debug_error_plain(const char* format, ...) DEBUG_REENTRANT {
    va_list args;
    DEBUG_MSG_DECL
    va_start(args, format);
    debug_vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

    _8051_send_prefixed(_color_enabled ? ERROR_TYPE : ERROR_TYPE_PLAIN, NULL, 0);
    _8051_puts(msg);
}

void debug_warning_plain(const char* format, ...) DEBUG_REENTRANT {
    va_list args;
    DEBUG_MSG_DECL
    va_start(args, format);
    debug_vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

    _8051_send_prefixed(_color_enabled ? WARNING_TYPE : WARNING_TYPE_PLAIN, NULL, 0);
    _8051_puts(msg);
}
#endif



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
#if DEBUG_PLATFORM_8051
    /* shared buffer, see _dbg_ansi above */
    snprintf(_dbg_ansi, sizeof(_dbg_ansi), "\033[38;2;%u;%u;%um", (unsigned)r, (unsigned)g, (unsigned)b);
    return _dbg_ansi;
#else
    static char ansi[24];
    snprintf(ansi, sizeof(ansi), "\033[38;2;%u;%u;%um", (unsigned)r, (unsigned)g, (unsigned)b);
    return ansi;
#endif
}

const char* customBgColor(uint8_t r, uint8_t g, uint8_t b) {
#if DEBUG_PLATFORM_8051
    /* shared buffer, see _dbg_ansi above */
    snprintf(_dbg_ansi, sizeof(_dbg_ansi), "\033[48;2;%u;%u;%um", (unsigned)r, (unsigned)g, (unsigned)b);
    return _dbg_ansi;
#else
    static char ansi[24];
    snprintf(ansi, sizeof(ansi), "\033[48;2;%u;%u;%um", (unsigned)r, (unsigned)g, (unsigned)b);
    return ansi;
#endif
}
