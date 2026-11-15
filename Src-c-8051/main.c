#include <REGX52.H>
#include "ElegantDebug.h"
#include "delay_xms.h"

/* Timer0 中断：每 1ms 进入一次，为时间戳提供基准。
   中断里只做自增，不要调用 debug_info 这类日志函数。 */
void Timer0_Isr(void) interrupt 1
{
    TL0 = 0x66;
    TH0 = 0xFC;
    debug_tick();
}

void main(void)
{
    unsigned int  u16;
    int           i16;
    unsigned long u32;
    long          i32;
    unsigned char ea;

    u16 = 12345;
    i16 = -1234;
    u32 = 123456789UL;
    i32 = -123456789L;

    /* 1. 初始化：9600 @ 11.0592MHz，1ms 定时器，时间戳开、颜色开、文件行号关 */
    Debug_init2D(0xFA);
    Timer0_Init();
    debug_init(1, 1, 0);

    Uart_SendString("\r\n== ElegantDebug self test ==\r\n");

    /* 2. 单字节和字符串 */
    Uart_SendString("-- 1. byte / string --\r\n");
    Uart_SendByte('A');
    Uart_SendString("\r\n");
    Uart_SendString("direct string\r\n");

    /* 3. 四种数值发送函数 */
    Uart_SendString("-- 2. numbers --\r\n");
    Uart_SendString("u16 max = ");
    Uart_SendUInt(65535);
    Uart_SendString("\r\n");
    Uart_SendString("i16 neg = ");
    Uart_SendInt(-12345);
    Uart_SendString("\r\n");
    Uart_SendString("u32 max = ");
    Uart_SendULong(0xFFFFFFFFUL);
    Uart_SendString("\r\n");
    Uart_SendString("i32 neg = ");
    Uart_SendLong(-123456789L);
    Uart_SendString("\r\n");

    /* 4. 四个不带文件行号的日志函数 */
    Uart_SendString("-- 3. log / info / ok / success --\r\n");
    debug_log("debug_log: no prefix\r\n");
    debug_info("debug_info: blue INFO\r\n");
    debug_ok("debug_ok: green OK\r\n");
    debug_success("debug_success: green SUCCESS\r\n");

    /* 5. ERROR 和 WARNING，默认不带文件行号 */
    Uart_SendString("-- 4. error / warning --\r\n");
    debug_error_fileline(__FILE__, __LINE__, "debug_error: red ERROR\r\n");
    debug_warning_fileline(__FILE__, __LINE__, "debug_warning: yellow WARNING\r\n");

    /* 6. 打开文件名行号开关，再关掉 */
    Uart_SendString("-- 5. file line on --\r\n");
    debug_setFilenameLineEnabled(1);
    debug_error_fileline(__FILE__, __LINE__, "with file and line\r\n");
    debug_warning_fileline(__FILE__, __LINE__, "with file and line\r\n");
    debug_setFilenameLineEnabled(0);
    debug_error_fileline(__FILE__, __LINE__, "without file and line\r\n");

    /* 7. 自定义标签和颜色 */
    Uart_SendString("-- 6. custom tag --\r\n");
    debug_logWithType("SENSOR", COLOR_GREEN,   "sensor ok\r\n");
    debug_logWithType("MOTOR",  COLOR_YELLOW,  "motor ok\r\n");
    debug_logWithType("RFID",   COLOR_MAGENTA, "rfid ok\r\n");

    /* 8. 格式化占位符 */
    Uart_SendString("-- 7. format --\r\n");
    debug_info("d=%d\r\n", i16);
    debug_info("u=%u\r\n", u16);
    debug_info("ld=%ld\r\n", i32);
    debug_info("lu=%lu\r\n", u32);
    debug_info("c1=%c\r\n", 'O');
    debug_info("c2=%c\r\n", 'K');
    debug_info("s=%s\r\n", "string ok");
    debug_info("x=%x\r\n", 0xABCD);
    debug_info("X=%X\r\n", 0xABCD);
    debug_info("lx=%lx\r\n", 0x12345678UL);
    debug_info("lX=%lX\r\n", 0x12345678UL);
    debug_info("100%% done\r\n");
    debug_info("float=%f\r\n", 3.14);

    /* 9. 24 位自定义颜色 */
    Uart_SendString("-- 8. rgb color --\r\n");
    debug_info("%s", customTextColor(255, 60, 0));
    Uart_SendString("orange red text\r\n");
    Uart_SendString(CLR);

    debug_info("%s", customBgColor(0, 60, 120));
    Uart_SendString("blue background\r\n");
    Uart_SendString(CLR);

    /* 10. 运行时关闭颜色和关闭时间戳 */
    Uart_SendString("-- 9. color off / time off --\r\n");
    debug_setColorEnabled(0);
    debug_info("no ansi color\r\n");
    debug_setColorEnabled(1);

    debug_setTimestampEnabled(0);
    debug_info("no timestamp\r\n");
    debug_setTimestampEnabled(1);

    /* 11. 循环观察时间戳递增 */
    Uart_SendString("-- 10. loop --\r\n");
    while (1)
    {
        /* 32 位变量在 8 位机上一次读不完，读之前关总中断，读完恢复 */
        ea = EA;
        EA = 0;
        u32 = _debug_tick_ms;
        EA = ea;

        debug_info("running tick=%lu\r\n", u32);
        Delay1ms(1000);//此处需包含延时函数头文件，实参为1000代表延时1s
    }
}
