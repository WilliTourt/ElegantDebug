#ifndef __ELEGANT_DEBUG_H
#define __ELEGANT_DEBUG_H

#include <REGX52.H>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef unsigned char bool;
typedef unsigned char uint8_t;
typedef unsigned long uint32_t;

#define true 1
#define false 0

#define DEBUG_BUFFER_LEN 32

#define ERROR_TYPE          "\033[91m\033[1m[ERROR]\033[0m "
#define WARNING_TYPE        "\033[93m\033[1m[WARNING]\033[0m "
#define INFO_TYPE           "\033[94m\033[1m[INFO]\033[0m "
#define OK_TYPE             "\033[92m\033[1m[OK]\033[0m "
#define SUCCESS_TYPE        "\033[92m\033[1m[SUCCESS]\033[0m "

#define ERROR_TYPE_PLAIN    "[ERROR] "
#define WARNING_TYPE_PLAIN  "[WARNING] "
#define INFO_TYPE_PLAIN     "[INFO] "
#define OK_TYPE_PLAIN       "[OK] "
#define SUCCESS_TYPE_PLAIN  "[SUCCESS] "

extern volatile unsigned long G_ms;
char putchar(char c);

void debug_init(bool enable_timestamp, bool enable_color, bool enable_filename_line);

void debug_log(const char* format, ...) reentrant;
void debug_logWithType(const char* type, const char* style, const char* format, ...) reentrant;
void debug_ok(const char* format, ...) reentrant;
void debug_success(const char* format, ...) reentrant;
void debug_info(const char* format, ...) reentrant;
void debug_error_fileline(const char* file, int line, const char* format, ...) reentrant;
void debug_warning_fileline(const char* file, int line, const char* format, ...) reentrant;

void debug_setTimestampEnabled(bool enabled);
void debug_setColorEnabled(bool enabled);
void debug_setFilenameLineEnabled(bool enabled);

#endif
