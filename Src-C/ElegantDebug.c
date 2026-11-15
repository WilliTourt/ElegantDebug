#include "ElegantDebug.h"

static bool _timestamp_enabled = true;
static bool _color_enabled = true;
static bool _filename_line_enabled = false;

// 公用缓冲
static xdata char _debug_msg[DEBUG_BUFFER_LEN];
static xdata char _debug_combined[DEBUG_BUFFER_LEN + 32];
static xdata char _debug_out[DEBUG_BUFFER_LEN * 2];



void debug_init(bool enable_timestamp, bool enable_color, bool enable_filename_line)
{
    _timestamp_enabled = enable_timestamp;
    _color_enabled = enable_color;
    _filename_line_enabled = enable_filename_line;
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

static void _send(const char* text)
{
    unsigned int pos = 0;
    unsigned int remain;
    unsigned int i;

    if (text == NULL) return;

    _debug_out[0] = '\0';

    if (_timestamp_enabled)
		{
        unsigned long ms = G_ms;
        unsigned int s = (unsigned int)(ms / 1000UL);
        unsigned int hours = s / 3600U;
        unsigned int minutes = (s % 3600U) / 60U;
        unsigned int seconds = s % 60U;
        
        sprintf(_debug_out, "[%02u:%02u:%02u.%03u] ",
                hours, minutes, seconds, (unsigned int)(ms % 1000UL));
        pos = strlen(_debug_out);
    }

    remain = sizeof(_debug_out) - pos;
    if (remain > 1)
		{
        strncpy(_debug_out + pos, text, remain - 1);
        _debug_out[sizeof(_debug_out) - 1] = '\0';
    }

    for (i = 0; _debug_out[i] != '\0'; i++)
		{
        putchar(_debug_out[i]);
    }
}

void debug_log(const char* format, ...) reentrant
{
    va_list args;
    va_start(args, format);
    vsprintf(_debug_msg, format, args);
    va_end(args);
    _send(_debug_msg);
}

void debug_logWithType(const char* type, const char* style, const char* format, ...) reentrant
{
    va_list args;
    va_start(args, format);
    vsprintf(_debug_msg, format, args);
    va_end(args);

    sprintf(_debug_combined, "\033[1m%s[%s]\033[0m %s", style, type, _debug_msg);
    _send(_debug_combined);
}

void debug_error_fileline(const char* file, int line, const char* format, ...) reentrant
{
    const char* prefix;
    va_list args;
    
    va_start(args, format);
    vsprintf(_debug_msg, format, args);
    va_end(args);

    prefix = _color_enabled ? ERROR_TYPE : ERROR_TYPE_PLAIN;
    if (_filename_line_enabled)
		{
        sprintf(_debug_combined, "%s[%s:%d] %s", prefix, file, line, _debug_msg);
    }
		else
		{
        sprintf(_debug_combined, "%s%s", prefix, _debug_msg);
    }
    _send(_debug_combined);
}

void debug_warning_fileline(const char* file, int line, const char* format, ...) reentrant
{
    const char* prefix;
    va_list args;
    
    va_start(args, format);
    vsprintf(_debug_msg, format, args);
    va_end(args);

    prefix = _color_enabled ? WARNING_TYPE : WARNING_TYPE_PLAIN;
    if (_filename_line_enabled)
		{
        sprintf(_debug_combined, "%s[%s:%d] %s", prefix, file, line, _debug_msg);
    }
		else
		{
        sprintf(_debug_combined, "%s%s", prefix, _debug_msg);
    }
    _send(_debug_combined);
}

void debug_ok(const char* format, ...) reentrant
{
    const char* prefix;
    va_list args;
    
    va_start(args, format);
    vsprintf(_debug_msg, format, args);
    va_end(args);

    prefix = _color_enabled ? OK_TYPE : OK_TYPE_PLAIN;
    sprintf(_debug_combined, "%s%s", prefix, _debug_msg);
    _send(_debug_combined);
}

void debug_success(const char* format, ...) reentrant
{
    const char* prefix;
    va_list args;
    
    va_start(args, format);
    vsprintf(_debug_msg, format, args);
    va_end(args);

    prefix = _color_enabled ? SUCCESS_TYPE : SUCCESS_TYPE_PLAIN;
    sprintf(_debug_combined, "%s%s", prefix, _debug_msg);
    _send(_debug_combined);
}

void debug_info(const char* format, ...) reentrant
{
    const char* prefix;
    va_list args;
    
    va_start(args, format);
    vsprintf(_debug_msg, format, args);
    va_end(args);

    prefix = _color_enabled ? INFO_TYPE : INFO_TYPE_PLAIN;
    sprintf(_debug_combined, "%s%s", prefix, _debug_msg);
    _send(_debug_combined);
}
