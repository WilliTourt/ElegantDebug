/*******************************************************************************
 * @file    ElegantDebug.cpp
 * @version 1.6
 * @brief   C++ implementation for ANSI-colored debug logging — STM32 HAL,
 *          Renesas RA FSP, TI MSPM0 DL.
 *
 * Implements the `ElegantDebug` class declared in `ElegantDebug.h`.
 * Supports STM32Cube HAL UART / USB-CDC, Renesas RA FSP SCI UART,
 * and TI MSPM0 DL UART.
 * Optional timestamps (via `_getTick()`) and ANSI color
 * prefixes are supported for terminals that accept escape sequences.
 *
 * Usage: Construct `ElegantDebug` with a `UART_HandleTypeDef*` (STM32),
 * `uart_instance_t const*` (RA), or `UART_Regs*` (TI MSPM0), then call
 * `log()`, `info()`, `error()`, etc. See README for examples and integration notes.
 *
 * @author:    WilliTourt <willitourt@foxmail.com>
 * @date:      2026-08-09
 * 
 * @changelog:
 * - (See header file)
 ******************************************************************************/

#include "ElegantDebug.h"



#if (DEBUG_PLATFORM_RA || DEBUG_PLATFORM_TI)
volatile uint32_t _debug_tick_ms = 0;
#endif

#if DEBUG_PLATFORM_RA
volatile bool ElegantDebug::_txDone = true;
#endif

#if __cplusplus < 202002L

    #if DEBUG_PLATFORM_STM32
    ElegantDebug::ElegantDebug(bool enable_timestamp, bool enable_color) :
        _timestamp_enabled(enable_timestamp), _color_enabled(enable_color) {}

    ElegantDebug::ElegantDebug(UART_HandleTypeDef *huart, bool enable_timestamp, bool enable_color) :
        _huart(huart), _timestamp_enabled(enable_timestamp), _color_enabled(enable_color) {}
    #elif DEBUG_PLATFORM_RA
    ElegantDebug::ElegantDebug(uart_instance_t const *uart, bool enable_timestamp, bool enable_color) :
        _uart(uart), _timestamp_enabled(enable_timestamp), _color_enabled(enable_color) {}
    #elif DEBUG_PLATFORM_TI
    ElegantDebug::ElegantDebug(UART_Regs *uart_inst, bool enable_timestamp, bool enable_color) :
        _uart_inst(uart_inst), _timestamp_enabled(enable_timestamp), _color_enabled(enable_color) {}
    #endif

#else

    #if DEBUG_PLATFORM_STM32
    ElegantDebug::ElegantDebug(bool enable_timestamp, bool enable_color,
                               bool enable_filename_line) :
                               _timestamp_enabled(enable_timestamp),
                               _color_enabled(enable_color),
                               _filename_line_enabled(enable_filename_line) {}

    ElegantDebug::ElegantDebug(UART_HandleTypeDef *huart, bool enable_timestamp,
                               bool enable_color, bool enable_filename_line) :
                               _huart(huart),
                               _timestamp_enabled(enable_timestamp),
                               _color_enabled(enable_color),
                               _filename_line_enabled(enable_filename_line) {}
    #elif DEBUG_PLATFORM_RA
    ElegantDebug::ElegantDebug(uart_instance_t const *uart, bool enable_timestamp,
                               bool enable_color, bool enable_filename_line) :
                               _uart(uart),
                              _timestamp_enabled(enable_timestamp),
                              _color_enabled(enable_color),
                              _filename_line_enabled(enable_filename_line) {}
    #elif DEBUG_PLATFORM_TI
    ElegantDebug::ElegantDebug(UART_Regs *uart_inst, bool enable_timestamp,
                               bool enable_color, bool enable_filename_line) :
                               _uart_inst(uart_inst),
                               _timestamp_enabled(enable_timestamp),
                               _color_enabled(enable_color),
                               _filename_line_enabled(enable_filename_line) {}
    #endif

#endif // __cplusplus < 202002L

#if (DEBUG_PLATFORM_STM32 || DEBUG_PLATFORM_TI)
ElegantDebug::~ElegantDebug() {
    // STM32: UART lifecycle managed by CubeMX-generated code
    // MSPM0: UART is configured by sysconfig, no explicit close needed
}
#endif

#if DEBUG_PLATFORM_RA
ElegantDebug::~ElegantDebug() {
    if (_uart != nullptr) {
        #ifdef R_SCI_UART_H
        R_SCI_UART_Close(const_cast<uart_instance_t*>(_uart));
        #else
        R_SCI_B_UART_Close(const_cast<uart_instance_t*>(_uart));
        #endif
    }
}
#endif

void ElegantDebug::_send(const char* text) {
    char out[DEBUG_BUFFER_LEN * 2];
    size_t pos = 0;
    if (_timestamp_enabled) { // Prefix timestamp [hh:mm:ss.mmm] using _getTick()
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

    // append text safely
    strncpy(out + pos, text, sizeof(out) - pos - 1);
    out[sizeof(out) - 1] = '\0';

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
        R_SCI_UART_Write(const_cast<uart_instance_t*>(_uart)->p_ctrl,
                         (uint8_t*)out,
                         (uint32_t)strlen(out));
        #else
        R_SCI_B_UART_Write(const_cast<uart_instance_t*>(_uart)->p_ctrl,
                           (uint8_t*)out,
                           (uint32_t)strlen(out));
        #endif
    #elif DEBUG_PLATFORM_TI
        for (size_t i = 0; out[i] != '\0'; i++) {
            DL_UART_transmitDataBlocking(_uart_inst, (uint8_t)out[i]);
        }
    #endif
}

uint32_t ElegantDebug::_getTick() {
    #if DEBUG_PLATFORM_STM32
    return HAL_GetTick();
    #elif (DEBUG_PLATFORM_RA || DEBUG_PLATFORM_TI)
    return _debug_tick_ms;
    #endif
}


void ElegantDebug::log(const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);
    _send(msg);
}

// #if __cplusplus >= 202002L
// void ElegantDebug::logWithType(const char* type, const char* format, std::source_location loc, ...) {
//     char msg[DEBUG_BUFFER_LEN];
//     va_list args;
//     va_start(args, loc);
//     vsnprintf(msg, sizeof(msg), format, args);
//     va_end(args);

//     char combined[DEBUG_BUFFER_LEN + 96];
//     // if (_filename_line_enabled) {
//     //     snprintf(combined, sizeof(combined), "\033[1m[%s]\033[0m [%s:%ld] %s", type, loc.file_name(), loc.line(), msg);
//     // } else {
//         snprintf(combined, sizeof(combined), "\033[1m[%s]\033[0m %s", type, msg);
//     // }
//     _send(combined);
// }
// #else
void ElegantDebug::logWithType(const char* type, const char* style, const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

    char combined[DEBUG_BUFFER_LEN + 32];
    snprintf(combined, sizeof(combined), "\033[1m%s[%s]\033[0m %s", style, type, msg);
    _send(combined);
}
// #endif

#if __cplusplus >= 202002L
void ElegantDebug::error(const char* format, std::source_location loc, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, loc);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

    const char* prefix = _color_enabled ? ERROR_TYPE : ERROR_TYPE_PLAIN;
    char combined[DEBUG_BUFFER_LEN + 96];

    if (_filename_line_enabled) {
        snprintf(combined, sizeof(combined), "%s[%s:%ld] %s", prefix, loc.file_name(), loc.line(), msg);
    } else {
        snprintf(combined, sizeof(combined), "%s%s", prefix, msg);
    }
    _send(combined);
}
#else
void ElegantDebug::error(const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

    const char* prefix = _color_enabled ? ERROR_TYPE : ERROR_TYPE_PLAIN;
    char combined[DEBUG_BUFFER_LEN + 32];

    snprintf(combined, sizeof(combined), "%s%s", prefix, msg);
    _send(combined);
}
#endif

#if __cplusplus >= 202002L
void ElegantDebug::warning(const char* format, std::source_location loc, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, loc);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

    const char* prefix = _color_enabled ? WARNING_TYPE : WARNING_TYPE_PLAIN;
    char combined[DEBUG_BUFFER_LEN + 96];

    if (_filename_line_enabled) {
        snprintf(combined, sizeof(combined), "%s[%s:%ld] %s", prefix, loc.file_name(), loc.line(), msg);
    } else {
        snprintf(combined, sizeof(combined), "%s%s", prefix, msg);
    }

    _send(combined);
}
#else
void ElegantDebug::warning(const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

    const char* prefix = _color_enabled ? WARNING_TYPE : WARNING_TYPE_PLAIN;
    char combined[DEBUG_BUFFER_LEN + 32];

    snprintf(combined, sizeof(combined), "%s%s", prefix, msg);
    _send(combined);
}
#endif

void ElegantDebug::ok(const char* format, ...) {
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

void ElegantDebug::success(const char* format, ...) {
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

void ElegantDebug::info(const char* format, ...) {
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


// runtime helpers for 24‑bit ANSI colours; return a pointer to a static buffer
const char* ElegantDebug::customTextColor(uint8_t r, uint8_t g, uint8_t b) {
    static char ansi[24];
    snprintf(ansi, sizeof(ansi), "\033[38;2;%u;%u;%um", (unsigned)r, (unsigned)g, (unsigned)b);
    return ansi;
}

const char* ElegantDebug::customBgColor(uint8_t r, uint8_t g, uint8_t b) {
    static char ansi[24];
    snprintf(ansi, sizeof(ansi), "\033[48;2;%u;%u;%um", (unsigned)r, (unsigned)g, (unsigned)b);
    return ansi;
}



/* Deprecated functions, originally for macro-based logging *************************************

void ElegantDebug::_log(const char* file, int line, const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);
    char combined[DEBUG_BUFFER_LEN + 64];
    if (_filename_line_enabled) {
        snprintf(combined, sizeof(combined), "[%s:%d] %s", file, line, msg);
    } else {
        strncpy(combined, msg, sizeof(combined) - 1);
        combined[sizeof(combined) - 1] = '\0';
    }
    _send(combined);
}

void ElegantDebug::_logWithType(const char* file, int line, const char* type, const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);
    char combined[DEBUG_BUFFER_LEN + 96];
    if (_filename_line_enabled) {
        snprintf(combined, sizeof(combined), "[ %s ] [%s:%d] %s", file, line, type, msg);
    } else {
        snprintf(combined, sizeof(combined), "[ %s ] %s", type, msg);
    }
    _send(combined);
}

void ElegantDebug::_error(const char* file, int line, const char* format, ...) {
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

void ElegantDebug::_warning(const char* file, int line, const char* format, ...) {
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

void ElegantDebug::_ok(const char* file, int line, const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);
    const char* prefix = _color_enabled ? OK_TYPE : OK_TYPE_PLAIN;
    char combined[DEBUG_BUFFER_LEN + 96];
    if (_filename_line_enabled) {
        snprintf(combined, sizeof(combined), "%s[%s:%d] %s", prefix, file, line, msg);
    } else {
        snprintf(combined, sizeof(combined), "%s%s", prefix, msg);
    }
    _send(combined);
}

void ElegantDebug::_success(const char* file, int line, const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);
    const char* prefix = _color_enabled ? SUCCESS_TYPE : SUCCESS_TYPE_PLAIN;
    char combined[DEBUG_BUFFER_LEN + 96];
    if (_filename_line_enabled) {
        snprintf(combined, sizeof(combined), "%s[%s:%d] %s", prefix, file, line, msg);
    } else {
        snprintf(combined, sizeof(combined), "%s%s", prefix, msg);
    }
    _send(combined);
}

void ElegantDebug::_info(const char* file, int line, const char* format, ...) {
    char msg[DEBUG_BUFFER_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);
    const char* prefix = _color_enabled ? INFO_TYPE : INFO_TYPE_PLAIN;
    char combined[DEBUG_BUFFER_LEN + 96];
    if (_filename_line_enabled) {
        snprintf(combined, sizeof(combined), "%s[%s:%d] %s", prefix, file, line, msg);
    } else {
        snprintf(combined, sizeof(combined), "%s%s", prefix, msg);
    }
    _send(combined);
}
*********************************************************************************************/