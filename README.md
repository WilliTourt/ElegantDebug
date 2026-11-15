# Elegant Debug for 8051 (STC89C52RC)

> **Language/语言**: [English](README.md) | [简体中文](README-zh-CN.md)

## 8051 quick start

Build the C library with SDCC for the STC89C52RC. The library sends output
through the application's `int putchar(int)`. The example configures UART
mode 1 at 9600 baud, 8N1, for an 11.0592 MHz oscillator in 12T mode.

From the repository root:

```sh
sdcc -mmcs51 --std-c99 -ISrc-C -c Src-C/ElegantDebug.c -o ElegantDebug.rel
sdcc -mmcs51 --std-c99 --iram-size 256 --xram-size 0 --code-size 8192 -ISrc-C examples/8051/main.c ElegantDebug.rel -o demo.ihx
```

Open a serial terminal at 9600 8N1 and reset the board. Expected output:

```text
[INFO] UART ready at 9600 baud
[OK] LED 64
```

If your application already supplies a UART `putchar`, initialize that driver
and call `debug_init(timestamp, color, filename_line)`. Call logging functions
from the main loop. For timestamps, call `debug_tick()` from a 1 ms timer ISR.
The SDCC backend streams the message and supports the formats in SDCC's
`vprintf`.
