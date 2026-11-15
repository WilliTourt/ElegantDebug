/*******************************************************************************
 * @file    ElegantDebug.c
 * @version 1.6
 * @brief   C implementation for ANSI-colored debug logging — Specially used for 8051 (STC89C52RC).
 *
 * SDCC streams formatted characters through the application UART putchar().
 *
 * Initialize the UART and call `debug_init()` before using
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




void debug_init(bool enable_timestamp, bool enable_color, bool enable_filename_line) {
    _timestamp_enabled = enable_timestamp;
    _color_enabled = enable_color;
    _filename_line_enabled = enable_filename_line;
}

static uint32_t _getTick(void)
#ifdef __SDCC_mcs51
    __critical  // A timer interrupt can update this four-byte value.
#endif
{
    return _debug_tick_ms;
}



#ifdef __SDCC_mcs51
// The SDCC small model shares 256 bytes of internal RAM with the stack.
// Print each part directly to the application's putchar().
static void _sendTimestamp(void) {
    if (_timestamp_enabled) {
        uint32_t ms = _getTick();
        uint32_t seconds = ms / 1000UL;
        printf("[%02lu:%02u:%02u.%03u] ", seconds / 3600UL,
               (unsigned)(seconds / 60UL % 60UL), (unsigned)(seconds % 60UL),
               (unsigned)(ms % 1000UL));
    }
}

static void _send(const char *text) {
    while (*text) {
        putchar(*text++);
    }
}

void debug_log(const char *format, ...) {
    va_list args;
    _sendTimestamp();
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void debug_logWithType(const char *type, const char *style, const char *format, ...) {
    va_list args;
    _sendTimestamp();
    if (_color_enabled) {
        _send(BOLD);
        _send(style);
    }
    printf("[%s]", type);
    if (_color_enabled) {
        _send(CLR);
    }
    putchar(' ');
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void debug_error_fileline(const char *file, int line, const char *format, ...) {
    va_list args;
    _sendTimestamp();
    _send(_color_enabled ? ERROR_TYPE : ERROR_TYPE_PLAIN);
    if (_filename_line_enabled) {
        printf("[%s:%d] ", file, line);
    }
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void debug_warning_fileline(const char *file, int line, const char *format, ...) {
    va_list args;
    _sendTimestamp();
    _send(_color_enabled ? WARNING_TYPE : WARNING_TYPE_PLAIN);
    if (_filename_line_enabled) {
        printf("[%s:%d] ", file, line);
    }
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void debug_ok(const char *format, ...) {
    va_list args;
    _sendTimestamp();
    _send(_color_enabled ? OK_TYPE : OK_TYPE_PLAIN);
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void debug_success(const char *format, ...) {
    va_list args;
    _sendTimestamp();
    _send(_color_enabled ? SUCCESS_TYPE : SUCCESS_TYPE_PLAIN);
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void debug_info(const char *format, ...) {
    va_list args;
    _sendTimestamp();
    _send(_color_enabled ? INFO_TYPE : INFO_TYPE_PLAIN);
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

#else
static void _send(const char* text) {

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

    fputs(out, stdout);
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
#ifdef __SDCC_mcs51
    // At most 20 bytes including the NUL (three uint8_t values).
    static __idata char ansi[20];
    sprintf(ansi, "\033[38;2;%u;%u;%um", (unsigned)r, (unsigned)g, (unsigned)b);
#else
    static char ansi[24];
    snprintf(ansi, sizeof(ansi), "\033[38;2;%u;%u;%um", (unsigned)r, (unsigned)g, (unsigned)b);
#endif
    return ansi;
}

const char* customBgColor(uint8_t r, uint8_t g, uint8_t b) {
#ifdef __SDCC_mcs51
    // At most 20 bytes including the NUL (three uint8_t values).
    static __idata char ansi[20];
    sprintf(ansi, "\033[48;2;%u;%u;%um", (unsigned)r, (unsigned)g, (unsigned)b);
#else
    static char ansi[24];
    snprintf(ansi, sizeof(ansi), "\033[48;2;%u;%u;%um", (unsigned)r, (unsigned)g, (unsigned)b);
#endif
    return ansi;
}
