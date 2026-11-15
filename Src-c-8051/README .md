# ElegantDebug for 8051 (STC89C52RC)

> **Language/语言**: [English](README.md) | [简体中文](README-zh-CN.md)

## Introduction

- A lightweight UART debug library for 8051 (STC89C52RC), built with Keil C51, providing formatted log output with ANSI colors.
- Features:
  - Formatted logs with common integer, character, string, hex and float specifiers
  - Optional timestamp in `[HH:MM:SS.mmm]` format
  - Optional filename and line number for ERROR / WARNING
  - Optional ANSI colors, including fully custom 24-bit true color
  - Convenient level prefixes (ERROR / WARNING / INFO / OK / SUCCESS)
  - Blocking transmission by writing `SBUF` directly — no `printf`, `vprintf` or `sprintf`, no long string concatenation
  - All log functions go through a single `_send()` entry, which makes control and trimming easy

## File Structure

- `ElegantDebug.c`: library implementation
- `ElegantDebug.h`: color macros, function declarations and format notes
- `main1.c`, `main2.c`: self-test examples, compiled in two rounds to cover every function

## Quick Start

### 1. Add the files

Add `ElegantDebug.c` and `ElegantDebug.h` to your Keil project and make sure `ElegantDebug.c` is compiled and linked.

- Keil MDK / C51: right-click in the Project window → Add Existing Files
- EIDE (VS Code): drag them into the project, or right-click the project → add source files

### 2. Initialize UART and timer

```c
#include <REGX52.H>
#include "ElegantDebug.h"
#include "delay_xms.h"

/* Timer0 interrupt: fires every 1 ms and drives the timestamp */
void Timer0_Isr(void) interrupt 1
{
    TL0 = 0x66;
    TH0 = 0xFC;
    debug_tick();      /* only increments the counter, never log inside an ISR */
}

void main(void)
{
    Debug_init2D(0xFA);   /* 9600 baud @ 11.0592 MHz with baud rate doubled */
    Timer0_Init();        /* 1 ms timer */
    debug_init(1, 1, 0);  /* timestamp on, color on, filename/line off */

    debug_info("Hello from 8051!\r\n");

    while (1)
    {
    }
}
```

### 3. Project settings

**The memory model must be set to Large**, otherwise the DATA segment overflows:

```
Options for Target → Target → Memory Model → Large: variables in XDATA
```

In EIDE:

```
Project Settings → C51 → Memory Model → Large: variables in XDATA
```

Reason: 8051 DATA is only 128 bytes, while this library needs more space for its formatting code and string constants. The STC89C52RC provides 256 bytes of on-chip XDATA, which is enough.

**It is also recommended to limit the variadic argument area.** In C51 → Misc Controls, add:

```
MAXARGS(4)
```

No call in this library passes more than 4 bytes of arguments, so this further reduces RAM usage.

## Shared Example

```c
debug_log("Hello world!\r\n");
debug_info("System started. Free memory: %d bytes\r\n", free_mem);
debug_warning_fileline(__FILE__, __LINE__, "Low battery: %d %%\r\n", percent);
debug_error_fileline(__FILE__, __LINE__, "Sensor failed: code = %d\r\n", err_code);
debug_ok("Operation finished in %d ms\r\n", elapsed_ms);
debug_success("Configuration saved\r\n");
debug_logWithType("CUSTOM", COLOR_GREEN, "custom message: %s\r\n", "ok");

/* Colors and text styles */
debug_log("%sI am GREEN!%s\r\n", COLOR_GREEN, CLR);
debug_log("%sI have a colored background!%s\r\n", BG_RED, CLR);
debug_log("%sI am underlined and %spartly colored!%s%s\r\n",
          UNDERLINE, COLOR_DARK_MAGENTA, CLR, CLR);

/* Enable filename and line number for ERROR / WARNING */
debug_setFilenameLineEnabled(1);
```

> Note: log functions never append a newline. Write `\r\n` yourself inside the format string.

## API

### Initialization

- `void debug_init(bit enable_timestamp, bit enable_color, bit enable_filename_line);`
  - Sets the initial state of the three runtime switches. Must be called before using any log function.
- `void Debug_init2D(unsigned char baud);`
  - Initializes UART in mode 1, 8-bit UART, baud rate doubled, blocking transmission, no UART interrupt.
  - The parameter is the reload value; the actual baud rate is twice the value it encodes. Use `0xFA` for 9600 @ 11.0592 MHz.
- `void Timer0_Init(void);`
  - Initializes Timer0 for 1 ms @ 11.0592 MHz.
  - The interrupt service routine must be written by the user and call `debug_tick()`.

### Timestamp

- `extern volatile unsigned long _debug_tick_ms;`
- `#define debug_tick() (_debug_tick_ms++)`
  - Call it inside the 1 ms timer interrupt to feed the millisecond counter.

### Low-level transmission

- `void Uart_SendByte(unsigned char dat);`
  - Blocking single-byte transmission; waits for TI then clears it.
- `void Uart_SendUInt(unsigned int Num);`
  - Sends an unsigned 16-bit integer, 0 ~ 65535.
- `void Uart_SendInt(int Num);`
  - Sends a signed 16-bit integer, -32768 ~ 32767.
- `void Uart_SendULong(unsigned long Num);`
  - Sends an unsigned 32-bit integer, 0 ~ 4294967295.
- `void Uart_SendLong(long Num);`
  - Sends a signed 32-bit integer.
- `void Uart_SendString(const char *str);`
  - Sends a null-terminated string.

### Log functions

- `void debug_log(const char *format, ...);`
  - Raw formatted output without a level prefix.
- `void debug_logWithType(const char *type, const char *style, const char *format, ...);`
  - Custom tag and color, e.g. `debug_logWithType("SENSOR", COLOR_GREEN, "ok\r\n");`
- `void debug_error_fileline(const char *file, int line, const char *format, ...);`
  - Red `[ERROR]`, supports filename and line number.
- `void debug_warning_fileline(const char *file, int line, const char *format, ...);`
  - Yellow `[WARNING]`, supports filename and line number.
- `void debug_ok(const char *format, ...);`
  - Green `[OK]`, no filename or line number.
- `void debug_success(const char *format, ...);`
  - Green `[SUCCESS]`, no filename or line number.
- `void debug_info(const char *format, ...);`
  - Blue `[INFO]`, no filename or line number.

### Runtime switches

- `void debug_setTimestampEnabled(bit enabled);`
- `void debug_setColorEnabled(bit enabled);`
- `void debug_setFilenameLineEnabled(bit enabled);`

### Custom colors

- `const char *customTextColor(unsigned char r, unsigned char g, unsigned char b);`
- `const char *customBgColor(unsigned char r, unsigned char g, unsigned char b);`
  - Return an internal static ANSI string that can be embedded with `%s`.
  - Note that the next call overwrites the same buffer.

### Format support

| Specifier | Description |
|---|---|
| `%d` `%i` | Signed 16-bit integer |
| `%u` | Unsigned 16-bit integer |
| `%ld` | Signed 32-bit integer |
| `%lu` | Unsigned 32-bit integer |
| `%c` | Single character |
| `%s` | String |
| `%x` `%X` | 16-bit hex, lower / upper case |
| `%lx` `%lX` | 32-bit hex, lower / upper case |
| `%f` | Float, always 6 decimal places |
| `%%` | A literal percent sign |

Width, precision, `%o`, `%p`, `%e`, `%g` and the `+` flag are not supported.

### Important notes

Keil C51 does not support variadic macros, so there are **no** `debug_error(...)` and `debug_warning(...)` macros. Call the fileline variants directly:

```c
debug_error_fileline(__FILE__, __LINE__, "error=%d\r\n", value);
debug_warning_fileline(__FILE__, __LINE__, "warning=%d\r\n", value);
```

Only ERROR / WARNING show filename and line number; INFO / OK / SUCCESS do not, matching the original library.

### About ANSI escape codes

All color and style macros are ANSI escape sequences. If your terminal does not support them, call `debug_setColorEnabled(0)` to send plain text only.

To color a piece of your own output, wrap the color/style macro and its reset macro with a pair of `%s`:

```c
debug_log("Hello World! %sI am RED!%s %s%sI am italic and GREEN!%s Now cleared italic but still GREEN.%s\r\n",
          COLOR_RED, CLR_TEXT_COLOR, ITALIC, COLOR_GREEN, CLR_ITALIC, CLR);
```

For 24-bit custom colors use `customTextColor(r, g, b)` and `customBgColor(r, g, b)`, where `r`, `g`, `b` are 0 ~ 255. The macros `COLOR_CUSTOM(r,g,b)` and `BG_COLOR_CUSTOM(r,g,b)` are also available.

For more information about ANSI escape codes see: `https://handwiki.org/wiki/ANSI_escape_code`

## Troubleshooting

- No output or garbage on the serial port:
  - Make sure UART is initialized correctly, **especially the baud rate**.
  - `Debug_init2D(0xFA)` is for 9600 @ 11.0592 MHz; recompute the reload value for a 12 MHz crystal.
- Timestamp stays at `[00:00:00.000]`:
  - Make sure Timer0 is initialized: `TR0`, `ET0` and `EA` must all be 1.
  - Make sure `debug_tick()` is called inside the Timer0 interrupt.
- Linker reports `L107: ADDRESS SPACE OVERFLOW`:
  - Set Memory Model to `Large: variables in XDATA`.
  - Or add `MAXARGS(4)` in C51 → Misc Controls.
- Colors appear as `.[91m`-style characters:
  - Your serial terminal does not support ANSI. Call `debug_setColorEnabled(0)` or use an ANSI-capable terminal.
- The timestamp is split across two lines:
  - Disable "receive timestamp" and "receive packet framing" in your terminal, or increase the framing timeout.
- The program hangs in `while (!TI);`:
  - Make sure UART is initialized and `_uart_ready` is 1. When UART is not initialized, `Uart_SendByte()` returns immediately instead of waiting forever.
- Chinese characters are garbled:
  - Only ASCII is sent over UART. Save source files with Chinese comments in ANSI/GBK encoding.

## Changelog

### v1.0 (2026-11-15)

- **Initial release**: UART debug library for 8051 (STC89C52RC)
- **Features**: formatted logs, timestamp, ANSI colors, level prefixes, filename/line number, 24-bit custom colors
- **Implementation**: blocking transmission via `SBUF`, no printf-family functions, single `_send()` entry
- **Toolchain**: Keil C51, EIDE (VS Code)
