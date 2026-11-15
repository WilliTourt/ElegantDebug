#ifndef __ElegantDebug_H__
#define __ElegantDebug_H__

#include <REGX52.H>



/* 文字颜色 */
#define COLOR_DARK_RED      "\033[31m"
#define COLOR_DARK_GREEN    "\033[32m"
#define COLOR_DARK_YELLOW   "\033[33m"
#define COLOR_DARK_BLUE     "\033[34m"
#define COLOR_DARK_MAGENTA  "\033[35m"
#define COLOR_DARK_CYAN     "\033[36m"
#define COLOR_DARK_WHITE    "\033[37m"
#define COLOR_RED           "\033[91m"
#define COLOR_GREEN         "\033[92m"
#define COLOR_YELLOW        "\033[93m"
#define COLOR_BLUE          "\033[94m"
#define COLOR_MAGENTA       "\033[95m"
#define COLOR_CYAN          "\033[96m"
#define COLOR_WHITE         "\033[97m"
#define COLOR_CUSTOM(r,g,b) customTextColor((r), (g), (b))



/* 背景颜色 */
#define BG_RED              "\033[41m"
#define BG_GREEN            "\033[42m"
#define BG_YELLOW           "\033[43m"
#define BG_BLUE             "\033[44m"
#define BG_MAGENTA          "\033[45m"
#define BG_CYAN             "\033[46m"
#define BG_WHITE            "\033[47m"
#define BG_COLOR_CUSTOM(r,g,b) customBgColor((r), (g), (b))



/* 文字样式 */
#define BOLD                "\033[1m"
#define DIM                 "\033[2m"
#define ITALIC              "\033[3m"
#define UNDERLINE           "\033[4m"
#define BLINK               "\033[5m"
#define REVERSE             "\033[7m"
#define CONCEAL             "\033[8m"



/* 清除颜色和样式 */
#define CLR                 "\033[0m"
#define CLR_TEXT_COLOR      "\033[39m"
#define CLR_BG_COLOR        "\033[49m"
#define CLR_BOLD            "\033[21m"
#define CLR_DIM             "\033[22m"
#define CLR_ITALIC          "\033[23m"
#define CLR_UNDERLINE       "\033[24m"
#define CLR_BLINK           "\033[25m"
#define CLR_REVERSE         "\033[27m"
#define CLR_CONCEAL         "\033[28m"



/* 日志级别前缀 */
#define ERROR_TYPE          "\033[91m\033[1m[ERROR]\033[0m "
#define WARNING_TYPE        "\033[93m\033[1m[WARNING]\033[0m "
#define INFO_TYPE           "\033[94m\033[1m[INFO]\033[0m "
#define OK_TYPE             "\033[92m\033[1m[OK]\033[0m "
#define SUCCESS_TYPE        "\033[92m\033[1m[SUCCESS]\033[0m "

#define ERROR_TYPE_PLAIN    "[ERROR] "
#define WARNING_TYPE_PLAIN  "[WARNING] "
#define INFO_TYPE_PLAIN     "[INFO] "
#define OK_TYPE_PLAIN       "[OK] "
#define SUCCESS_TYPE_PLAIN  "[SUCCESS] "



/* 在 1ms 定时器中断中调用 debug_tick()，不要在中断中调用日志函数。 */
extern volatile unsigned long _debug_tick_ms;
#define debug_tick()        (_debug_tick_ms++)



/* 初始化函数 */
void debug_init(bit enable_timestamp, bit enable_color, bit enable_filename_line);
void Debug_init2D(unsigned char baud);
void Timer0_Init(void);



/* 底层串口与数值发送函数 */
void Uart_SendByte(unsigned char dat);
void Uart_SendUInt(unsigned int Num);
void Uart_SendInt(int Num);
void Uart_SendULong(unsigned long Num);
void Uart_SendLong(long Num);
void Uart_SendString(const char *str);



/*
   公共日志函数，顺序与原 ElegantDebug.c 一致。
   支持：%d %i %u %ld %lu %c %s %x %X %lx %lX %f %%
   不支持：宽度、精度、%o %p %e %g 以及 + 号标志。
   %f 固定输出 6 位小数。
   与原库一样，函数不会自动换行，需要换行时请在格式字符串中写 \r\n。
*/
void debug_log(const char *format, ...);
void debug_logWithType(const char *type, const char *style, const char *format, ...);
void debug_error_fileline(const char *file, int line, const char *format, ...);
void debug_warning_fileline(const char *file, int line, const char *format, ...);
void debug_ok(const char *format, ...);
void debug_success(const char *format, ...);
void debug_info(const char *format, ...);

/*
   Keil C51 不支持可变参数宏，因此不提供 debug_error(...) 和
   debug_warning(...) 宏。ERROR/WARNING 请直接这样调用：

   debug_error_fileline(__FILE__, __LINE__, "error=%d\r\n", value);
   debug_warning_fileline(__FILE__, __LINE__, "warning=%d\r\n", value);

   只有 ERROR/WARNING 使用文件名和行号；INFO/OK/SUCCESS 不使用。
*/



/* 运行时开关 */
void debug_setTimestampEnabled(bit enabled);
void debug_setColorEnabled(bit enabled);
void debug_setFilenameLineEnabled(bit enabled);



/* 运行时 24 位 RGB 颜色；返回内部静态 ANSI 字符串。 */
const char *customTextColor(unsigned char r, unsigned char g, unsigned char b);
const char *customBgColor(unsigned char r, unsigned char g, unsigned char b);

#endif
