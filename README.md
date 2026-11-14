# Elegant Debug for STM32, Renesas-RA, TI-MSPM0 & 8051 MCUs

> **Language/语言**: [English](README.md) | [简体中文](README-zh-CN.md)

## Introduction

- A lightweight serial/USB-CDC debug library suitable for both C and C++ projects on STM32 using the HAL drivers, Renesas RA Family using FSP, TI MSPM0 using DL (Driver Library), or 8051 cores using Keil C51.
- Features:
  - Formatted output
  - Optional timestamp
  - Optional filename and line number display for ERROR/WARNING messages
  - Optional ANSI color output, including fully customizable 24-bit colors
  - Convenient type prefixes (ERROR/WARNING/INFO/OK/SUCCESS)

![Example](example.png)

Supports showing filename and line number for ERROR/WARNING messages:
![Showing Filename and Line](example_fileline.png)

Supports customized 24-bit colors:
![Custom Color](example_customcolor.png)

## Repository layout

- C implementation is in `Src-C/`
- C++ implementation is in `Src-CPP/`
- The 8051 (Keil C51) port lives in the **C implementation only**: C51 is a C90 compiler and no C++ toolchain for 8051 is assumed.

## Quick Start

- Depends on STM32Cube HAL / Renesas RA FSP / TI MSPM0 DL, At least one UART enabled, or USB-CDC enabled (USB-CDC are currently only available on STM32. Enable USB_DEVICE middleware in MX and set to CDC class). The 8051 port needs no vendor HAL at all — it writes through a function you provide.
- The output buffer length is controlled by the `DEBUG_BUFFER_LEN` macro (default 256; 32 on 8051).
- Output method (UART/USB) is controlled by the `USB_AS_DEBUG_PORT` macro (default 0 for UART; set to 1 for USB-CDC).
- ⚠️ **Platform must be selected before use**: uncomment either `USE_STM32_HAL`, `USE_RA_FSP`, `USE_TI_MSPM0_DL` or `USE_8051_C51` in the header file before including it.

### STM32 HAL Platform

1. Add `Src-C/ElegantDebug.h` and `Src-C/ElegantDebug.c` to your project (make sure `ElegantDebug.c` is compiled and linked).
   - Makefile/CMake: add `.../Src-C/ElegantDebug.c` to your `SRCS` / `target_sources`.
   - Keil/MDK / IAR / CubeIDE: in the Project Explorer, Right-click -> Add Existing Files.

2. Include the header and initialize (example using STM32 `huart1`, C API shown; C++ usage is identical):

```c
#define USE_STM32_HAL
#include "ElegantDebug.h"

// Initialize at an appropriate place (e.g. in main())
// When USB-CDC is configured as the debug port (compile-time macro
// `USB_AS_DEBUG_PORT` set to 1), the `huart` argument can be NULL and will be ignored by the library.
debug_init(&huart1, true /* enable timestamp */, true /* enable color */, false /* enable filename line */);
```

### Renesas RA FSP Platform

1. Add `Src-C/ElegantDebug.h` and `Src-C/ElegantDebug.c` to your Renesas project.

2. Configure an **SCI UART** peripheral in RASC (any SCI channel).
![Example of RA UART Config](example_ra_uart_config.png)

3. Initialize (C++ version shown, using `g_uart0` as example). **Important:** RA FSP requires calling `ElegantDebug::onTxComplete(p_args)` (C++) or `debug_onTxComplete(p_args)` (C) inside the UART interrupt callback to detect TX completion. Failing to call this function will cause the library to hang on subsequent transmissions.

```cpp
#include "ElegantDebug.h"

// Global debug instance
ElegantDebug dbg(&g_uart0, true, true, false);

void cpp_main() {
    // ...

    dbg.info("Hello from Renesas RA!");
    dbg.ok("ElegantDebug logging");

    // ...
}

extern "C" void uart0_callback(uart_callback_args_t *p_args) {
    ElegantDebug::onTxComplete(p_args);
}
```

4. **Timestamp feeding**: Since Renesas does not have an equivalent of `GetTick()` like STM32, the library needs you to maintain a **1 ms** counter. Call `debug_tick()` (C) / `ElegantDebug::tick()` (C++) from your timer ISR to increment this counter. For example, configure an AGT:
![Example of RA Tick Config](example_ra_tick_config.png)

```c
// In your 1 ms timer ISR:
void vSysTick(void) {
    debug_tick(); // Feed ElegantDebug timestamp. C++ uses `ElegantDebug::tick()` instead.
}
```

### TI MSPM0 DL Platform

1. Add `Src-C/ElegantDebug.h` and `Src-C/ElegantDebug.c` to your CCS Theia project.

2. Configure a **UART** peripheral in SysConfig and make sure the instance is enabled.

3. Initialize and use (C version shown):

```c
#define USE_TI_MSPM0
#include "ElegantDebug.h"

debug_init(UART0_INST, true, true, false);
debug_info("Hello from TI MSPM0!");
```

4. **Timestamp feeding**: Same as the RA platform, create a **1 ms** counter or call `debug_tick()` (C) / `ElegantDebug::tick()` (C++) from the SysTick ISR to update the timestamp.

### 8051 (Keil C51) Platform

The C implementation also runs on 8051 cores (AT89C52/AT89S52, STC89C52, STC15, N76E003, ...). No vendor HAL is needed: the library writes **one byte at a time** through a function you provide, so it works with any UART driver, a bit-banged pin or a software serial port.

1. Add `Src-C/ElegantDebug.h` and `Src-C/ElegantDebug.c` to your Keil project (Project window → right-click → *Add Existing Files*; make sure `ElegantDebug.c` is compiled and linked).

2. Define `USE_8051_C51` before including the header (or uncomment it in the header itself so that both `main.c` and `ElegantDebug.c` see it).

3. Initialise with a byte output function and log:

```c
#include <REG52.H>

#define USE_8051_C51
#include "ElegantDebug.h"

/* Called once per character; must block until the byte is queued. */
static void uart_putc(char c) {
    SBUF = c;
    while (!TI);
    TI = 0;
}

void main(void) {
    uart_init();                     /* your own UART setup, 9600 8N1 for example */

    debug_init(uart_putc, true /* timestamp */, true /* color */, false /* filename:line */);

    debug_log("Hello from 8051!");
    debug_info("Free memory check: %d", 42);
    debug_ok("Elapsed: %u ms", 1234);

    while (1);
}
```

4. **Timestamp feeding**: same as the RA / MSPM0 platforms, call `debug_tick()` from a 1 ms timer ISR:

```c
void timer0_isr(void) interrupt 1 {
    TH0 = 0xFC; TL0 = 0x18;          /* reload for 1 ms */
    debug_tick();                    /* feeds the timestamp counter */
}
```

5. **RAM and flash budget.** Keil C51 keeps a fixed parameter block in DATA for every non-reentrant variadic function, which an 8051 cannot afford, so on this platform the whole API is compiled `reentrant` (parameters live on the stack) and all log functions share a single message buffer. Measured with Keil C51 V9.54, small model, on the `AT89C52` demo of this port (the values include the demo application itself):

| Configuration | DATA | XDATA | CODE |
| --- | --- | --- | --- |
| default (`DEBUG_BUFFER_LEN 32`, buffers in DATA) | 106 B | – | 9521 B |
| `DEBUG_8051_BUF_MEM idata`, `DEBUG_BUFFER_LEN 64` | 30 B (+108 B IDATA) | – | 9521 B |
| `DEBUG_8051_BUF_MEM xdata`, `DEBUG_BUFFER_LEN 128` | 30 B | 172 B | 9521 B |
| `DEBUG_8051_ENABLE_FLOAT 0` (adds to the rows above) | – | – | 7378 B |

- `DEBUG_BUFFER_LEN` — size of the message buffer; 32 by default so that a plain 8051 (128 bytes of RAM) still links.
- `DEBUG_8051_BUF_MEM` — memory space of the internal buffers. Leave it empty for DATA (any 8051). On an 8052 / AT89S52 / STC89C52 use `idata` to move them into the upper 128 bytes, and on a part with external RAM use `xdata`; both free the DATA area for your own variables and allow a much larger `DEBUG_BUFFER_LEN`.
- `DEBUG_8051_ENABLE_FLOAT` — set to `0` to drop `%f` (saves ~2 KB of code space, useful on 8 KB flash parts).

Like the RA / TI MSPM0 ports, the 8051 port needs the **Keil C51** compiler; the `Src-CPP/` implementation is not available for 8051.

#### 8051 / C90 notes

- Keil C51 is a **C90** compiler: it has no `__VA_ARGS__`, so `debug_error(fmt, ...)` and `debug_warning(fmt, ...)` cannot insert `__FILE__` / `__LINE__` automatically. On 8051 they are mapped to location-less variants (`debug_error_plain()` / `debug_warning_plain()`); call `debug_error_fileline(__FILE__, __LINE__, ...)` or `debug_warning_fileline(__FILE__, __LINE__, ...)` when you want the location shown.
- C51 provides neither `snprintf()` nor `vsnprintf()`, so a small **bounded** formatter is built into `ElegantDebug.c` and used through those names. It supports `%% %c %s %d %i %u %o %x %X %f`, the `-`, `0`, `+`, ` ` and `#` flags, field width, precision and the `l` length modifier, always NUL-terminates the buffer and never writes past it.
- `%f` is converted by that formatter (fixed point, up to 6 decimals); `%e` and `%g` are printed like `%f`, and floats are rounded half away from zero. Unsupported conversions are echoed back unchanged.
- The library is not interrupt safe on any platform; do not log from an ISR while the main loop is logging.

### Shared Examples

```c
debug_log("Hello world!");
debug_info("System started. Free memory: %d bytes", free_mem);
debug_warning("Low battery: %.2f %%", battery_percentage);  // Automatically includes filename and line number (if enabled)
debug_error("Sensor failed: code = %d", err_code);          // Automatically includes filename and line number (if enabled)
debug_ok("Operation finished in %d ms", elapsed_ms);
debug_success("Configuration saved");
debug_logWithType("[CUSTOM]", "custom message: %s", "ok");

/* Colors and styles */
debug_log("%sI am GREEN!%s\n", COLOR_GREEN, CLR);
debug_log("%sI have a colored background!%s\n", BG_RED, CLR);
debug_log("%sI am underlined and %spartly colored!%s%s\n", UNDERLINE, COLOR_DARK_MAGENTA, CLR, CLR);

/* Enable filename and line number display for ERROR / WARNING messages */
debug_setFilenameLineEnabled(true);  // After enabling, error and warning messages will show filename and line number
```

## API

### C API

- `void debug_init(UART_HandleTypeDef *huart, bool enable_timestamp, bool enable_color, bool enable_filename_line);` (STM32)
- `void debug_init(uart_instance_t const *uart, bool enable_timestamp, bool enable_color, bool enable_filename_line);` (Renesas RA)
- `void debug_init(UART_Regs *uart_inst, bool enable_timestamp, bool enable_color, bool enable_filename_line);` (TI MSPM0)
- `void debug_init(debug_putc_t uart_putc, bool enable_timestamp, bool enable_color, bool enable_filename_line);` (8051)
  - Initialize the library. Must be called before other functions. Provide a HAL UART handle / FSP UART instance / DL UART register pointer / byte output function and flags to enable timestamp/color/filename-line display. `debug_putc_t` is `void (*)(char)`; passing `NULL` makes every log call a no-op.
- `static inline void debug_tick(void);` (RA FSP, TI MSPM0 and 8051; on 8051 it is a plain `static` function because C51 has no `inline`)
  - Call from a timer ISR to increment the internal millisecond counter used for timestamps.
- `void debug_onTxComplete(uart_callback_args_t *p_args);` (RA FSP only)
  - RA FSP specific — must be called from the UART TX complete interrupt callback. **Failure to call this will cause the library to hang on subsequent transmissions.**
- `void debug_log(const char* format, ...);`
  - Basic formatted output (no prefix).
- `void debug_logWithType(const char* type, const char* style, const char* format, ...);`
  - Output with a custom type prefix and style (e.g. `"\033[91m"` + `"[ CUSTOM ]"`).
- Convenience helpers (automatically include filename and line number):
  - `void debug_error(const char* format, ...);`
  - `void debug_warning(const char* format, ...);`
  - 8051: C51 has no `__VA_ARGS__`, so these names are mapped to the location-less `debug_error_plain()` / `debug_warning_plain()`. Use `debug_error_fileline(__FILE__, __LINE__, ...)` / `debug_warning_fileline(__FILE__, __LINE__, ...)` to show the location.
- Convenience helpers (no filename/line):
  - `void debug_ok(const char* format, ...);`
  - `void debug_success(const char* format, ...);`
  - `void debug_info(const char* format, ...);`
- Runtime setters:
  - `void debug_setTimestampEnabled(bool enabled);`
  - `void debug_setColorEnabled(bool enabled);`
  - `void debug_setFilenameLineEnabled(bool enabled);`

### C++ API

- `ElegantDebug(UART_HandleTypeDef *huart, bool enable_timestamp = true, bool enable_color = true, bool enable_filename_line = false);` (STM32)
- `ElegantDebug(uart_instance_t const *uart, bool enable_timestamp = true, bool enable_color = true, bool enable_filename_line = false);` (Renesas RA)
- `ElegantDebug(UART_Regs *uart_inst, bool enable_timestamp = true, bool enable_color = true, bool enable_filename_line = false);` (TI MSPM0)
  - Constructor: pass a HAL UART handle / FSP UART instance / DL UART register pointer and flags to enable timestamp/color/filename-line display.
- `static inline void tick(void);` (RA FSP and TI MSPM0 only)
  - Static method, called from a timer ISR to increment the internal millisecond counter used for timestamps.
- `static inline void onTxComplete(uart_callback_args_t *p_args);` (RA FSP only)
  - RA FSP specific — must be called from the UART TX complete interrupt callback. **Failure to call this will cause the library to hang on subsequent transmissions.**
- `void log(const char* format, ...);`
  - Basic formatted output (no prefix).
- `void logWithType(const char* type, const char* style, const char* format, ...);`
  - Output with a custom type prefix and style (e.g. `"\033[91m"` + `"[ CUSTOM ]"`).
- Convenience helpers (C++20 and above automatically include filename and line number):
  - `void error(const char* format, ...);` (C++20: supports `std::source_location`)
  - `void warning(const char* format, ...);` (C++20: supports `std::source_location`)
- Convenience helpers (no filename/line):
  - `void ok(const char* format, ...);`
  - `void success(const char* format, ...);`
  - `void info(const char* format, ...);`
- Runtime setters:
  - `void setTimestampEnabled(bool enabled);`
  - `void setColorEnabled(bool enabled);`
  - `void setFilenameLineEnabled(bool enabled);` (C++20 and above only)

#### Note

- `error` and `warning` methods **cannot use mutable parameters** because they need to obtain the calling location (file name and line number).

```cpp
	dbg.error("error test... %d\n", 12345); // Wrong
	dbg.error("%serror test...\n", BOLD);   // Wrong
	dbg.error("error test... \n");          // Correct

// The first two lines of code report a compilation error due to a parameter
// matching issue when default parameters and variable parameters are mixed.
```

### About ANSI Escape Codes

The color and style macros in the library are ANSI escape codes. If your terminal doesn't support them, you can disable color output at runtime and avoid using the `%s` formatting operations.

To set custom colors and styles for your output strings, wrap the color/style macros and the clear macros with a pair of `%s` placeholders. For example:

```cpp
log("Hello World! %sI am RED!%s %s%sI am italic and GREEN!%s Now i cleared italic style but still GREEN.%s\n", 
COLOR_RED, CLR_TEXT_COLOR, ITALIC, COLOR_GREEN, CLR_ITALIC, CLR);
```

For 24-bit custom colors, use the `COLOR_CUSTOM(r,g,b)` and `BG_COLOR_CUSTOM(r,g,b)` macros, where `r`, `g`, `b` are integers between 0 and 255. These macros now call helper functions internally, so the red/green/blue values can be supplied from variables instead of literal constants.

For more information about ANSI escape codes, refer to: [ANSI escape code - Handwiki](https://handwiki.org/wiki/ANSI_escape_code)

## Troubleshooting

- No output or garbled output on the serial terminal:
  - Verify the UART is correctly initialized (especially baud rate).
  - If colors are enabled but your terminal does not support ANSI, call `debug_setColorEnabled(false)`.
- RA / TI MSPM0 / 8051 platform timestamps not working or showing `[00:00:00.000]`:
  - Ensure you are calling the tick function mentioned above in your 1 ms timer ISR.
- RA platform TX hanging (program stuck in infinite loop):
  - Make sure `onTxComplete()` / `debug_onTxComplete()` is called from your UART TX complete callback.
- Unexpected resets/crashes:
  - Possible buffer overflow: check `DEBUG_BUFFER_LEN` (default 256; 32 on 8051). Increase it in `ElegantDebug.h` if you need longer messages.
- 8051: `*** ERROR L114: SEGMENT DOES NOT FIT, SPACE: DATA` while linking
  - The internal buffers do not fit in the 128 bytes of DATA. Keep `DEBUG_BUFFER_LEN` at 32, or move the buffers with `#define DEBUG_8051_BUF_MEM idata` (8052 with 256 bytes of RAM, add `RAMSIZE(256)` to the linker) or `xdata` (part with external RAM).
- 8051: `*** ERROR L114`/`L105` because a log function is never called
  - Keil C51 always reserves the parameter block of a variadic function. Either call every log function you include, or let the linker drop the unused ones (uVision: *Options for Target → L251 Misc → remove unused sections*, or `LX51 ... REMOVEUNUSED`).
- 8051: message text is cut short
  - That is the bounded formatter working as intended; raise `DEBUG_BUFFER_LEN` (and move the buffers to `idata`/`xdata`) for longer messages.
- 8051: `%f` prints `?`
  - `DEBUG_8051_ENABLE_FLOAT` is set to 0; set it back to 1 (costs ~2 KB of code space).

## Changelog

### v1.0 (2025-12-10)

- **Initial release**: First release of STM32 Elegant Debug library
- **Features**: Formatted log output, timestamp, ANSI colors, type prefixes and other basic functionality
- **Support**: Dual C and C++ versions, compatible with STM32 HAL library

### v1.1 (2025-12-11)

- **New**: Support for displaying filename and line number in error and warning messages
  - C version: Enable with `debug_setFilenameLineEnabled(true)`, `debug_error()` and `debug_warning()` automatically include filename and line number
  - C++ version (C++20 and above): Enable with `setFilenameLineEnabled(true)`, supports `std::source_location`
- **New**: Supports more ANSI colors and styles
- **Improvement**: Updated API documentation to accurately reflect actual functionality

### v1.2 (2026-02-22)

- **New**: This library now supports USB-CDC output. To use this feature, simply enable the USB_DEVICE middleware in your MX configuration and set USB to CDC class. Then define `USB_AS_DEBUG_PORT` to `1` in the library's header file.

### v1.3 (2026-03-01)

- **Improvement**: Custom colors can now accept runtime variables. This enables more diverse usage, such as setting gradient colors based on numeric values (as shown in the example image).

### v1.4 (2026-07-16)

- **New**: Renesas RA FSP platform supported (use `USE_RA_FSP` macro)
  - SCI UART output (both SCI and SCI_B supported)
  - Independent tick counter `_debug_tick_ms`, fed from user timer ISR: C `debug_tick()` / C++ `ElegantDebug::tick()` inline functions
- **New**: Platform selection mechanism (`#if defined(USE_STM32_HAL)` / `#elif defined(USE_RA_FSP)`), conditional compilation

### v1.5 (2026-07-24)

- **New**: TI MSPM0 DL platform supported (use `USE_TI_MSPM0_DL` macro)
  - The timestamp shares `_debug_tick_ms` with the RA platform, feeds in from SysTick ISR
  - The platform selection mechanism is extended to three platform conditional compilations

### v1.6 (2026-08-09)

- **Improvement**: In v1.4 and v1.5, consecutive log calls on the RA FSP platform (without manual delays between them) would cause data loss due to the non-blocking TX behaviour. Added `onTxComplete()` / `debug_onTxComplete()` which must be called from the TX complete UART callback to signal that the transmission has finished.

### v1.7 (2026-11-13)

- **New**: 8051 (Keil C51) platform supported (use `USE_8051_C51` macro, C implementation only)
  - Output is streamed byte by byte through a user supplied `void (*)(char)` function, so no vendor UART HAL is needed
  - The timestamp shares `_debug_tick_ms` with the RA / MSPM0 platforms and is fed by `debug_tick()`
  - The library is compiled `reentrant` on 8051 and all log functions share one message buffer, so a bare 8051 with 128 bytes of RAM still links
  - `DEBUG_BUFFER_LEN` defaults to 32; `DEBUG_8051_BUF_MEM` moves the buffers to `idata`/`xdata`; `DEBUG_8051_ENABLE_FLOAT` drops `%f` to save ~2 KB of code space
  - C51 ships no `snprintf()`/`vsnprintf()`, so a small bounded formatter is included; the platform selection mechanism is extended to four platforms
  - C90 limitation: `debug_error()` / `debug_warning()` cannot append `__FILE__`/`__LINE__` automatically, use `debug_error_fileline(__FILE__, __LINE__, ...)`

## Other

> This project was inspired by a suggestion from Zodiak_Jealously ;p

WilliTourt 2025.12.10 initial release | willitourt@foxmail.com

Contributions and PRs welcome!!
