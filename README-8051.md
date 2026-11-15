# ElegantDebug — 8051 / 8052 port (Keil C51)

This directory contains the 8051 port of ElegantDebug as a **self-contained
module**. `Src-C/ElegantDebug.c` and `Src-C/ElegantDebug.h` are left exactly as
the author wrote them — no `#if` branches, no new platform macros. The 8051 code
lives in `Src-C/ElegantDebug_8051.c` / `Src-C/ElegantDebug_8051.h`, next to the
originals, and does not include them.

## Why a separate module

Keil C51 reserves a permanent static argument area in `DATA` for **every**
variadic function — roughly 48 bytes each. The original library has six of them:

| Implementation | `DATA` | Link result |
|---|---|---|
| original (6 variadic functions) | 316 B | `ERROR L107: ADDRESS SPACE OVERFLOW` |
| one variadic entry + formatter | 272 B | still `L107` |
| this port (no varargs at all) | **47 B** | `LINK/LOCATE RUN COMPLETE` |

An 8051 has 128 bytes of direct RAM, so a variadic API cannot fit. Keeping the
port in its own files also means the PR is purely additive: no existing file is
touched, and nothing changes for the STM32 / RA / MSPM0 paths.

## API

Fixed signatures only — a string, or a string plus one number:

```c
void debug_8051_init(ed8051_bool timestamp, ed8051_bool color,
                     ed8051_bool fileline);
void debug_8051_tick(void);                       /* call from a 1 ms ISR */

void debug_8051_log(const char *text);            /* timestamp only        */
void debug_8051_info(const char *text);
void debug_8051_ok(const char *text);
void debug_8051_success(const char *text);
void debug_8051_warning(const char *file, ed8051_u16 line, const char *text);
void debug_8051_error  (const char *file, ed8051_u16 line, const char *text);

void debug_8051_log_u32(const char *label, ed8051_u32 value);  /* + CRLF */

void debug_8051_setTimestamp(ed8051_bool enabled);
void debug_8051_setColor(ed8051_bool enabled);

#define DEBUG_8051_WARNING(text)  debug_8051_warning(__FILE__, __LINE__, (text))
#define DEBUG_8051_ERROR(text)    debug_8051_error(__FILE__, __LINE__, (text))
```

Output format is identical to the original library:

```
[00:00:01.250] \033[94m\033[1m[INFO]\033[0m hello
```

(no implicit newline — write `\r\n` yourself, same convention as `debug_info()`)

## UART output

By default a **polled** transmitter is used (`SBUF = c; while (!TI);`), so the
example keeps `ES = 0`. If your firmware already serves `TI` in a UART
interrupt, define the hook before including the header:

```c
#define ED8051_PUTC(c)  my_uart_put((unsigned char)(c))
```

## Requirements and limitations

- 8052-class device (256 bytes RAM: AT89C52, STC89C52RC). A 128-byte 8051 does
  not fit.
- No `%f` and no `customTextColor()`: both need the variadic formatter that the
  port deliberately avoids.
- C90 only — no `//` comments, no mixed declarations, no `<stdint.h>`.
- SDCC: the module uses `<reg52.h>`; for SDCC change it to `<8052.h>` and
  `__data`/`__idata` attributes. Not tested here (no SDCC on the author's
  machine).

## Verification

Keil C51 9.60.7 + BL51, device AT89C52 (`RAMSIZE(256)`), 11.0592 MHz,
UART mode 1 at 9600 8N1:

```
ElegantDebug_8051.c   0 Warning(s)  0 Error(s)
example main.c        0 Warning(s)  0 Error(s)
Program Size: data=47.0  xdata=0  code=2038
LINK/LOCATE RUN COMPLETE.  0 WARNING(S), 0 ERROR(S)
```

Hardware run: STC89C52RC + CH340, MobaXterm at 9600 8N1 (see the PR description
for the screenshot): timestamps advance at ~500 ms per line and every type
prefix renders in its color.

## Build the example

See `example_8051/README.md`.
