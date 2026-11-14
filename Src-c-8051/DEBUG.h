#ifndef __DEBUG_H_
#define __DEBUG_H_
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

#define Debug_log(str)  Debug_log_fileline( str)
#define Debug_info(str)  Debug_info_fileline( str)
#define Debug_ok(str)  Debug_ok_fileline( str)
#define Debug_success(str)  Debug_success_fileline( str)
#define Debug_warning(str)  Debug_warning_fileline(__FILE__, __LINE__, str)
#define Debug_error(str)  Debug_error_fileline(__FILE__, __LINE__, str)



//
extern volatile unsigned long int tick_ms;


extern bit _timestamp_enabled;
extern bit _color_enabled;
extern bit _filename_line_enabled;


void DEBUG_init2D(unsigned char baud);//传入波特率重装值，实际波特率是传入值的两倍，无中断
void Uart_SendByte(unsigned char dat);//单片机将8位二进制数据发送到电脑端
void uart_init2D_Rupt(unsigned char baud);//传入波特率重装值，实际波特率是传入值的两倍;有中断
void Timer0_Init(void);		//1毫秒@11.0592MHz
void Uart_SendUInt(unsigned int Num);//发送给电脑
void Uart_SendString(char *str);

void debug_init(bit enable_timestamp,  bit enable_color, bit enable_filename_line);//初始化
void debug_setTimestampEnabled(bit enabled);
void debug_setColorEnabled(bit enabled);
void debug_setFilenameLineEnabled(bit enabled);

void Send_tick(void);

void Debug_error_fileline(const char *file,int line,char *str);
void Debug_log_fileline(char *str);
void Debug_info_fileline(char *str);
void Debug_ok_fileline(char *str);
void Debug_success_fileline(char *str);
void Debug_warning_fileline(const char *file,int line,char *str);
void Debug_logWithType(char *type, char *str);
void debug_customTextColor(unsigned char r,unsigned char g,unsigned char b,char *str);
void Ansi_TextColor(unsigned char r,unsigned char g,unsigned char b);
void Debug_infoValue(char *text, unsigned int value);//unsigned 类型
void Debug_infoChar(char *text, char value);//char 类型
void Debug_infoString(char *text, char *value);//
void Debug_infoFloat2(char *text, float value);
void Uart_SendInt(int Num);//int 类型
void Uart_SendULong(unsigned long Num);//unsigned long 类型



#endif