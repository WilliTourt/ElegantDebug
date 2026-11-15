/*******************************************************************************
 * @file    ElegantDebug.c
 * @version 1.0
 * @brief   8051 (STC89C52RC) 串口调试库实现，带 ANSI 颜色、时间戳。
 *
 * 特点：
 *   - 所有日志统一经过 _send() 发送
 *   - 直接操作 SBUF，不使用 printf / vprintf / sprintf，不拼长字符串
 *   - 轻量格式化：%d %u %ld %lu %c %s %x %X %lx %lX %f %%
 *   - 不解析宽度和精度（%5d、%.2f 这类写法不支持）
 *   - 可选择时间戳、颜色、ERROR/WARNING 的文件名行号
 *
 * 使用前必须先调用 Debug_init2D() 初始化串口，
 * 并调用 Timer0_Init() 提供 1ms 时间基准。
 ******************************************************************************/

#include <REGX52.H>
#include "ElegantDebug.h"
#include <STDARG.H>

/* ============================== 全局状态 ============================== */

volatile unsigned long _debug_tick_ms = 0;

static bit _timestamp_enabled = 1;
static bit _color_enabled = 1;
static bit _filename_line_enabled = 0;
static bit _uart_ready = 0;

/* ============================== 初始化 ============================== */

/* 设置三个运行时开关，默认：时间戳开、颜色开、文件行号关 */
void debug_init(bit enable_timestamp, bit enable_color, bit enable_filename_line)
{
    _timestamp_enabled = enable_timestamp;
    _color_enabled = enable_color;
    _filename_line_enabled = enable_filename_line;
}

/* 串口初始化，工作方式1，8位UART，波特率加倍，阻塞发送，不使用串口中断 */
void Debug_init2D(unsigned char baud)
{
    TMOD |= 0x20;   /* 定时器1，工作方式2，8位自动重装 */
    SCON = 0x50;    /* 方式1，允许接收 */
    PCON = 0x80;    /* SMOD=1，波特率加倍 */
    TH1 = baud;
    TL1 = baud;
    ES = 0;         /* 不用串口中断 */
    TR1 = 1;
    _uart_ready = 1;
}

/* Timer0 初始化，1ms @ 11.0592MHz，中断函数由用户自己实现 */
void Timer0_Init(void)
{
    TMOD &= 0xF0;
    TMOD |= 0x01;

    TL0 = 0x66;
    TH0 = 0xFC;
    TF0 = 0;
    TR0 = 1;

    ET0 = 1;
    EA = 1;
    PT0 = 0;
}

/* ============================== 底层发送 ============================== */

/* 阻塞发送一个字节。串口未初始化时直接返回，避免死等 TI */
void Uart_SendByte(unsigned char dat)
{
    if (!_uart_ready) return;

    SBUF = dat;
    while (!TI);
    TI = 0;
}

/* 发送无符号 16 位整数，0 ~ 65535 */
void Uart_SendUInt(unsigned int Num)
{
    unsigned char idata buf[5];
    unsigned char i = 0;

    if (Num == 0)
    {
        Uart_SendByte('0');
        return;
    }

    while (Num > 0)
    {
        buf[i] = (unsigned char)(Num % 10) + '0';
        Num = Num / 10;
        i++;
    }

    while (i > 0)
    {
        Uart_SendByte(buf[--i]);
    }
}

/* 发送有符号 16 位整数，-32768 ~ 32767 */
void Uart_SendInt(int Num)
{
    unsigned int magnitude;

    if (Num < 0)
    {
        Uart_SendByte('-');
        /* 先加 1 再取负，避免 -32768 溢出 */
        magnitude = (unsigned int)(-(Num + 1));
        magnitude++;
    }
    else
    {
        magnitude = (unsigned int)Num;
    }

    Uart_SendUInt(magnitude);
}

/* 发送无符号 32 位整数，0 ~ 4294967295 */
void Uart_SendULong(unsigned long Num)
{
    unsigned char idata buf[10];
    unsigned char i = 0;

    if (Num == 0)
    {
        Uart_SendByte('0');
        return;
    }

    while (Num > 0)
    {
        buf[i] = (unsigned char)(Num % 10UL) + '0';
        Num = Num / 10UL;
        i++;
    }

    while (i > 0)
    {
        Uart_SendByte(buf[--i]);
    }
}

/* 发送有符号 32 位整数，-2147483648 ~ 2147483647 */
void Uart_SendLong(long Num)
{
    unsigned long magnitude;
    unsigned char negative = 0;

    if (Num < 0)
    {
        negative = 1;
        magnitude = (unsigned long)(-(Num + 1));
        magnitude++;
    }
    else
    {
        magnitude = (unsigned long)Num;
    }

    if (negative)
    {
        Uart_SendByte('-');
    }

    Uart_SendULong(magnitude);
}

/* 发送以 '\0' 结尾的字符串 */
void Uart_SendString(const char *str)
{
    if (str == 0) return;

    while (*str != '\0')
    {
        Uart_SendByte(*str);
        str++;
    }
}

/* ============================== 内部辅助 ============================== */

/* 取毫秒计数。32 位变量在 8 位机上不是原子读，先关总中断再恢复 */
static unsigned long _getTick(void)
{
    unsigned char ea_backup;
    unsigned long now;

    ea_backup = EA;
    EA = 0;
    now = _debug_tick_ms;
    EA = ea_backup;

    return now;
}

/* 发送时间戳，格式 [HH:MM:SS.mmm] ，末尾带一个空格 */
static void _send_timestamp(void)
{
    unsigned long now;
    unsigned long total_s;
    unsigned int h;
    unsigned int m;
    unsigned int s;
    unsigned int ms;

    now = _getTick();

    total_s = now / 1000UL;
    h  = (unsigned int)(total_s / 3600UL);
    m  = (unsigned int)((total_s % 3600UL) / 60UL);
    s  = (unsigned int)(total_s % 60UL);
    ms = (unsigned int)(now % 1000UL);

    Uart_SendByte('[');

    if (h < 10) Uart_SendByte('0');
    Uart_SendUInt(h);
    Uart_SendByte(':');

    if (m < 10) Uart_SendByte('0');
    Uart_SendUInt(m);
    Uart_SendByte(':');

    if (s < 10) Uart_SendByte('0');
    Uart_SendUInt(s);
    Uart_SendByte('.');

    if (ms < 100) Uart_SendByte('0');
    if (ms < 10)  Uart_SendByte('0');
    Uart_SendUInt(ms);

    Uart_SendString("] ");
}

/* 发送 [文件名:行号] 前缀 */
static void _send_fileline(const char *file, int line)
{
    if (file == 0) return;

    Uart_SendByte('[');
    Uart_SendString(file);
    Uart_SendByte(':');
    Uart_SendUInt((unsigned int)line);
    Uart_SendString("] ");
}

/* 发送十六进制，不加前导 0，uppercase 控制 A-F 大小写 */
static void _sendHex(unsigned long value, unsigned char uppercase)
{
    unsigned char idata buf[8];
    unsigned char i = 0;
    unsigned char digit;

    if (value == 0)
    {
        Uart_SendByte('0');
        return;
    }

    while (value > 0)
    {
        digit = (unsigned char)(value & 0x0F);

        if (digit < 10)
        {
            buf[i] = (unsigned char)('0' + digit);
        }
        else
        {
            buf[i] = (unsigned char)((uppercase ? 'A' : 'a') + (digit - 10));
        }

        i++;
        value >>= 4;
    }

    while (i > 0)
    {
        Uart_SendByte(buf[--i]);
    }
}

/* 发送浮点数，固定 decimals 位小数，四舍五入 */
static void _sendFloat(double value, unsigned char decimals)
{
    unsigned char i;
    unsigned char negative = 0;
    unsigned long scale = 1UL;
    unsigned long scaled;
    unsigned long integer_part;
    unsigned long frac_part;

    if (value < 0)
    {
        negative = 1;
        value = -value;
    }

    for (i = 0; i < decimals; i++)
    {
        scale = scale * 10UL;
    }

    scaled = (unsigned long)(value * (double)scale + 0.5);

    integer_part = scaled / scale;
    frac_part = scaled % scale;

    if (negative)
    {
        Uart_SendByte('-');
    }

    Uart_SendULong(integer_part);

    if (decimals == 0) return;

    Uart_SendByte('.');

    /* 小数部分不足位时补 0 */
    while (frac_part < scale / 10UL)
    {
        Uart_SendByte('0');
        scale = scale / 10UL;
        if (scale == 1UL) break;
    }

    Uart_SendULong(frac_part);
}

/* ============================== 轻量格式化 ============================== */

/*
   支持：%d %i %u %ld %lu %c %s %x %X %lx %lX %f %%
   %f 固定输出 6 位小数。
   不支持宽度、精度、%o、%p、%e、%g 等写法。
*/
static void _vprint(const char *format, va_list args)
{
    char specifier;
    char *text;
    unsigned char long_mode;

    if (format == 0) return;

    while (*format != '\0')
    {
        if (*format != '%')
        {
            Uart_SendByte(*format);
            format++;
            continue;
        }

        format++;

        if (*format == '%')
        {
            Uart_SendByte('%');
            format++;
            continue;
        }

        long_mode = 0;
        if (*format == 'l')
        {
            long_mode = 1;
            format++;
        }

        specifier = *format;

        switch (specifier)
        {
            case 'd':
            case 'i':
                if (long_mode)
                {
                    Uart_SendLong(va_arg(args, long));
                }
                else
                {
                    Uart_SendInt(va_arg(args, int));
                }
                break;

            case 'u':
                if (long_mode)
                {
                    Uart_SendULong(va_arg(args, unsigned long));
                }
                else
                {
                    Uart_SendUInt(va_arg(args, unsigned int));
                }
                break;

            case 'x':
            case 'X':
                if (long_mode)
                {
                    _sendHex(va_arg(args, unsigned long), specifier == 'X');
                }
                else
                {
                    _sendHex((unsigned long)va_arg(args, unsigned int),
                             specifier == 'X');
                }
                break;

            case 'c':
                Uart_SendByte((unsigned char)va_arg(args, int));
                break;

            case 's':
                text = va_arg(args, char *);
                Uart_SendString(text);
                break;

            case 'f':
                _sendFloat(va_arg(args, double), 6);
                break;

            default:
                Uart_SendByte('%');
                if (long_mode) Uart_SendByte('l');
                Uart_SendByte(specifier);
                break;
        }

        if (*format != '\0') format++;
    }
}

/* ============================== 统一发送入口 ============================== */

/*
   所有日志函数最终都调用这里。
   顺序：时间戳 -> 级别前缀（或自定义标签） -> 文件名行号 -> 正文
*/
static void _send(const char *type,
                  const char *style,
                  unsigned char custom_type,
                  const char *file,
                  int line,
                  const char *format,
                  va_list args)
{
    unsigned char ct = custom_type;

    if (_timestamp_enabled)
    {
        _send_timestamp();
    }

    if (ct)
    {
        /* 自定义标签：[TYPE] ，颜色由 style 控制 */
        if (_color_enabled)
        {
            Uart_SendString(BOLD);
            if (style != 0) Uart_SendString(style);
        }

        Uart_SendByte('[');
        if (type != 0) Uart_SendString(type);
        Uart_SendString("] ");

        if (_color_enabled)
        {
            Uart_SendString(CLR);
        }
    }
    else
    {
        if (type != 0) Uart_SendString(type);
    }

    if (file != 0 && _filename_line_enabled)
    {
        _send_fileline(file, line);
    }

    _vprint(format, args);
}

/* ============================== 公共日志函数 ============================== */

/* 无前缀日志 */
void debug_log(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    _send(0, 0, 0, 0, 0, format, args);
    va_end(args);
}

/* 自定义标签和颜色。style 传 COLOR_xxx，type 传标签文字 */
void debug_logWithType(const char *type, const char *style,
                       const char *format, ...)
{
    va_list args;

    va_start(args, format);
    _send(type, style, 1, 0, 0, format, args);
    va_end(args);
}

/* ERROR，支持文件名和行号 */
void debug_error_fileline(const char *file, int line,
                          const char *format, ...)
{
    va_list args;

    va_start(args, format);
    _send(_color_enabled ? ERROR_TYPE : ERROR_TYPE_PLAIN,
          0, 0, file, line, format, args);
    va_end(args);
}

/* WARNING，支持文件名和行号 */
void debug_warning_fileline(const char *file, int line,
                            const char *format, ...)
{
    va_list args;

    va_start(args, format);
    _send(_color_enabled ? WARNING_TYPE : WARNING_TYPE_PLAIN,
          0, 0, file, line, format, args);
    va_end(args);
}

/* OK，不带文件名行号 */
void debug_ok(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    _send(_color_enabled ? OK_TYPE : OK_TYPE_PLAIN, 0, 0, 0, 0, format, args);
    va_end(args);
}

/* SUCCESS，不带文件名行号 */
void debug_success(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    _send(_color_enabled ? SUCCESS_TYPE : SUCCESS_TYPE_PLAIN,
          0, 0, 0, 0, format, args);
    va_end(args);
}

/* INFO，不带文件名行号 */
void debug_info(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    _send(_color_enabled ? INFO_TYPE : INFO_TYPE_PLAIN,
          0, 0, 0, 0, format, args);
    va_end(args);
}

/* ============================== 运行时开关 ============================== */

void debug_setTimestampEnabled(bit enabled)
{
    _timestamp_enabled = enabled;
}

void debug_setColorEnabled(bit enabled)
{
    _color_enabled = enabled;
}

void debug_setFilenameLineEnabled(bit enabled)
{
    _filename_line_enabled = enabled;
}

/* ============================== 24 位自定义颜色 ============================== */

/* 把 0~255 写成十进制，返回新的写入位置 */
static char *_write_u8(char *dst, unsigned char value)
{
    unsigned char idata buf[3];
    unsigned char i = 0;

    if (value == 0)
    {
        *dst++ = '0';
        return dst;
    }

    while (value > 0)
    {
        buf[i] = (unsigned char)(value % 10) + '0';
        value = value / 10;
        i++;
    }

    while (i > 0)
    {
        *dst++ = buf[--i];
    }

    return dst;
}

/* 构造 ANSI 真彩色字符串，例如 ESC[38;2;255;100;0m */
static void _build_ansi_color(char *buf, unsigned char background,
                              unsigned char r, unsigned char g, unsigned char b)
{
    char *p = buf;

    *p++ = '\033';
    *p++ = '[';
    *p++ = background ? '4' : '3';
    *p++ = '8';
    *p++ = ';';
    *p++ = '2';
    *p++ = ';';

    p = _write_u8(p, r);
    *p++ = ';';
    p = _write_u8(p, g);
    *p++ = ';';
    p = _write_u8(p, b);
    *p++ = 'm';
    *p = '\0';
}

/*
   返回静态缓冲区的 ANSI 字符串，可以直接用 %s 拼进日志。
   注意：下一次调用会覆盖同一块缓冲区。
*/
const char *customTextColor(unsigned char r, unsigned char g, unsigned char b)
{
    static char idata ansi[24];

    _build_ansi_color(ansi, 0, r, g, b);
    return ansi;
}

const char *customBgColor(unsigned char r, unsigned char g, unsigned char b)
{
    static char idata ansi[24];

    _build_ansi_color(ansi, 1, r, g, b);
    return ansi;
}

/* ============================== 中断模板 ============================== */

/*
串口中断模板（接收电脑发来的调试命令时使用）。
默认注释掉，需要时取消注释，并在主程序里把 ES 置 1。

void UART_Isr(void) interrupt 4
{
    if (RI == 1)
    {
        // 在这里读取 SBUF 处理接收到的字节
        RI = 0;
    }

    if (TI == 1)
    {
        TI = 0;
    }
}
*/

/*
Timer0 中断服务函数，每 1ms 进入一次。
作用：为调试库提供时间基准，时间戳能走起来全靠 debug_tick()。
TL0/TH0 重新装载初值，0xFC66 = 64614，计到 65536 溢出即 1ms。
注意：中断里不要调用 debug_info 这类日志函数，只做自增。

void Timer0_Isr(void) interrupt 1
{
    TL0 = 0x66;
    TH0 = 0xFC;

    debug_tick();
}
*/
