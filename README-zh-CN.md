# Elegant Debug for STM32, Renesas-RA, TI-MSPM0 & 8051 MCUs

> **Language/语言**: [English](README.md) | [简体中文](README-zh-CN.md)

## 简介

- 这是一个轻量的 串口/USB-CDC 调试库，C / C++ 均适用，适配STM32 HAL库、瑞萨RA FSP、TI MSPM0 DL（Driver Library）以及 8051 内核（Keil C51），提供格式化日志输出功能。
- 特点：
  - 格式化日志
  - 可选的时间戳
  - 可选的 ERROR/WARNING 信息 显示报错文件名和行号
  - 可选的 ANSI 颜色输出，包括完全自定义的24位颜色
  - 便捷的类型前缀（ERROR/WARNING/INFO/OK/SUCCESS）

![Example](example.png)

支持错误信息文件名行号输出：
![Showing Filename and Line](example_fileline.png)

支持自定义颜色输出：
![Custom Color](example_customcolor.png)

## 文件结构（仓库）

- C 版本放在 `Src-C/`
- 另有 C++ 版本放在 `Src-CPP/`
- 8051（Keil C51）适配**只在 C 版本中提供**：C51 是 C90 编译器，且不假定 8051 上有可用的 C++ 工具链。

## 快速开始

- 依赖 STM32Cube HAL 驱动 / Renesas RA FSP，必须启用一个串口，或者启用USB-CDC（USB模式当前仅在STM32上有效。在MX配置中打开USB_DEVICE中间件，设置为CDC类即可）。8051 不需要任何厂商 HAL，只需要你提供一个逐字节输出的函数。
- 输出长度由 `DEBUG_BUFFER_LEN` 宏控制（默认 256，8051 上默认 32）
- 输出方式（串口/USB）由 `USB_AS_DEBUG_PORT` 宏控制（默认0，使用串口；设置为1使用USB-CDC）
- ⚠️ **使用前必须选择平台**：在 include 之前，取消注释头文件中 `USE_STM32_HAL`、`USE_RA_FSP`、`USE_TI_MSPM0_DL` 或 `USE_8051_C51` 其中一个宏

### STM32 HAL 平台

1. 将 `Src-C/ElegantDebug.h` 与 `Src-C/ElegantDebug.c` 添加到你的工程（确保 `ElegantDebug.c` 被编译并加入链接）。
   - Makefile/CMake：将 `.../Src-C/ElegantDebug.c` 加入 `SRCS` / `target_sources`。
   - Keil/MDK / IAR / CubeIDE：在 Project Explorer 中 Right-click -> Add Existing Files。

2. 在你的代码中包含头文件并初始化（以 STM32 `huart1` ，C语言版本为例，C++版本用法完全一致）：

```c
#define USE_STM32_HAL
#include "ElegantDebug.h"

// 在合适的位置（比如 main() 里）初始化
// 如果 USB-CDC 被设置为调试端口（编译时宏 `USB_AS_DEBUG_PORT` 为 1），`huart` 参数可以传 NULL，库会忽略它。
debug_init(&huart1, true /* enable timestamp */, true /* enable color */, false /* enable filename line */);
```

### Renesas RA FSP 平台

1. 同样将 `Src-C/ElegantDebug.h` 与 `Src-C/ElegantDebug.c` 添加到你的瑞萨工程。

2. 在 RASC 中配置一个 **SCI UART** 外设（随便哪个SCI通道）。
![Example of RA UART Config](example_ra_uart_config.png)

3. 初始化并使用（这里以 cpp 版本 `g_uart0` 为例）。**注意（非常重要）：** RA FSP 需要特别在串口中断里调用`ElegantDebug::onTxComplete(p_args);`以检测发送是否完成，未调用此函数将会导致后续发送卡死循环。

```cpp
#include "ElegantDebug.h"

// 全局 debug 实例
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

4. **时间戳喂入**：由于瑞萨没有像STM32一样的`GetTick()`，库需要你创建一个**1ms**计数器来生成时间戳，你需要从定时器回调中调用 `debug_tick()`（C版本）/`ElegantDebug::tick()`（CPP版本） 来递增这个计数器。例如配置一个AGT：
![Example of RA Tick Config](example_ra_tick_config.png)

```c
// 在你的 1ms 定时器 ISR 中：
void vSysTick(void) {
    debug_tick(); // 喂入 ElegantDebug 时间戳. C++ 版本使用 `ElegantDebug::tick()` 替代
}
```

### TI MSPM0 DL 平台

1. 同样将 `Src-C/ElegantDebug.h` 与 `Src-C/ElegantDebug.c` 添加到你的 CCS Theia 工程。

2. 在 SysConfig 中配置一个 **UART** 外设，确保启用实例。

3. 初始化并使用（C 版本为例）：

```c
#define USE_TI_MSPM0
#include "ElegantDebug.h"

debug_init(UART0_INST, true, true, false);
debug_info("Hello from TI MSPM0!");
```

4. **时间戳喂入**：与 RA 平台相同，请创建一个**1ms**计数器，或者从 SysTick ISR 调用 `debug_tick()`（C版本）/ `ElegantDebug::tick()`（CPP版本）来更新时间戳。

### 8051（Keil C51）平台

C 版本同样可以在 8051 内核上运行（AT89C52/AT89S52、STC89C52、STC15、N76E003……）。它不需要任何厂商 HAL：库通过你提供的函数**逐字节**输出，因此任何串口驱动、软件模拟串口或普通 GPIO 都可以使用。

1. 把 `Src-C/ElegantDebug.h` 与 `Src-C/ElegantDebug.c` 添加到 Keil 工程（Project 窗口 → 右键 → *Add Existing Files*；确认 `ElegantDebug.c` 已参与编译和链接）。

2. 在 include 之前定义 `USE_8051_C51`（也可以直接取消头文件里这一行的注释，这样 `main.c` 和 `ElegantDebug.c` 都能看到它）。

3. 传入一个逐字节输出函数并初始化：

```c
#include <REG52.H>

#define USE_8051_C51
#include "ElegantDebug.h"

/* 每输出一个字符调用一次；必须等到该字节进入发送缓冲后才返回。 */
static void uart_putc(char c) {
    SBUF = c;
    while (!TI);
    TI = 0;
}

void main(void) {
    uart_init();                     /* 你自己的串口初始化，例如 9600 8N1 */

    debug_init(uart_putc, true /* 时间戳 */, true /* 颜色 */, false /* 文件名:行号 */);

    debug_log("Hello from 8051!");
    debug_info("Free memory check: %d", 42);
    debug_ok("Elapsed: %u ms", 1234);

    while (1);
}
```

4. **时间戳喂入**：与 RA / MSPM0 平台相同，在 **1ms** 定时器中断里调用 `debug_tick()`：

```c
void timer0_isr(void) interrupt 1 {
    TH0 = 0xFC; TL0 = 0x18;          /* 重新装载，1ms */
    debug_tick();                    /* 时间戳计数器递增 */
}
```

5. **RAM 与 Flash 占用**。Keil C51 会给每个非重入的变参函数在 DATA 里分配固定参数区，8051 无法承受，因此本平台把整个 API 编译为 `reentrant`（参数放在堆栈上），并让所有日志函数共用同一个消息缓冲区。下表是用 Keil C51 V9.54、small 模式在本仓库的 `AT89C52` 演示工程上实测的结果（包含演示程序本身）：

| 配置 | DATA | XDATA | CODE |
| --- | --- | --- | --- |
| 默认（`DEBUG_BUFFER_LEN 32`，缓冲区在 DATA） | 106 B | – | 9521 B |
| `DEBUG_8051_BUF_MEM idata`，`DEBUG_BUFFER_LEN 64` | 30 B（另有 108 B IDATA） | – | 9521 B |
| `DEBUG_8051_BUF_MEM xdata`，`DEBUG_BUFFER_LEN 128` | 30 B | 172 B | 9521 B |
| `DEBUG_8051_ENABLE_FLOAT 0`（在上表基础上） | – | – | 7378 B |

- `DEBUG_BUFFER_LEN` —— 消息缓冲区大小，默认 32，以便普通 8051（128 字节 RAM）也能链接通过。
- `DEBUG_8051_BUF_MEM` —— 内部缓冲区的存储空间。留空表示 DATA（任何 8051 都可用）；在 8052 / AT89S52 / STC89C52 上可设为 `idata`，使用内部 RAM 的高 128 字节；带外部 RAM 的芯片可设为 `xdata`。这两种方式都能把 DATA 让给你的变量，并允许使用更大的 `DEBUG_BUFFER_LEN`。
- `DEBUG_8051_ENABLE_FLOAT` —— 设为 `0` 可去掉 `%f` 支持（节省约 2 KB 代码空间，适合 8 KB Flash 的芯片）。

与 RA / TI MSPM0 一样，8051 适配需要 **Keil C51** 编译器；`Src-CPP/` 的 C++ 版本不支持 8051。

#### 8051 / C90 注意事项

- Keil C51 是 **C90** 编译器，没有 `__VA_ARGS__`，所以 `debug_error(fmt, ...)` 和 `debug_warning(fmt, ...)` 无法自动追加 `__FILE__` / `__LINE__`。在 8051 上这两个宏会映射到不带位置的 `debug_error_plain()` / `debug_warning_plain()`；需要显示文件名和行号时，请直接调用 `debug_error_fileline(__FILE__, __LINE__, ...)` 或 `debug_warning_fileline(__FILE__, __LINE__, ...)`。
- C51 既没有 `snprintf()` 也没有 `vsnprintf()`，因此 `ElegantDebug.c` 内置了一个**有长度限制**的格式化实现，并用上面这两个名字调用它。它支持 `%% %c %s %d %i %u %o %x %X %f`、`-` `0` `+` 空格 `#` 标志、字段宽度、精度以及 `l` 长度修饰符，保证 NUL 结尾且不会写出缓冲区。
- `%f` 由内置格式化实现转换（定点输出，最多 6 位小数）；`%e`、`%g` 按 `%f` 的形式输出，浮点数采用“四舍五入远离 0”的规则。不支持的转换符会原样输出。
- 该库在所有平台上都不是中断安全的；请不要在中断里与主循环同时打印。

### 共用示例

```c
debug_log("Hello world！");
debug_info("System started. Free memory: %d bytes", free_mem);
debug_warning("Low battery: %.2f %%", battery_percentage);  // 自动包含文件名和行号（如果启用）
debug_error("Sensor failed: code = %d", err_code);          // 自动包含文件名和行号（如果启用）
debug_ok("Operation finished in %d ms", elapsed_ms);
debug_success("Configuration saved");
debug_logWithType("[CUSTOM]", "custom message: %s", "ok");

/* 颜色和文本样式 */
debug_log("%sI am GREEN!%s\n", COLOR_GREEN, CLR);
debug_log("%sI have a colored background!%s\n", BG_RED, CLR);
debug_log("%sI am underlined and %spartly colored!%s%s\n", UNDERLINE, COLOR_DARK_MAGENTA, CLR, CLR);

/* 启用 ERROR / WARNING 信息的文件名和行号显示 */
debug_setFilenameLineEnabled(true);  // 启用后，error和warning信息将显示文件名和行号
```

## API

### C 版本

- `void debug_init(UART_HandleTypeDef *huart, bool enable_timestamp, bool enable_color, bool enable_filename_line);`（STM32）
- `void debug_init(uart_instance_t const *uart, bool enable_timestamp, bool enable_color, bool enable_filename_line);`（Renesas RA）
- `void debug_init(UART_Regs *uart_inst, bool enable_timestamp, bool enable_color, bool enable_filename_line);`（TI MSPM0）
- `void debug_init(debug_putc_t uart_putc, bool enable_timestamp, bool enable_color, bool enable_filename_line);`（8051）
  - 初始化库，必须先调用，传入 HAL UART 句柄 / FSP UART 实例 / DL UART 寄存器指针 / 逐字节输出函数，以及是否启用时间戳/颜色/文件名行号显示。`debug_putc_t` 即 `void (*)(char)`；传 `NULL` 时所有打印都为空操作。
- `static inline void debug_tick(void);`（RA FSP、TI MSPM0 与 8051；8051 上是普通 `static` 函数，因为 C51 不支持 `inline`）
  - 定时器 ISR 中调用以递增内部毫秒计数器，用于时间戳功能。
- `void debug_onTxComplete(uart_callback_args_t *p_args);`（仅 RA FSP）
  - RA FSP 特有，用于检测发送是否完成，**未调用此函数会导致后续发送卡死循环。**
- `void debug_log(const char* format, ...);`
  - 原始格式化输出（无前缀）。
- `void debug_logWithType(const char* type, const char* style, const char* format, ...);`
  - 自定义前缀和样式的输出（例如 `"\033[91m"` + `"[ CUSTOM ]"`）。
- 便捷类型输出（自动包含文件名和行号）：
  - `void debug_error(const char* format, ...);`
  - `void debug_warning(const char* format, ...);`
  - 8051：C51 没有 `__VA_ARGS__`，这两个名字会映射到不带位置的 `debug_error_plain()` / `debug_warning_plain()`；需要显示位置时请使用 `debug_error_fileline(__FILE__, __LINE__, ...)` / `debug_warning_fileline(__FILE__, __LINE__, ...)`。
- 便捷类型输出（无文件名行号）：
  - `void debug_ok(const char* format, ...);`
  - `void debug_success(const char* format, ...);`
  - `void debug_info(const char* format, ...);`
- 运行时设置：
  - `void debug_setTimestampEnabled(bool enabled);`
  - `void debug_setColorEnabled(bool enabled);`
  - `void debug_setFilenameLineEnabled(bool enabled);`

### C++ 版本

- `ElegantDebug(UART_HandleTypeDef *huart, bool enable_timestamp = true, bool enable_color = true, bool enable_filename_line = false);`（STM32）
- `ElegantDebug(uart_instance_t const *uart, bool enable_timestamp = true, bool enable_color = true, bool enable_filename_line = false);`（Renesas RA）
- `ElegantDebug(UART_Regs *uart_inst, bool enable_timestamp = true, bool enable_color = true, bool enable_filename_line = false);`（TI MSPM0）
  - 构造函数，传入 HAL UART 句柄 / FSP UART 实例 / DL UART 寄存器指针 和是否启用时间戳/颜色/文件名行号显示。
- `static inline void tick(void);`（仅 RA FSP 和 TI MSPM0）
  - 类静态方法，从定时器 ISR 中调用以递增内部毫秒计数器，用于时间戳功能。
- `static inline void onTxComplete(uart_callback_args_t *p_args);`（仅 RA FSP）
  - RA FSP 特有，用于检测发送是否完成，**未调用此函数会导致后续发送卡死循环。**
- `void log(const char* format, ...);`
  - 原始格式化输出（无前缀）。
- `void logWithType(const char* type, const char* style, const char* format, ...);`
  - 自定义前缀和样式的输出（例如 `"\033[91m"` + `"[ CUSTOM ]"`）。
- 便捷类型输出（C++20及以上版本自动包含文件名和行号）：
  - `void error(const char* format, ...);` (C++20: 支持 `std::source_location`)
  - `void warning(const char* format, ...);` (C++20: 支持 `std::source_location`)
- 便捷类型输出（无文件名行号）：
  - `void ok(const char* format, ...);`
  - `void success(const char* format, ...);`
  - `void info(const char* format, ...);`
- 运行时设置：
  - `void setTimestampEnabled(bool enabled);`
  - `void setColorEnabled(bool enabled);`
  - `void setFilenameLineEnabled(bool enabled);` (仅C++20及以上版本)

#### 需要注意

- `error` 和 `warning` 函数**不能使用可变参数**，因为它们需要获取调用位置（文件名和行号）。

```cpp
	dbg.error("error test... %d\n", 12345); // 不对的用法
	dbg.error("%serror test...\n", BOLD);   // 不对的用法
	dbg.error("error test... \n");          // 对的用法

// 前两行代码由于默认参数和可变参数混合时，参数匹配问题导致编译报错
```

### 关于ANSI转义码

库中的颜色和样式宏均为 ANSI 转义码，如果终端不支持，可以通过运行时设置关闭颜色输出，并停用%s修饰的操作。

要为你的输出字符串设置自定义颜色和样式，请用一对 `%s` 来包裹颜色/样式宏和用以清除颜色/样式的宏。例如：

```cpp
log("Hello World! %sI am RED!%s %s%sI am italic and GREEN!%s Now i cleared italic style but still GREEN.%s\n", 
COLOR_RED, CLR_TEXT_COLOR, ITALIC, COLOR_GREEN, CLR_ITALIC, CLR);
```

对于24位自定义颜色，请使用 `COLOR_CUSTOM(r,g,b)` 和 `BG_COLOR_CUSTOM(r,g,b)` 宏，其中 `r`、`g`、`b` 为0~255之间的整数。这些宏现在内部调用了运行时辅助函数，因此可以使用变量而非字面常量传递颜色值。

关于 ANSI 转义码的更多信息，请参考：[ANSI escape code - Handwiki](https://handwiki.org/wiki/ANSI_escape_code)

## 常见问题与排查

- 串口无输出或者一坨乱码：
  - 确认串口已正确初始化（**特别是波特率**）。
  - 如果启用颜色但终端不支持 ANSI，可以通过 `debug_setColorEnabled(false)` 关闭颜色。
- RA 平台 / TI MSPM0 平台 / 8051 时间戳不工作或显示为 `[00:00:00.000]`：
  - 确认已在你的 1ms 定时器 ISR 中调用前文所述 tick。
- RA 平台发送时程序卡死循环：
  - 确认在串口回调中调用了onTxComplete()。
- 异常重启
  - 可能是字符串溢出，检查`DEBUG_BUFFER_LEN`缓冲区长度（默认 256，8051 上默认 32）。如需更长信息，在 `ElegantDebug.h` 中调整此宏
- 8051 链接时提示 `*** ERROR L114: SEGMENT DOES NOT FIT, SPACE: DATA`
  - 内部缓冲区放不进 128 字节 DATA。保持 `DEBUG_BUFFER_LEN` 为 32，或者用 `#define DEBUG_8051_BUF_MEM idata`（8052，256 字节内部 RAM，链接时加 `RAMSIZE(256)`）/ `xdata`（带外部 RAM 的芯片）把缓冲区挪走。
- 8051 链接时提示 `*** ERROR L105`，提示某个日志函数从未被调用
  - Keil C51 会为变参函数保留固定的参数区。请调用你引入的每个日志函数，或者让链接器丢弃未使用的段（uVision：*Options for Target → L251 Misc → remove unused sections*，或 `LX51 ... REMOVEUNUSED`）。
- 8051 输出内容被截断
  - 这是有长度限制的格式化实现的正常行为；需要更长的消息请增大 `DEBUG_BUFFER_LEN`，并把缓冲区放到 `idata` / `xdata`。
- 8051 上 `%f` 打印出 `?`
  - `DEBUG_8051_ENABLE_FLOAT` 被设为 0；改回 1 即可（会多占约 2 KB 代码空间）。

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

### v1.7 (2026-11-13)

- **新增**: 支持 8051（Keil C51）平台（使用 `USE_8051_C51` 宏，仅 C 版本）
  - 通过用户提供的 `void (*)(char)` 函数逐字节输出，不需要任何厂商 UART HAL
  - 时间戳与 RA / MSPM0 平台共用 `_debug_tick_ms`，由 `debug_tick()` 喂入
  - 8051 上整个库以 `reentrant` 编译、所有日志函数共用一个消息缓冲区，因此 128 字节 RAM 的普通 8051 也能链接通过
  - `DEBUG_BUFFER_LEN` 默认 32；`DEBUG_8051_BUF_MEM` 可把缓冲区移到 `idata`/`xdata`；`DEBUG_8051_ENABLE_FLOAT` 可去掉 `%f` 以节省约 2 KB 代码空间
  - C51 没有 `snprintf()`/`vsnprintf()`，因此内置了一个有长度限制的格式化实现；平台选择机制扩展为四个平台
  - C90 限制：`debug_error()` / `debug_warning()` 无法自动追加 `__FILE__`/`__LINE__`，请使用 `debug_error_fileline(__FILE__, __LINE__, ...)`

## 其他

> 此库灵感最初源于学长Zodiak_Jealously的提议 ;p

WilliTourt 2025.12.10 initial release | willitourt@foxmail.com

欢迎提交 PR 和 Issue！！
