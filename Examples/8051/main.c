#include <reg52.h>
#include <stdio.h>

#include "ElegantDebug.h"

void UART_Init(void)
{
    TMOD &= 0x0F;
    TMOD |= 0x20;
    TH1 = 0xFD;
    TL1 = 0xFD;
    TR1 = 1;
    SCON = 0x50;
}

void Timer0_Init(void)
{
    TMOD &= 0xF0;
    TMOD |= 0x01;
    TH0 = 0xFC;
    TL0 = 0x66;
    ET0 = 1;
    TR0 = 1;
}

void T0_ISR(void) interrupt 1
{
    TH0 = 0xFC;
    TL0 = 0x66;
    debug_tick();
}


void main(void)
{
    UART_Init();
    Timer0_Init();
    debug_init(true, true, true);
    EA = 1;

    debug_log("plain %d", 1);
    debug_logWithType("TRACE", "\033[90m", "typed %d", 2);
    debug_info("info %d", 3);
    debug_ok("ok %d", 4);
    debug_success("success %d", 5);
    debug_error("error %d", -6);
    debug_warning("warn %d", 7);
    debug_error_fileline("main.c", 42, "efile %d", 8);
    debug_warning_fileline("main.c", 43, "wfile %d", 9);

    debug_setTimestampEnabled(true);
    debug_setColorEnabled(true);
    debug_setFilenameLineEnabled(false);

    debug_log("color %s", customTextColor(255, 128, 0));
    debug_log("bg %s", customBgColor(0, 0, 255));

    while (1);
}