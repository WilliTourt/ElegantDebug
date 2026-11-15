#include <REGX52.H>
#include <stdio.h>
#include "ElegantDebug.h"


void Uart_Init(void)
{
	SCON = 0x50;
	TMOD &= 0x0F;
	TMOD |= 0x20;
	TH1 = TL1 = 0xFD;
	TR1 = 1;
}


char putchar(char c)
{
	SBUF = c;
	while (!TI);
	TI = 0;
	return c;
}


void Timer0_Init(void)   /* 1ms @ 11.0592MHz */
{
	TMOD &= 0xF0;
	TMOD |= 0x01;
	TH0 = 0xFC;
	TL0 = 0x66;
	ET0 = 1;
	TR0 = 1;
}

void Timer0_ISR(void) interrupt 1
{
	TH0 = 0xFC;
	TL0 = 0x66;
	debug_tick();       
}

void main(void)
{
	unsigned char err = 7;
	unsigned char val = 42;
	Uart_Init();
	Timer0_Init();
	EA = 1;


	debug_init(true, true, true);

	debug_log("ElegantDebug 8051 port demo");
	debug_info("UART initialized, tick=%lu", (unsigned long)_debug_tick_ms);
	debug_ok("init done");
	debug_success("all self tests passed");
	
	debug_warning_fileline(__FILE__, __LINE__, "value=%d is suspicious", (int)val);
	debug_error_fileline(__FILE__, __LINE__, "channel %u failed", (unsigned int)err);
	debug_logWithType("CUSTOM", COLOR_DARK_MAGENTA, "hello from 8051, %s", "colorful");
	debug_log("custom color test: %sred%s %sgreen%s",
	          COLOR_RED, CLR, COLOR_GREEN, CLR);
	debug_setColorEnabled(false);
	debug_log("colors disabled now");
	debug_setColorEnabled(true);
	debug_setTimestampEnabled(false);
	debug_log("timestamp disabled now");

	while (1);
}
