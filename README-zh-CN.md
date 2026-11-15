# Elegant Debug for 8051 Core (STC89C52RC)

> **Language/语言**: [English](README.md) | [简体中文](README-zh-CN.md)

## 简介

- 这是一个轻量的串口调试库，适用，适配8051内核，提供格式化日志输出功能。
- 特点：
  - 格式化日志
  - 可选的时间戳
  - 可选的 ANSI 颜色输出，包括完全自定义的24位颜色
  - 便捷的类型前缀（ERROR/WARNING/INFO/OK/SUCCESS）

![Example](example.png)

支持错误信息文件名行号输出：
![Showing Filename and Line](example_fileline.png)

支持自定义颜色输出：
![Custom Color](example_customcolor.png)

## 文件结构（仓库）

- C 版本放在 `Src-C/`

## 快速开始

- 本分支专用于 8051，使用 C 语言。
- 将 `Src-C/ElegantDebug.h` 和 `Src-C/ElegantDebug.c` 加入工程，包含路径添加 `Src-C`。只有一个头文件，实现文件只编译一次。
- 先初始化 UART 并提供标准库的 `putchar`，再调用 `debug_init`。库不初始化 UART 或定时器。
- 使用标准库 `vprintf` 逐字符发送，`DEBUG_BUFFER_LEN` 为保留的兼容宏，不为 8051 分配 256 字节消息缓冲区，也不限制单条消息长度。

### 8051 Keil C51 平台（仅 C 版本）

1. 将 `Src-C/ElegantDebug.h` 与 `Src-C/ElegantDebug.c` 加入 Keil C51 工程。本分支专用于 8051，不需要定义 `USE_8051` 或选择其他平台宏，工程宏列表可以保持为空。
2. 将以下内容填写到 **Options for Target → BL51 Misc → Overlay**，使未调用的变参日志接口也能复用参数区，避免 DATA 空间溢出。此设置要求日志和 RGB 接口仅从主循环调用，`debug_tick()` 仅从定时器中断调用。

```text
MAIN ! (_DEBUG_INIT, DEBUG_TICK, _DEBUG_LOG, _DEBUG_LOGWITHTYPE,
_DEBUG_ERROR_FILELINE, _DEBUG_WARNING_FILELINE, _DEBUG_ERROR, _DEBUG_WARNING,
_DEBUG_OK, _DEBUG_SUCCESS, _DEBUG_INFO, _DEBUG_SETTIMESTAMPENABLED,
_DEBUG_SETCOLORENABLED, _DEBUG_SETFILENAMELINEENABLED,
_CUSTOMTEXTCOLOR, _CUSTOMBGCOLOR)
```

3. 初始化串口，并提供符合 C51 标准库声明的 `char putchar(char)`。例如，轮询发送实现如下；串口中断处理程序不能同时清除 TI。

```c
#include <REG52.H>
#include "ElegantDebug.h"

char putchar(char c) {
    SBUF = c;
    while (!TI) { }
    TI = 0;
    return c;
}

/* 串口初始化后调用；8051 不需要 HAL UART 句柄。 */
debug_init(true, true, true);
debug_info("count=%u\r\n", (unsigned int)123);
debug_error_fileline(__FILE__, __LINE__, "error=%d\r\n", (int)-1);
```

4. 需要时间戳时，在约 1 ms 的定时器 ISR 中调用 `debug_tick()`；不需要时将 `debug_init` 的第一个参数设为 `false`。库读取 32 位毫秒计数器时会短暂关闭中断并恢复原 EA 状态，计数器约 49.7 天后自然回绕。库不初始化串口或定时器。

C51 不支持可变参数宏，普通 `debug_error()`、`debug_warning()` 不带调用位置；需要位置时使用上述 `_fileline` 接口。格式化采用 C51 标准库 `vprintf`，消息逐字符发送，`DEBUG_BUFFER_LEN` 不限制该分支的消息长度。变参实参应与格式匹配，例如 `(unsigned int)value`；字符串常量用于 `%s` 时写 `(char *)"text"`。SMALL 模型默认变参区为 15 字节，避免一次传入过多参数。

RGB 文本和背景色函数各使用一个 24 字节 IDATA 静态缓冲区，返回值在下次调用相同函数时覆盖。日志不会自动添加换行，按需要在格式字符串中使用 `\r\n`。

### 8051 SDCC 平台（仅 C 版本）

在 VS Code + EIDE 中使用普通 **SDCC** 工具链，目标架构设为 `mcs51`，C 标准设为 C89。将 `Src-C` 加入源码目录及包含路径。本分支专用于 8051，不需要定义 `USE_8051` 或选择其他平台宏，工程宏列表可以保持为空。

在你的代码中包含头文件并初始化。先完成 UART 初始化并提供 `int putchar(int c)`，再从 `main()` 中调用下面的 `Debug_Init()`：

```c
#include <8052.h>
#include "ElegantDebug.h"

void Debug_Init(void){
    debug_init(false, false, false);
    debug_info("Hello from SDCC!\r\n");
}
```

构建参数可使用：

```text
--model-small --iram-size 256 --xram-size 0 --code-size 8192 --opt-code-size
```

应用程序包含 `<8052.h>`，提供 `int putchar(int c)`，中断采用 `void Timer2_ISR(void) __interrupt(5)` 写法。先初始化 UART，再调用 `debug_init(true, false, true)`；需要时间戳时，从约 1 ms 的定时器 ISR 调用 `debug_tick()`。库通过保存并恢复 EA 保护时间戳读取，同时保留 Keil C51 支持。

默认 SDCC mcs51 SMALL 格式化库不支持 `%f` 输出，可以使用整数定点数，例如 `debug_info("value=%u.%02u\r\n", (unsigned int)25, (unsigned int)50)`。改变格式化库时，应重新评估 Flash 和 RAM。串口初始化、波特率、时钟模式和 `putchar` 由应用程序负责。

### 串口与时间戳

P3.1/TXD 接串口模块 RXD，P3.0/RXD 接 TXD，并共地。打开 HEX 后在 STC 下载工具中选择实际串口和 STC89C52RC，等待下载提示后冷启动开发板。下载结束再打开串口接收，设置 9600、8 位数据、无校验、1 位停止位；先打开串口再复位可看到启动日志。

不要让按键占用 P3.0/P3.1。查看日志期间不要按仍接在 TXD 上的按键。

需要时间戳时，每约 1 ms 从中断调用 `debug_tick()`。示例 Timer2 使用 `0xFC66` 自动重装，在 11.0592 MHz、12T 下约为 1.000434 ms，适合日志计时；精确长期计时需另行校准。读取计数器时会短暂关闭中断并恢复原 EA 状态，32 位计数约 49.7 天后回绕。

### 示例

```c
#include "ElegantDebug.h"

void app_start(void) {
    debug_init(false, false, false);
    debug_info("UART: %u baud\r\n", (unsigned int)9600);
    debug_logWithType("SENSOR", COLOR_CYAN, "temperature=%u.%02u C\r\n",
                      (unsigned int)25, (unsigned int)50);
    debug_setFilenameLineEnabled(true);
    debug_error_fileline(__FILE__, __LINE__, "error=%d\r\n", (int)-1);
}
```

## API

- `void debug_init(bool enable_timestamp, bool enable_color, bool enable_filename_line);`
  - 串口初始化后调用，三个参数依次控制时间戳、颜色、文件名行号，不传串口句柄。
- `void debug_tick(void);`
  - 定时器 ISR 中调用以递增内部毫秒计数器，用于时间戳功能。
- `void debug_log(const char* format, ...);`
  - 原始格式化输出（无前缀）。
- `void debug_logWithType(const char* type, const char* style, const char* format, ...);`
  - 自定义前缀和样式的输出（例如 `"\033[91m"` + `"[ CUSTOM ]"`）。
- 便捷类型输出（无自动文件名行号）：
  - `void debug_error(const char* format, ...);`
  - `void debug_warning(const char* format, ...);`
- 便捷类型输出（无文件名行号）：
  - `void debug_ok(const char* format, ...);`
  - `void debug_success(const char* format, ...);`
  - `void debug_info(const char* format, ...);`
- 运行时设置：
  - `void debug_setTimestampEnabled(bool enabled);`
  - `void debug_setColorEnabled(bool enabled);`
  - `void debug_setFilenameLineEnabled(bool enabled);`

### 关于ANSI转义码

库中的颜色和样式宏均为 ANSI 转义码，如果终端不支持，可以通过运行时设置关闭颜色输出，并停用%s修饰的操作。

要为你的输出字符串设置自定义颜色和样式，请用一对 `%s` 来包裹颜色/样式宏和用以清除颜色/样式的宏。例如：

```c
debug_setColorEnabled(true);
debug_log("%sred text%s\r\n", (char *)COLOR_RED, (char *)CLR);
debug_log("%srgb text%s\r\n", COLOR_CUSTOM(0, 128, 255), (char *)CLR);
```

对于24位自定义颜色，请使用 `COLOR_CUSTOM(r,g,b)` 和 `BG_COLOR_CUSTOM(r,g,b)` 宏，其中 `r`、`g`、`b` 为0~255之间的整数。这些宏现在内部调用了运行时辅助函数，因此可以使用变量而非字面常量传递颜色值。

关于 ANSI 转义码的更多信息，请参考：[ANSI escape code - Handwiki](https://handwiki.org/wiki/ANSI_escape_code)

## 常见问题与排查

- 无输出：确认 UART 已初始化、TI 未被串口中断同时清除、接线及波特率正确，重新复位开发板。
- 时间戳不走：确认定时器、总中断以及周期性的 `debug_tick()` 调用；1.25 ms 应用节拍不能直接当作 1 ms 使用。
- Keil DATA 溢出：检查 SMALL 模型及示例中的 Overlay 设置；新增业务变量后仍需评估 RAM 和栈空间。
- 终端显示 ANSI 控制字符：关闭颜色，并停用手动插入转义码的语句。
- 格式化值异常：检查参数类型；SDCC 的 `<NO FLOAT>` 表示默认库未启用浮点格式化。

## 更新日志

### v1.0 (2025-12-10)

- **初始发布**: STM32 Elegant Debug 库首次发布
- **功能**: 格式化日志输出、时间戳、ANSI颜色、类型前缀等基础功能
- **支持**: C和C++双版本，适配STM32 HAL库

### v1.1 (2025-12-11)

- **新增**: 支持在错误和警告信息中显示文件名和行号
  - C版本：通过 `debug_setFilenameLineEnabled(true)` 启用，`debug_error()` 和 `debug_warning()` 自动包含文件名行号
  - C++版本（C++20及以上）：通过 `setFilenameLineEnabled(true)` 启用，支持 `std::source_location`
- **新增**: 支持更多的ANSI颜色和样式
- **改进**: 更新API文档，更准确地反映实际功能

### v1.2 (2026-02-22)

- **新增**: 此库现在支持USB-CDC输出。要使用此功能，只需打开MX配置中的USB_DEVICE中间件，将USB设置为CDC类。然后在此库头文件中将 `USB_AS_DEBUG_PORT` 定义为 `1` 即可。

### v1.3 (2026-03-01)

- **改进**: 现在自定义颜色可以传入运行时变量。这支持了更多样的用法，例如根据数字大小设置对应的渐变颜色（如简介处图）

### v1.4 (2026-07-16)

- **新增**: 支持 Renesas RA FSP 平台（`USE_RA_FSP` 宏）
  - SCI UART 输出
  - 独立的时间戳计数器 `_debug_tick_ms`，用户从定时器 ISR 喂入 C 版本 `debug_tick()` / C++ 版本 `ElegantDebug::tick()` 内联函数
- **新增**: 平台选择机制（`#if defined(USE_STM32_HAL)` / `#elif defined(USE_RA_FSP)`）条件编译

### v1.5 (2026-07-24)

- **新增**: 支持 TI MSPM0 DL 平台（`USE_TI_MSPM0_DL` 宏）
  - 时间戳与 RA 平台共用 `_debug_tick_ms`，用户从 SysTick ISR 喂入
  - 平台选择机制扩展至三个平台条件编译

### v1.6 (2026-08-09)

- **改进**: v1.4 和 v1.5 版本中，RA FSP 平台中如连续发送log（中间无手动延时），会导致发送的数据丢失。针对 RA FSP 的非阻塞发送函数，添加了`onTxComplete()`函数，此函数在 TX 完成回调中必须调用来更新发送完成标志。

### v1.6 - 8051 (2026-11-15)

- **新分支**: 当前分支基于主分支产生。特用于 STC89C52RC 或其他 8051 内核的单片机

## 其他

> 此库灵感最初源于学长Zodiak_Jealously的提议 ;p

WilliTourt 2025.12.10 initial release | willitourt@foxmail.com

欢迎提交 PR 和 Issue！！
