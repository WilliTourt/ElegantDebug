#include "ElegantDebug.h"

volatile uint32_t _debug_tick_ms = 0;

static bool _timestamp_enabled = true;
static bool _color_enabled = true;
static bool _filename_line_enabled = false;

#ifdef __C51__
void debug_init(bool enable_timestamp, bool enable_color, bool enable_filename_line) {
    _timestamp_enabled = enable_timestamp;
    _color_enabled = enable_color;
    _filename_line_enabled = enable_filename_line;
}
#else
void debug_init(void* huart, bool enable_timestamp, bool enable_color, bool enable_filename_line) {
    (void)huart;
    _timestamp_enabled = enable_timestamp;
    _color_enabled = enable_color;
    _filename_line_enabled = enable_filename_line;
}
#endif

static uint32_t _getTick(void) {
#ifdef __C51__
    uint32_t ms;
    bool saved_ea = EA;
    EA = 0;
    ms = _debug_tick_ms;
    EA = saved_ea;
    return ms;
#else
    return _debug_tick_ms;
#endif
}

static void _send(const char* text) {
    if (text == 0) return;

#ifdef __C51__
    while (*text != '\0') {
        SBUF = *text++;
        while (TI == 0); 
        TI = 0;          
    }
#else
    while (*text != '\0') {
        putchar(*text++);
    }
#endif
}

#ifdef __C51__
void debug_log(const char* text) {
    _send(text); _send("\r\n");
}

void debug_logWithType(const char* type, const char* style, const char* text) {
    _send("\033[1m"); _send(style); _send("[");
    _send(type); _send("]\033[0m "); _send(text); _send("\r\n");
}

void debug_ok(const char* text) {
    _send(_color_enabled ? OK_TYPE : OK_TYPE_PLAIN);
    _send(text); _send("\r\n");
}

void debug_success(const char* text) {
    _send(_color_enabled ? SUCCESS_TYPE : SUCCESS_TYPE_PLAIN);
    _send(text); _send("\r\n");
}

void debug_info(const char* text) {
    _send(_color_enabled ? INFO_TYPE : INFO_TYPE_PLAIN);
    _send(text); _send("\r\n");
}

void debug_error_fileline(const char* file, int line, const char* text) {
    _send(_color_enabled ? ERROR_TYPE : ERROR_TYPE_PLAIN);
    _send(text); _send("\r\n");
}

void debug_warning_fileline(const char* file, int line, const char* text) {
    _send(_color_enabled ? WARNING_TYPE : WARNING_TYPE_PLAIN);
    _send(text); _send("\r\n");
}

#else
void debug_log(const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);
    _send(msg); _send("\r\n");
}

void debug_logWithType(const char* type, const char* style, const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);
    _send("\033[1m"); _send(style); _send("[");
    _send(type); _send("]\033[0m "); _send(msg); _send("\r\n");
}

void debug_ok(const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);
    _send(_color_enabled ? OK_TYPE : OK_TYPE_PLAIN);
    _send(msg); _send("\r\n");
}

void debug_success(const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);
    _send(_color_enabled ? SUCCESS_TYPE : SUCCESS_TYPE_PLAIN);
    _send(msg); _send("\r\n");
}

void debug_info(const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);
    _send(_color_enabled ? INFO_TYPE : INFO_TYPE_PLAIN);
    _send(msg); _send("\r\n");
}

void debug_error_fileline(const char* file, int line, const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);
    _send(_color_enabled ? ERROR_TYPE : ERROR_TYPE_PLAIN);
    _send(msg); _send("\r\n");
}

void debug_warning_fileline(const char* file, int line, const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);
    _send(_color_enabled ? WARNING_TYPE : WARNING_TYPE_PLAIN);
    _send(msg); _send("\r\n");
}
#endif

void debug_setTimestampEnabled(bool enabled) { _timestamp_enabled = enabled; }
void debug_setColorEnabled(bool enabled) { _color_enabled = enabled; }
void debug_setFilenameLineEnabled(bool enabled) { _filename_line_enabled = enabled; }

const char* customTextColor(uint8_t r, uint8_t g, uint8_t b) {
    static char ansi[24];
    sprintf(ansi, "\033[38;2;%u;%u;%um", (unsigned int)r, (unsigned int)g, (unsigned int)b);
    return ansi;
}

const char* customBgColor(uint8_t r, uint8_t g, uint8_t b) {
    static char ansi[24];
    sprintf(ansi, "\033[48;2;%u;%u;%um", (unsigned int)r, (unsigned int)g, (unsigned int)b);
    return ansi;
}
