/*******************************************************************************
 * @file    main.c
 * @brief   ElegantDebug on 8051 — minimal example (Keil C51 / uVision).
 *
 * Target : 8052-class MCU with a UART (AT89C52, STC89C52RC, ...)
 * Clock  : 11.0592 MHz  (9600 baud with TH1 = 0xFD, 1 ms with TH0/TL0 = 0xFC66)
 * Output : UART TX only — 9600 8N1, open a serial terminal on the same port
 *
 * Wiring : none beyond the UART (CH340/MAX232 on the board)
 *          P3.1 = TXD, P3.0 = RXD
 *
 * Build  : add these two files to the project
 *            Src-C/ElegantDebug.c
 *            example_8051/main.c  (this file)
 *          (nothing else: the 8051 port does not use the C library)
 *          Set "Device" to your part and enable "Create HEX File".
 *
 *          Two project settings are required, otherwise C51 reports
 *          "warning C318: can't open file 'ElegantDebug.h'" or
 *          "error C202: 'true': undefined identifier":
 *            1. Options for Target -> C51 -> Include Paths: add `Src-C`,
 *               because this file includes "ElegantDebug.h" from another
 *               folder (C51 only searches the source file's own folder plus
 *               the include paths);
 *            2. Options for Target -> C51 -> Define: add `USE_8051`
 *               (ElegantDebug.c needs it as well, not only this file).
 *
 * Toolchain: written for Keil C51 (verified).  The library files also build
 *          with SDCC, which spells a few things differently:
 *            <reg52.h> -> <8052.h>,  _nop_() -> __asm nop __endasm;,
 *            "void f(void) interrupt 1" -> "void f(void) __interrupt(1)",
 *            "idata" -> "__idata" (already handled in ElegantDebug.c).
 *
 * RAM    : the 8051 port needs about 90 bytes of DATA (8 bytes of statics plus
 *          the call frames) and no IDATA, so it fits a 128-byte 8051 as well
 *          as a 256-byte 8052.
 ******************************************************************************/

#include <reg52.h>
#include <intrins.h>          /* _nop_() for the delay loop */

/* The 8051 build never formats, so DEBUG_BUFFER_LEN is not used there;
 * it only sizes the buffers of the other platforms. */

#ifndef USE_8051
    #define USE_8051
#endif
#include "ElegantDebug.h"



/* ---- 1 ms tick for timestamps: Timer0 mode 1 -------------------------- */
static void Timer0_Init(void)
{
    TMOD = (TMOD & 0xF0) | 0x01;    /* Timer0 mode 1 (16 bit) */
    TH0  = 0xFC;                    /* 11.0592 MHz: 922 cycles = 1 ms */
    TL0  = 0x66;
    ET0  = 1;                       /* enable Timer0 interrupt */
    TR0  = 1;                       /* start */
}


/* ---- 9600 8N1: Timer1 mode 2, SMOD = 0 ------------------------------- */
static void Uart_Init(void)
{
    SCON = 0x50;                    /* mode 1, REN = 1 */
    TMOD = (TMOD & 0x0F) | 0x20;    /* Timer1 mode 2 (8-bit auto reload) */
    TH1  = 0xFD;                    /* 9600 baud @ 11.0592 MHz */
    TL1  = 0xFD;
    PCON &= 0x7F;                   /* SMOD = 0 */
    TR1  = 1;                       /* start baud generator */

    /* NOTE: deliberately no `ES = 1`.  The 8051 port transmits by polling TI
     * (see the ED8051_PUTC() hook in ElegantDebug.h); if your application needs
     * UART RX interrupts, route TI handling through your own ISR or define your
     * own ED8051_PUTC before including the header. */
}


/* ---- Timer0 interrupt: feeds the library's millisecond counter -------- */
void Timer0_ISR(void) interrupt 1
{
    TH0 = 0xFC;
    TL0 = 0x66;
    debug_tick();                   /* ++_debug_tick_ms */
}


static void Delay_ms(unsigned int ms)
{
    unsigned int i, j;
    for (i = 0; i < ms; i++) {
        for (j = 0; j < 92; j++) {  /* ~1 ms at 11.0592 MHz, 12T */
            _nop_();
        }
    }
}


void main(void)
{
    unsigned int i;

    Uart_Init();
    Timer0_Init();
    EA = 1;

    /* No UART handle on 8051: timestamp / color / file:line flags only. */
    debug_init(true, true, false);

    /* The 8051 build has no printf-style API (C51 would need a 48-byte static
     * argument area per variadic function, plus the formatter frames — more
     * than the 128 bytes of directly addressed RAM).  Use a literal message
     * or "label + one unsigned number" instead.  The *_lit() functions do not
     * append a line break, exactly like the other platforms. */
    debug_info_lit("Hello from 8051!\r\n");
    debug_ok_lit("UART 9600 8N1 works\r\n");
    debug_log_u32("counter = ", 42ul);              /* one complete line */

    debug_setFilenameLineEnabled(true);
    debug_warning_lit("main.c", 106, "warning with file and line\r\n");
    debug_error_lit("main.c", 107, "error code -7\r\n");
    debug_setFilenameLineEnabled(false);

    /* Custom prefixes / 24-bit colours: write the escape sequence yourself */
    debug_log_lit("\033[1m\033[95m[CUSTOM]\033[0m escape sequences work\r\n");
    debug_log_lit("\033[92mgreen text\033[0m and \033[1mbold\033[0m\r\n");

    debug_success_lit("setup finished, printing the tick every 500 ms\r\n");

    for (i = 0; ; i++) {
        debug_log_u32("tick(ms) = ", (unsigned long)_debug_tick_ms);
        Delay_ms(500);
    }
}
