# Elegant Debug for STM32, Renesas-RA & TI-MSPM0 MCUs

> **Language/语言**: [English](README.md) | [简体中文](README-zh-CN.md)

---

### 8051 Keil C51 Support
This branch adds support for 8051 microcontrollers (e.g., STC89C52RC). Adaptations:
1. **C90 Compatibility**: Isolated C99 varargs (`va_list`, `vsnprintf`) using `#ifdef __C51__` to prevent compilation errors.
2. **Memory Optimization**: Replaced the large intermediate array in `_send` with direct streaming via the `SBUF` register, eliminating `L107 ADDRESS SPACE OVERFLOW`.
3. **Limitations**: Floating-point (`%f`) and dynamic memory allocation are not supported due to the 128-byte RAM limit.

**Usage**: Include `ElegantDebug.h` in your Keil C51 project, initialize the UART baud rate, and call `debug_tick()` in a 1ms timer ISR for timestamps.
