# Elegant Debug for 8051 Core (STC89C52RC)

> **Language/语言**: [English](README.md) | [简体中文](README-zh-CN.md)

---

## 简介

轻量的串口调试库，适用于 8051 内核，提供格式化日志输出。

- 格式化日志
- 可选时间戳
- 可选 ANSI 颜色（含 24 位自定义颜色）
- 类型前缀（ERROR / WARNING / INFO / OK / SUCCESS）

![Example](Examples/8051/screenshot.png)

## 快速开始

```c
#include "ElegantDebug.h"

void T0_ISR(void) interrupt 1      /* 1ms 定时器中断 */
{
    TH0 = 0xFC;  TL0 = 0x66;       /* 11.0592MHz */
    debug_tick();                  /* 喂时间戳 */
}

void main(void)
{
    UART_Init();                   /* 9600 8N1, TH1 = 0xFD */
    Timer0_Init();
    debug_init(true, true, true);  /* 时间戳 / 颜色 / 文件名行号 */
    EA = 1;

    debug_info("System started. Free memory: %d bytes\r\n", free_mem);
    debug_error("Sensor failed: code = %d\r\n", -3);

    while (1);
}
```

> 上面这段是 Keil + STC-ISP 的接法。**若你使用其他工具链或芯片，可以删掉这一节**，
> 只保留「包含头文件 → 调用 debug_init → 在 1ms ISR 中调用 debug_tick」这三步即可。

## API

| 函数 | 说明 |
|---|---|
| `void debug_init(bool ts, bool color, bool fileline)` | 初始化，必须先调用 |
| `void debug_tick(void)` | 1ms 定时器 ISR 中调用（本分支实现为宏） |
| `void debug_log(fmt, ...)` | 无前缀输出 |
| `void debug_logWithType(type, style, fmt, ...)` | 自定义前缀和样式 |
| `void debug_info/ok/success(fmt, ...)` | 带前缀，无文件名行号 |
| `void debug_error/warning(fmt, ...)` | 带前缀，**无文件名行号** |
| `void debug_error/warning_fileline(file, line, fmt, ...)` | 带前缀和文件名行号 |
| `void debug_setTimestampEnabled/ColorEnabled/FilenameLineEnabled(bool)` | 运行时开关 |
| `const char* customTextColor/customBgColor(r, g, b)` | 生成 24 位颜色转义序列 |

## 与主分支的差异及原因

| 改动 | 主分支 | 本分支 | 为什么必须改 |
|---|---|---|---|
| 类型定义 | `#include <stdbool.h>` `<stdint.h>` | 自行 `typedef` | Keil C51 是 C89 编译器，**没有这两个头文件** |
| `debug_tick()` | `static inline` 函数 | 宏 | C51 **不支持 `inline`**（报 C129） |
| `debug_error/warning()` | 变参宏，自动传 `__FILE__`/`__LINE__` | 普通函数 | C51 **不支持变参宏** `#define f(...)`（报 C301/C304） |
| 缓冲区位置 | 各函数内的局部数组 | 模块级共享，放在 `xdata` | C51 **局部变量静态分配**，10 个函数各自的缓冲合计 442 字节，超过 128 字节内部 RAM |
| 输出方式 | 时间戳+前缀+正文先拼进 `combined` 再发 | 时间戳、前缀、正文**分段直接写 SBUF** | STC89C52RC 的 `xdata` 只有 **256 字节**，装不下 `msg` + `combined` + `out` 三个缓冲 |
| `DEBUG_BUFFER_LEN` | 256 | 48 | 同上（单条日志上限 47 字符） |
| 格式化函数 | `vsnprintf` / `snprintf` | `vsprintf` / `sprintf` | C51 的 `STDIO.H` **只有后者** |

## 新增的内容及原因

| 新增 | 为什么加 |
|---|---|
| `_putc()` / `_puts()` | 原文件在此处只留了 `// TODO HERE: Transmit data to SBUF`，需要补上逐字节发送 |
| `_timestamp()` | 原实现用 `sprintf` 拼时间戳，会把 `printf` 模块牵进调用链，8051 的栈不够；改为直接输出数字 |
| `_put_uint()` | `debug_error_fileline()` 显示 `[文件名:行号]` 时需要打印十进制行号 |
| `_send_type(prefix, text)` | 替代原来的 `combined` 拼接，省掉一个缓冲区 |
| `debug_error()` / `debug_warning()` 的函数体 | 头文件里已声明为函数（因为不能用变参宏），必须有实现 |
| `debug_init()` 的三个参数 | 8051 只有一个串口，`SBUF`/`TI` 是全局寄存器，不需要传入句柄 |

## 已知限制

- **单条日志最长 47 字符**（受 `DEBUG_BUFFER_LEN = 48` 限制）。
- **必须启用扩展 RAM**：`xdata` 缓冲区需要芯片的「内部扩展RAM」（STC89C52RC 为 256 字节）。
  烧录时勾选对应选项，或确认所用芯片具备 xdata。
- **`debug_error()` / `debug_warning()` 不带文件名行号**。需要时用
  `debug_error_fileline(__FILE__, __LINE__, ...)`。
- **整型常量作为可变参数时必须强制转换**：C51 未实现 C89 的默认参数提升，
  能装进 1 字节的常量会按 1 字节存储，而接收方按 16 位 `int` 读取。

  ```c
  debug_info("value = %d", 1);        /* 8051 上输出错误 */
  debug_info("value = %d", (int)1);   /* 正确 */
  ```

  传变量或强制转换表达式则正常。这是编译器行为，`printf`/`sprintf` 同样受影响。
- **不支持 `%f`**：会链入 C51 浮点库，约占 1.5KB Flash。

## 资源占用（STC89C52RC：8KB Flash / 128B 内部RAM / 256B 扩展RAM）

```
Program Size: data=80.1 xdata=72 code=3752
0 Error(s), 0 Warning(s)
```

## 示例

见 `Examples/8051/`：Keil 工程、演示程序、截图与烧录说明。

## 更新日志

### v1.6 - 8051 (2026-11-15)

- **新分支**：基于主分支，特用于 STC89C52RC 或其他 8051 内核单片机
- **实现**：补全 `debug_init()` 与 `_send()` 的 SBUF 发送逻辑
- **适配**：处理 C89 编译器与有限存储资源带来的限制，详见上文「与主分支的差异及原因」
- **新增**：`Examples/8051/` 示例工程

（v1.0 ~ v1.6 的历史记录请参见主分支。）

## 其他

> 此库灵感最初源于学长 Zodiak_Jealously 的提议 ;p

WilliTourt 2025.12.10 initial release | willitourt@foxmail.com

欢迎提交 PR 和 Issue！！
