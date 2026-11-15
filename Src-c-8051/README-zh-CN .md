# ElegantDebug for 8051 (STC89C52RC)

> **Language/语言**: [English](README.md) | [简体中文](README-zh-CN.md)

## 简介

- 这是一个面向 8051（STC89C52RC）的轻量串口调试库，使用 Keil C51 编译，提供带 ANSI 颜色的格式化日志输出。
- 特点：
  - 格式化日志输出，支持常用的整数、字符、字符串、十六进制和浮点数
  - 可选的时间戳，格式为 `[HH:MM:SS.mmm]`
  - 可选的 ERROR / WARNING 文件名和行号显示
  - 可选的 ANSI 颜色输出，包括完全自定义的 24 位真彩色
  - 便捷的级别前缀（ERROR / WARNING / INFO / OK / SUCCESS）
  - 直接操作 SBUF 阻塞发送，不使用 `printf`、`vprintf`、`sprintf`，也不拼接长字符串
  - 所有日志函数统一经过 `_send()` 发送，方便统一控制和裁剪

## 文件结构

- `ElegantDebug.c`：库的实现
- `ElegantDebug.h`：颜色宏、函数声明和格式说明
- `main1.c`、`main2.c`：自检示例，分两次编译覆盖全部函数

## 快速开始

### 1. 添加文件

把 `ElegantDebug.c` 和 `ElegantDebug.h` 加入你的 Keil 工程，确保 `ElegantDebug.c` 被编译并参与链接。

- Keil MDK / C51：Project 窗口右键 → Add Existing Files
- EIDE（VS Code）：直接拖入项目，或右键项目 → 添加源文件

### 2. 初始化串口和定时器

```c
#include <REGX52.H>
#include "ElegantDebug.h"
#include "delay_xms.h"

/* Timer0 中断：每 1ms 一次，为时间戳提供基准 */
void Timer0_Isr(void) interrupt 1
{
    TL0 = 0x66;
    TH0 = 0xFC;
    debug_tick();      /* 只做自增，不要在中断里调用日志函数 */
}

void main(void)
{
    Debug_init2D(0xFA);   /* 9600 波特率 @ 11.0592MHz，波特率加倍 */
    Timer0_Init();        /* 1ms 定时器 */
    debug_init(1, 1, 0);  /* 时间戳开、颜色开、文件行号关 */

    debug_info("Hello from 8051!\r\n");

    while (1)
    {
    }
}
```

### 3. 工程设置

**内存模型必须选择 Large**，否则 DATA 段会溢出：

```
Options for Target → Target → Memory Model → Large: variables in XDATA
```

EIDE 中对应：

```
项目设置 → C51 → Memory Model → Large: variables in XDATA
```

原因：8051 的 DATA 只有 128 字节，本库的格式化函数和多组字符串常量需要更多空间。STC89C52RC 有 256 字节片内 XDATA，足够使用。

**建议同时限制可变参数长度**，在 C51 → Misc Controls 中填入：

```
MAXARGS(4)
```

本库每次调用的参数都不超过 4 字节，这样可以进一步降低 RAM 占用。

## 共用示例

```c
debug_log("Hello world!\r\n");
debug_info("System started. Free memory: %d bytes\r\n", free_mem);
debug_warning_fileline(__FILE__, __LINE__, "Low battery: %d %%\r\n", percent);
debug_error_fileline(__FILE__, __LINE__, "Sensor failed: code = %d\r\n", err_code);
debug_ok("Operation finished in %d ms\r\n", elapsed_ms);
debug_success("Configuration saved\r\n");
debug_logWithType("CUSTOM", COLOR_GREEN, "custom message: %s\r\n", "ok");

/* 颜色和文本样式 */
debug_log("%sI am GREEN!%s\r\n", COLOR_GREEN, CLR);
debug_log("%sI have a colored background!%s\r\n", BG_RED, CLR);
debug_log("%sI am underlined and %spartly colored!%s%s\r\n",
          UNDERLINE, COLOR_DARK_MAGENTA, CLR, CLR);

/* 开启 ERROR / WARNING 的文件名和行号 */
debug_setFilenameLineEnabled(1);
```

> 注意：日志函数不会自动换行，需要换行时请在格式字符串中自己写 `\r\n`。

## API

### 初始化

- `void debug_init(bit enable_timestamp, bit enable_color, bit enable_filename_line);`
  - 设置三个运行时开关的初始状态，必须在使用日志函数前调用。
- `void Debug_init2D(unsigned char baud);`
  - 串口初始化，工作方式 1，8 位 UART，波特率加倍，阻塞发送，不使用串口中断。
  - 参数是波特率重装值，实际波特率是该值对应的两倍。9600 @ 11.0592MHz 传 `0xFA`。
- `void Timer0_Init(void);`
  - Timer0 初始化，1ms @ 11.0592MHz。
  - 中断服务函数需要用户自己实现，并在里面调用 `debug_tick()`。

### 时间戳

- `extern volatile unsigned long _debug_tick_ms;`
- `#define debug_tick() (_debug_tick_ms++)`
  - 在 1ms 定时器中断中调用，为时间戳提供毫秒计数。

### 底层发送

- `void Uart_SendByte(unsigned char dat);`
  - 阻塞发送一个字节，等待 TI 置位后清零。
- `void Uart_SendUInt(unsigned int Num);`
  - 发送无符号 16 位整数，0 ~ 65535。
- `void Uart_SendInt(int Num);`
  - 发送有符号 16 位整数，-32768 ~ 32767。
- `void Uart_SendULong(unsigned long Num);`
  - 发送无符号 32 位整数，0 ~ 4294967295。
- `void Uart_SendLong(long Num);`
  - 发送有符号 32 位整数。
- `void Uart_SendString(const char *str);`
  - 发送以 `'\0'` 结尾的字符串。

### 日志函数

- `void debug_log(const char *format, ...);`
  - 原始格式化输出，无级别前缀。
- `void debug_logWithType(const char *type, const char *style, const char *format, ...);`
  - 自定义标签和颜色，例如 `debug_logWithType("SENSOR", COLOR_GREEN, "ok\r\n");`
- `void debug_error_fileline(const char *file, int line, const char *format, ...);`
  - 红色 `[ERROR]`，支持文件名和行号。
- `void debug_warning_fileline(const char *file, int line, const char *format, ...);`
  - 黄色 `[WARNING]`，支持文件名和行号。
- `void debug_ok(const char *format, ...);`
  - 绿色 `[OK]`，不带文件名行号。
- `void debug_success(const char *format, ...);`
  - 绿色 `[SUCCESS]`，不带文件名行号。
- `void debug_info(const char *format, ...);`
  - 蓝色 `[INFO]`，不带文件名行号。

### 运行时开关

- `void debug_setTimestampEnabled(bit enabled);`
- `void debug_setColorEnabled(bit enabled);`
- `void debug_setFilenameLineEnabled(bit enabled);`

### 自定义颜色

- `const char *customTextColor(unsigned char r, unsigned char g, unsigned char b);`
- `const char *customBgColor(unsigned char r, unsigned char g, unsigned char b);`
  - 返回内部静态 ANSI 字符串，可以直接用 `%s` 拼进日志。
  - 注意下一次调用会覆盖同一块缓冲区。

### 格式支持

| 写法 | 说明 |
|---|---|
| `%d` `%i` | 有符号 16 位整数 |
| `%u` | 无符号 16 位整数 |
| `%ld` | 有符号 32 位整数 |
| `%lu` | 无符号 32 位整数 |
| `%c` | 单个字符 |
| `%s` | 字符串 |
| `%x` `%X` | 16 位十六进制，小写 / 大写 |
| `%lx` `%lX` | 32 位十六进制，小写 / 大写 |
| `%f` | 浮点数，固定 6 位小数 |
| `%%` | 输出一个百分号 |

不支持宽度、精度、`%o`、`%p`、`%e`、`%g` 和 `+` 号标志。

### 需要注意

Keil C51 不支持可变参数宏，因此**没有** `debug_error(...)` 和 `debug_warning(...)` 宏。ERROR / WARNING 请直接调用带文件行号的函数：

```c
debug_error_fileline(__FILE__, __LINE__, "error=%d\r\n", value);
debug_warning_fileline(__FILE__, __LINE__, "warning=%d\r\n", value);
```

只有 ERROR / WARNING 支持文件名行号，INFO / OK / SUCCESS 不支持，与原库行为一致。

### 关于 ANSI 转义码

库中的颜色和样式宏均为 ANSI 转义码。如果终端不支持，可以通过 `debug_setColorEnabled(0)` 关闭颜色输出，此时只会发送纯文本。

要为输出字符串设置自定义颜色和样式，请用一对 `%s` 包裹颜色/样式宏和清除宏：

```c
debug_log("Hello World! %sI am RED!%s %s%sI am italic and GREEN!%s Now cleared italic but still GREEN.%s\r\n",
          COLOR_RED, CLR_TEXT_COLOR, ITALIC, COLOR_GREEN, CLR_ITALIC, CLR);
```

24 位自定义颜色使用 `customTextColor(r, g, b)` 和 `customBgColor(r, g, b)`，其中 `r`、`g`、`b` 为 0 ~ 255。也可以使用 `COLOR_CUSTOM(r,g,b)` 和 `BG_COLOR_CUSTOM(r,g,b)` 宏。

关于 ANSI 转义码的更多信息，请参考：`https://handwiki.org/wiki/ANSI_escape_code`

## 常见问题与排查

- 串口无输出或者一堆乱码：
  - 确认串口已正确初始化，**特别是波特率**。
  - `Debug_init2D(0xFA)` 对应 9600 @ 11.0592MHz；如果是 12MHz 晶振，需要重新计算重装值。
- 时间戳一直是 `[00:00:00.000]`：
  - 确认 Timer0 已初始化：`TR0`、`ET0`、`EA` 都要为 1。
  - 确认 Timer0 中断函数里调用了 `debug_tick()`。
- 链接报 `L107: ADDRESS SPACE OVERFLOW`：
  - 把 Memory Model 改成 `Large: variables in XDATA`。
  - 或在 C51 → Misc Controls 中加 `MAXARGS(4)`。
- 颜色显示成 `.[91m` 这类字符：
  - 串口助手不支持 ANSI，调用 `debug_setColorEnabled(0)` 关闭颜色，或换支持 ANSI 的终端。
- 时间戳被串口助手拆成两行：
  - 关闭串口助手的「接收时间戳」和「接收分帧显示」，或调大分帧超时。
- 程序卡在 `while (!TI);`：
  - 确认串口已初始化，`_uart_ready` 为 1；未初始化时 `Uart_SendByte()` 会直接返回，不会死等。
- 中文输出乱码：
  - 串口输出部分只使用英文字符；源码中文注释请用 ANSI/GBK 编码保存。

## 更新日志

### v1.0 (2026-11-15)

- **初始发布**: 面向 8051（STC89C52RC）的串口调试库
- **功能**: 格式化日志、时间戳、ANSI 颜色、级别前缀、文件名行号、24 位自定义颜色
- **实现**: 直接操作 SBUF 阻塞发送，不使用 printf 系列函数；统一 `_send()` 入口
- **适配**: Keil C51、EIDE（VS Code）
