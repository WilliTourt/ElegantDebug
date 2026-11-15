/*******************************************************************************
 * @file    main.c
 * @brief   Example for the ElegantDebug 8051 port.
 *
 * Target : 8052-class MCU (STC89C52RC / AT89C52), 11.0592 MHz crystal,
 *          UART mode 1 at 9600 8N1, Timer0 interrupt every 1 ms.
 * Output : connect a USB-serial adapter (CH340 etc.) to TXD/P3.1 and open a
 *          terminal that understands ANSI escape sequences (MobaXterm, Xshell,
 *          VS Code serial monitor) to see the colors.
 *
 * Build  : add main.c and ../Src-C/ElegantDebug_8051.c to the project and put
 *          ..\Src-C on the C51 include path. See README.md in this folder.
 *
 * @author:    <your name>
 * @date:      2026-11-15
 ******************************************************************************/
#include <reg52.h>
#include "ElegantDebug_8051.h"

static volatile unsigned long uptime_ms = 0UL;

/* 9600 8N1 with an 11.0592 MHz crystal, SMOD = 0:
 *   9600 = (1/32) * (11059200/12) / (256 - 0xFD) */
static void uart_init_9600(void)
{
    SCON = 0x50;                /* mode 1, REN = 1 */
    TMOD &= 0x0F;               /* keep Timer0 bits */
    TMOD |= 0x20;               /* Timer1 mode 2: baud rate generator */
    TH1 = 0xFD;
    TL1 = 0xFD;
    PCON &= 0x7F;               /* SMOD = 0 */
    TR1 = 1;
    ES = 0;                     /* polled TX: no UART interrupt */
}

static void timer0_init_1ms(void)
{
    TMOD &= 0xF0;
    TMOD |= 0x01;               /* Timer0 mode 1: 16-bit timer */
    TH0 = 0xFC;                 /* 65536 - 922 = 0xFC66 for 1 ms */
    TL0 = 0x66;
    ET0 = 1;
    TR0 = 1;
    EA  = 1;
}

void timer0_isr(void) interrupt 1
{
    TH0 = 0xFC;                 /* mode 1 does not reload itself */
    TL0 = 0x66;
    uptime_ms++;
    debug_8051_tick();          /* 1 ms tick for the timestamp */
}

void main(void)
{
    unsigned long next_print = 0UL;

    uart_init_9600();
    timer0_init_1ms();

    /* timestamp on, color on, file:line on */
    debug_8051_init(ED8051_TRUE, ED8051_TRUE, ED8051_TRUE);

    debug_8051_info("ElegantDebug 8051 port, 9600 8N1\r\n");
    debug_8051_ok("UART ready\r\n");
    debug_8051_log("timestamp only, no type prefix\r\n");
    debug_8051_success("ANSI colors are produced by the terminal\r\n");
    DEBUG_8051_WARNING("warning with file:line\r\n");
    DEBUG_8051_ERROR("error with file:line\r\n");
    debug_8051_log_u32("counter = ", 42UL);

    /* the runtime switches work too */
    debug_8051_setTimestamp(ED8051_TRUE);
    debug_8051_setColor(ED8051_TRUE);

    while (1)
    {
        if (uptime_ms - next_print >= 500UL)
        {
            next_print = uptime_ms;
            debug_8051_log_u32("tick(ms) = ", uptime_ms);
        }
    }
}
