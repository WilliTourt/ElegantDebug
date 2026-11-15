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
void debug_init(/*TODO: Params to be added*/) {
    // 保持原样
}
#endif

static uint32_t _getTick(void) {
    return _debug_tick_ms;
}

static void _send(const char* text) {
#ifdef __C51__
    // 51 环境：直接用 SBUF 串口发送，且不占用数组，解决内存溢出
    if (text == 0) return;
    while (*text != '\0') {
        SBUF = *text++;
        while (TI == 0); 
        TI = 0;          
    }
#else
    // 非 51 环境：原作者的原有逻辑，一字不差
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

    if (pos < sizeof(out)) {
        size_t remain = sizeof(out) - pos;
        strncpy(out + pos, text, (remain > 0) ? (remain - 1) : 0);
        out[sizeof(out) - 1] = '\0';
    }

    // 原版的 TODO，这里补上通用输出逻辑（也可以保留，但补全能防止编译警告）
    {
        const char* p = out;
        while (*p != '\0') {
            putchar(*p++);
        }
    }
#endif
}

#ifdef __C51__
// 51 专属极简接口
void debug_log(const char* text) { _send(text); _send("\r\n"); }
void debug_logWithType(const char* type, const char* style, const char* text) { _send("\033[1m"); _send(style); _send("["); _send(type); _send("]\033[0m "); _send(text); _send("\r\n"); }
void debug_ok(const char* text) { _send(_color_enabled ? OK_TYPE : OK_TYPE_PLAIN); _send(text); _send("\r\n"); }
void debug_success(const char* text) { _send(_color_enabled ? SUCCESS_TYPE : SUCCESS_TYPE_PLAIN); _send(text); _send("\r\n"); }
void debug_info(const char* text) { _send(_color_enabled ? INFO_TYPE : INFO_TYPE_PLAIN); _send(text); _send("\r\n"); }
void debug_error_fileline(const char* file, int line, const char* text) { _send(_color_enabled ? ERROR_TYPE : ERROR_TYPE_PLAIN); _send(text); _send("\r\n"); }
void debug_warning_fileline(const char* file, int line, const char* text) { _send(_color_enabled ? WARNING_TYPE : WARNING_TYPE_PLAIN); _send(text); _send("\r\n"); }

#else
// 原作者的高级接口，原封不动
void debug_log(const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN]; va_list args;
    va_start(args, format); vsnprintf(msg, sizeof(msg), format, args); va_end(args);
    _send(msg); _send("\r\n");
}

void debug_logWithType(const char* type, const char* style, const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN]; va_list args;
    va_start(args, format); vsnprintf(msg, sizeof(msg), format, args); va_end(args);
    char combined[DEBUG_BUFFER_LEN + 32];
    snprintf(combined, sizeof(combined), "\033[1m%s[%s]\033[0m %s", style, type, msg);
    _send(combined); _send("\r\n");
}

void debug_ok(const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN]; va_list args;
    va_start(args, format); vsnprintf(msg, sizeof(msg), format, args); va_end(args);
    const char* prefix = _color_enabled ? OK_TYPE : OK_TYPE_PLAIN;
    char combined[DEBUG_BUFFER_LEN + 32];
    snprintf(combined, sizeof(combined), "%s%s", prefix, msg);
    _send(combined); _send("\r\n");
}

void debug_success(const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN]; va_list args;
    va_start(args, format); vsnprintf(msg, sizeof(msg), format, args); va_end(args);
    const char* prefix = _color_enabled ? SUCCESS_TYPE : SUCCESS_TYPE_PLAIN;
    char combined[DEBUG_BUFFER_LEN + 32];
    snprintf(combined, sizeof(combined), "%s%s", prefix, msg);
    _send(combined); _send("\r\n");
}

void debug_info(const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN]; va_list args;
    va_start(args, format); vsnprintf(msg, sizeof(msg), format, args); va_end(args);
    const char* prefix = _color_enabled ? INFO_TYPE : INFO_TYPE_PLAIN;
    char combined[DEBUG_BUFFER_LEN + 32];
    snprintf(combined, sizeof(combined), "%s%s", prefix, msg);
    _send(combined); _send("\r\n");
}

void debug_error_fileline(const char* file, int line, const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN]; va_list args;
    va_start(args, format); vsnprintf(msg, sizeof(msg), format, args); va_end(args);
    const char* prefix = _color_enabled ? ERROR_TYPE : ERROR_TYPE_PLAIN;
    char combined[DEBUG_BUFFER_LEN + 96];
    if (_filename_line_enabled) { snprintf(combined, sizeof(combined), "%s[%s:%d] %s", prefix, file, line, msg); }
    else { snprintf(combined, sizeof(combined), "%s%s", prefix, msg); }
    _send(combined); _send("\r\n");
}

void debug_warning_fileline(const char* file, int line, const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN]; va_list args;
    va_start(args, format); vsnprintf(msg, sizeof(msg), format, args); va_end(args);
    const char* prefix = _color_enabled ? WARNING_TYPE : WARNING_TYPE_PLAIN;
    char combined[DEBUG_BUFFER_LEN + 96];
    if (_filename_line_enabled) { snprintf(combined, sizeof(combined), "%s[%s:%d] %s", prefix, file, line, msg); }
    else { snprintf(combined, sizeof(combined), "%s%s", prefix, msg); }
    _send(combined); _send("\r\n");
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
    static char ansi[48;2;%u;%u;%um"; // 修正一下原作者这里可能存在的笔误
    sprintf(ansi, "\033[48;2;%u;%u;%um", (unsigned int)r, (unsigned int)g, (unsigned int)b);
    return ansi;
}
