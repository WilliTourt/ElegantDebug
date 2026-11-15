#include <reg52.h>
#include <stdio.h>

#include "ElegantDebug.h"

/* set by the UART ISR when any byte arrives from the PC */
volatile unsigned char g_rx_flag = 0;
static volatile unsigned char g_rx_dummy = 0;

void UART_Init(void)
{
    TMOD &= 0x0F;
    TMOD |= 0x20;
    TH1 = 0xFD;
    TL1 = 0xFD;
    TR1 = 1;
    SCON = 0x50;
    ES = 1;
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

void UART_ISR(void) interrupt 4
{
    if (RI) {
        RI = 0;
        g_rx_dummy = SBUF;
        g_rx_flag = 1;
    }
}

static void print_all(void)
{
    unsigned int free_mem;
    int err_code;

    free_mem = 1024;
    err_code = -3;

    debug_log("Hello from 8051!\r\n");
    debug_info("System started. Free memory: %d bytes\r\n", free_mem);
    debug_ok("UART 9600 8N1 initialized\r\n");
    debug_success("All peripherals ready\r\n");
    debug_warning("Battery low: %d %%\r\n", (int)15);
    debug_error("Sensor failed: code = %d\r\n", err_code);
    debug_logWithType("TRACE", "\033[90m", "tick = %u\r\n", (unsigned int)42);

    debug_setTimestampEnabled(true);
    debug_setColorEnabled(true);
    debug_setFilenameLineEnabled(true);

    debug_error_fileline("main.c", 62, "explicit file and line\r\n");
    debug_warning_fileline("main.c", 63, "explicit file and line\r\n");
    debug_log("custom fg %s\r\n", customTextColor(255, 128, 0));
    debug_log("custom bg %s\r\n", customBgColor(0, 0, 255));
}

void main(void)
{
    unsigned long t0;
    unsigned int i;

    UART_Init();
    Timer0_Init();
    debug_init(true, true, true);
    EA = 1;

    /* wait 20 ms (1 ms tick) before the first line, so the USB-UART
       bridge and the terminal are ready and nothing is lost */
    t0 = _debug_tick_ms;
    while ((_debug_tick_ms - t0) < 20) {
        ;
    }

    print_all();

    while (1) {
        if (g_rx_flag) {
            g_rx_flag = 0;
            /* small gap so the previous burst is flushed */
            for (i = 0; i < 200; i++) {
                ;
            }
            print_all();
        }
    }
}
