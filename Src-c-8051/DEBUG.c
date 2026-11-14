#include <REGX52.H>
#include "DEBUG.h"

volatile unsigned long int tick_ms = 0;

bit _timestamp_enabled = 1;
bit _color_enabled = 1;
bit _filename_line_enabled = 0;

char putchar(char c)//printf重定向
{
if(c=='\n')
{
Uart_SendByte('\r');
}
Uart_SendByte(c);
return c;
}

//@brief 初始化串口寄存器,无中断
//@param 传入波特率重装值，实际波特率为传入的两倍
//@retval 无
void DEBUG_init2D(unsigned char baud)//传入波特率重装值，实际波特率是传入值的两倍;无中断
{
	TMOD|=0X20;	//设置计数器工作方式2：0020 0000，八位自动重装载定时器，第6位为0，用作定时器，第七位置0，定时器的开关由TR1控制，TR1=1时开启
	SCON=0X50;	//设置为工作方式1 ：0101 0000 前两位(高)01->方式1 8位UART，波特率可变，左起第4位置1，开启REN，允许串行接收器RXD接收信息，置0时禁止接收
	PCON=0X80;	//波特率加倍 1000 0000 SMOD位置1，使串行通信方式1、2、3的波特率加倍
	TH1=baud;	//计数器初始值设置，可自由设置
	TL1=baud;
	ES=0;		//关闭接收中断，中断允许控制寄存器置0，不允许中断
	EA=0;		//关闭总中断
	TR1=1;		//打开计数器		
}


//@brief 单片机向电脑发送8位二进制数
//@param 8位二进制数
//@retval 无
void Uart_SendByte(unsigned char dat)//单片机将8位二进制数据发送到电脑端
{
    SBUF = dat;//单片机将dat发送到SBUF缓存区
    while(!TI); // 等待发送完成，再退出该函数:一开始TI初值为0，每完成1帧的发送，会将中断请求位置1，向主机请求中断处理
    TI=0;
}
void Uart_SendUInt(unsigned int Num)//发送给电脑
{
    unsigned char buf[5]; // 65535最大5位，开5个字节
    unsigned char i = 0;
    unsigned char j;

    // 1. 特殊情况：如果是 0，直接发 '0'
    if (Num == 0) {
        Uart_SendByte('0');
        return;
    }

    // 2. 从个位开始提取，存入 buf
    while (Num > 0) 
    {
        buf[i] = (Num % 10) + '0'; // 取余数并转成 ASCII 字符
        Num = Num / 10;            // 砍掉最后一位
        i++;
    }

    // 3. 倒序输出（因为刚才存的时候是个位在前，高位在后）
    for (j = i; j > 0; j--) 
    {
        Uart_SendByte(buf[j - 1]);
    }
}

//@brief 初始化串口寄存器,有中断
//@param 传入波特率重装值，实际波特率为传入的两倍
//@retval 无
void uart_init2D_Rupt(unsigned char baud)//传入波特率重装值，实际波特率是传入值的两倍;有中断
{
	TMOD|=0X20;	//设置计数器工作方式2：0020 0000，八位自动重装载定时器，第6位为0，用作定时器，第七位置0，定时器的开关由TR1控制，TR1=1时开启
	SCON=0X50;	//设置为工作方式1 ：0101 0000 前两位(高)01->方式1 8位UART，波特率可变，左起第4位置1，开启REN，允许串行接收器RXD接收信息，置0时禁止接收
	PCON=0X80;	//波特率加倍 1000 0000 SMOD位置1，使串行通信方式1、2、3的波特率加倍
	TH1=baud;	//计数器初始值设置，可自由设置
	TL1=baud;
	ES=1;		//开启接收中断，中断允许控制寄存器置0，允许中断
	EA=1;		//开启总中断
	TR1=1;		//打开计数器		
}


//中断函数模板
// void UART_2D() interrupt 4 //串口通信中断函数
// {
// 	if(RI==1)//说明单片机已经接收到了来自电脑的一次数据
// 	{
// 		//code
// 		RI=0;//接收中断标志清零
// 	}

// 	if(TI==1)//说明单片机已经向电脑发送了一次数据
// 	{
// 		//code
// 		TI=0;//发送中断标志清0
// 	}
// }


void debug_init(bit enable_timestamp,  bit enable_color, bit enable_filename_line)//初始化
 {
    _timestamp_enabled = enable_timestamp;
    _color_enabled = enable_color;
    _filename_line_enabled = enable_filename_line;
}

void debug_setTimestampEnabled(bit enabled) {
    _timestamp_enabled = enabled;
}

void debug_setColorEnabled(bit enabled) {
    _color_enabled = enabled;
}

void debug_setFilenameLineEnabled(bit enabled) {
    _filename_line_enabled = enabled;
}

void Uart_SendInt(int Num)//int 类型
{
    unsigned int magnitude;

    if (Num < 0)
    {
        Uart_SendByte('-');

        /*
           先加1再取负，可以正确处理最小的int：-32768。
           最后再加回1，得到真正的绝对值。
        */
        magnitude = (unsigned int)(-(Num + 1));
        magnitude++;
    }
    else
    {
        magnitude = (unsigned int)Num;
    }

    Uart_SendUInt(magnitude);
}

void Uart_SendULong(unsigned long Num)//unsigned long 类型
{
    unsigned char buf[10];  /* 4294967295最多10位 */
    unsigned char i = 0;
    unsigned char j;

    if (Num == 0)
    {
        Uart_SendByte('0');
        return;
    }

    /* 从个位开始取出每一位 */
    while (Num > 0)
    {
        buf[i] = (unsigned char)(Num % 10UL) + '0';
        Num = Num / 10UL;
        i++;
    }

    /* 倒序发送 */
    for (j = i; j > 0; j--)
    {
        Uart_SendByte(buf[j - 1]);
    }
}


void Uart_SendString(char *str) 
{
    // 安全检查：如果是空指针，直接退出
    if (str == 0) return;

    // 只要没遇到 '\0'，就一直发
    while (*str != '\0') 
    {
        Uart_SendByte(*str); // 调用你之前写好的单字节发送函数
        str++;               // 指针后移，指向下一个字符
    }
}

void Debug_logWithType(char *type, char *str)
{
    if (_timestamp_enabled)
    {
        Send_tick();
    }

    Uart_SendByte('[');
    Uart_SendString(type);
    Uart_SendString("] ");

    Uart_SendString(str);
    Uart_SendString("\r\n");
}


void Debug_error_fileline(const char *file,int line,char *str)
{	
	if(_timestamp_enabled)
	{
        Send_tick();
	}
	if (_color_enabled) 
		Uart_SendString(ERROR_TYPE);
	else 
   		Uart_SendString(ERROR_TYPE_PLAIN);
	
	
	
	if(_filename_line_enabled)
	{   
        Uart_SendString("[");
        Uart_SendString((char*)file);
		Uart_SendString(":");
		Uart_SendUInt(line);
        Uart_SendString("] ");
	}
	
	Uart_SendString(str);
	Uart_SendString("\r\n");
}

void Debug_log_fileline(char *str)
{
    if (_timestamp_enabled)
    {
        Send_tick();
    }
	Uart_SendString(str);
	Uart_SendString("\r\n");
}

void Debug_info_fileline(char *str)
{	
	if(_timestamp_enabled)
	{
     Send_tick();
	}
	if (_color_enabled) 
		Uart_SendString(INFO_TYPE);
	else 
   		Uart_SendString(INFO_TYPE_PLAIN);
	Uart_SendString(str);
	Uart_SendString("\r\n");
}

void Send_tick(void)
{
    unsigned char ea_backup;
    unsigned long now;
    unsigned long total_s;
    unsigned int h;
    unsigned int m;
    unsigned int s;
    unsigned int ms;


    ea_backup = EA;
    EA = 0;
    now = tick_ms;
    EA = ea_backup;

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

void Debug_ok_fileline(char *str)
{
	if(_timestamp_enabled)
	{
     Send_tick();
	}
	if (_color_enabled) 
		Uart_SendString(OK_TYPE);
	else 
   		Uart_SendString(OK_TYPE_PLAIN);
	Uart_SendString(str);
	Uart_SendString("\r\n");
}

void Debug_success_fileline(char *str)
{
	if(_timestamp_enabled)
	{
     Send_tick();
	}
	if (_color_enabled) 
		Uart_SendString(SUCCESS_TYPE);
	else 
   		Uart_SendString(SUCCESS_TYPE_PLAIN);
	Uart_SendString(str);
	Uart_SendString("\r\n");
}

void Debug_warning_fileline(const char *file,int line,char *str)
{
	if(_timestamp_enabled)
	{
     Send_tick();
	}
	if (_color_enabled) 
		Uart_SendString(WARNING_TYPE);
	else 
   		Uart_SendString(WARNING_TYPE_PLAIN);
	
	if(_filename_line_enabled)
	{   
        Uart_SendString("[");
	    Uart_SendString((char*)file);
		Uart_SendString(":");
		Uart_SendUInt(line);
        Uart_SendString("] ");
	}
	
	Uart_SendString(str);
	Uart_SendString("\r\n");
}

static void Debug_infoBegin(char *text)
{
    if (_timestamp_enabled)
    {
        Send_tick();
    }

    if (_color_enabled)
        Uart_SendString(INFO_TYPE);
    else
        Uart_SendString(INFO_TYPE_PLAIN);

    Uart_SendString(text);
}

void Debug_infoValue(char *text, unsigned int value)
{
    Debug_infoBegin(text);
    Uart_SendUInt(value);
    Uart_SendString("\r\n");
}

void Debug_infoChar(char *text, char value)
{
    Debug_infoBegin(text);
    Uart_SendByte(value);
    Uart_SendString("\r\n");
}

void Debug_infoString(char *text, char *value)
{
    Debug_infoBegin(text);
    Uart_SendString(value);
    Uart_SendString("\r\n");
}

void Debug_infoFloat2(char *text, float value)
{
    unsigned int integer_part;
    unsigned int decimal_part;

    Debug_infoBegin(text);

    if (value < 0)
    {
        Uart_SendByte('-');
        value = -value;
    }

    integer_part = (unsigned int)value;

    decimal_part = (unsigned int)(
        (value - (float)integer_part) * 100.0f + 0.5f
    );

    /*
       四舍五入后，小数部分可能变成100，
       例如25.999会变成26.00
    */
    if (decimal_part >= 100)
    {
        integer_part++;
        decimal_part = 0;
    }

    Uart_SendUInt(integer_part);
    Uart_SendByte('.');

    /* 小数部分不足两位时补零 */
    if (decimal_part < 10)
    {
        Uart_SendByte('0');
    }

    Uart_SendUInt(decimal_part);
    Uart_SendString("\r\n");
}

void debug_customTextColor(unsigned char r,unsigned char g,unsigned char b,char *str)
{
    if (_timestamp_enabled)
    {
        Send_tick();
    }

    if (_color_enabled)
    {
        Ansi_TextColor(r, g, b);
    }

    Uart_SendString(str);

    if (_color_enabled)
    {
        Uart_SendString("\033[0m");
    }

    Uart_SendString("\r\n");
}

void Ansi_TextColor(unsigned char r,unsigned char g,unsigned char b)
{
    if (!_color_enabled) return;

    Uart_SendString("\033[38;2;");
    Uart_SendUInt(r);
    Uart_SendByte(';');
    Uart_SendUInt(g);
    Uart_SendByte(';');
    Uart_SendUInt(b);
    Uart_SendByte('m');
}
//计时器函数，与中断函数，用于计时
void Timer0_Init(void)		//1毫秒@11.0592MHz
{
	
	TMOD &= 0xF0;			//设置定时器模式
	TMOD |= 0x01;			//设置定时器模式
	
	TL0 = 0x66;				//设置定时初始值0100 0110
	TH0 = 0xFC;				//设置定时初始值1111 1100
							//初始值即1111 1100 0100 0110=64528（10），计数到65535时溢出
	TF0 = 0;				//清除TF0标志，再计数器值溢出时，会自动置1，如果TF0置1，则会立马执行中断
	TR0 = 1;				//定时器0开始计时
	
	
	ET0 = 1;				//使能定时器0中断,
	EA=1;					//中断允许控制寄存器导通
	PT0=0;					//中断优先级控制寄存器导通
}

//中断程序函数，用于tick_ms计时
/*
void Timer0_Isr(void) interrupt 1
{   
	static unsigned int T0_counter;
	TL0 = 0x66;			//设置定时初始值
	TH0 = 0xFC;			//设置定时初始值
	T0_counter++;
	if(T0_counter>=1000)
	{
	//     code       
	T0_counter=0;}
}
*/