/* ---------------------------------------------------------------------------
 * ElegantDebug 8051 demo — STC89C52RC @ 11.0592 MHz, UART 9600 8N1, Timer0 1 ms
 *
 * Prints every level once, then re-prints the whole block whenever a byte
 * arrives on the UART (press Enter in the terminal).
 *
 * NOTE on the receiver: the demo polls RI in the main loop instead of using a
 * serial interrupt. The library's default putchar() owns TI (it waits for TI,
 * then clears it), so enabling ES would put the two in a fight: the ISR would
 * be entered on TI, and clearing or not clearing TI both break. If you want an
 * interrupt-driven receiver, define DEBUG_EXTERNAL_PUTCHAR and give the library
 * your own putchar() backed by a TX ring buffer.
 * ------------------------------------------------------------------------- */
#include <reg52.h>

#include "ElegantDebug.h"

#define UART_HAS_BYTE()   (RI)

/* Dumping ground for the received byte: reading SBUF is what clears the
 * receive buffer, and RI has to be cleared by software. */
static volatile unsigned char rx_dummy;

static void UART_Init(void)
{
    TMOD &= 0x0F;
    TMOD |= 0x20;       /* Timer1, mode 2: baud rate generator */
    TH1 = 0xFD;         /* 9600 @ 11.0592 MHz, SMOD = 0 */
    TL1 = 0xFD;
    SCON = 0x50;        /* mode 1, 8-bit UART, REN = 1 */
    TR1 = 1;
    ES = 0;             /* polled RX; see the note at the top of this file */
}

static void Timer0_Init(void)
{
    TMOD &= 0xF0;
    TMOD |= 0x01;       /* Timer0, mode 1: 16-bit */
    TH0 = 0xFC;         /* 1 ms @ 11.0592 MHz: 65536 - 922 = 0xFC66 */
    TL0 = 0x66;
    ET0 = 1;
    TR0 = 1;
}

void T0_ISR(void) interrupt 1
{
    TH0 = 0xFC;         /* mode 1 does not reload itself */
    TL0 = 0x66;
    debug_tick();       /* macro: one 32-bit increment */
}

static void print_all(void)
{
    unsigned int free_mem;
    int err_code;

    free_mem = 1024;
    err_code = -3;

    /* timestamp / color / file:line are all on (see debug_init below) */
    debug_log("Hello from 8051!\r\n");
    debug_info("System started. Free memory: %u bytes\r\n", free_mem);
    debug_ok("UART 9600 8N1 initialized\r\n");
    debug_success("All peripherals ready\r\n");

    /* Integer literals must be cast: C51 stores them in the smallest type that
       fits while the callee reads a 16-bit int. */
    debug_warning("Battery low: %d %%\r\n", (int)15);
    debug_error("Sensor failed: code = %d\r\n", err_code);

    debug_logWithType("TRACE", "\033[90m", "tick = %u\r\n", (unsigned int)42);

    /* C51 has no variadic macros, so the location is passed explicitly.
       __FILE__ / __LINE__ keep it correct when this file is edited. */
    debug_error_fileline(__FILE__, __LINE__, "explicit file and line\r\n");
    debug_warning_fileline(__FILE__, __LINE__, "explicit file and line\r\n");

    debug_log("custom fg %s\r\n", customTextColor(255, 128, 0));
    debug_log("custom bg %s\r\n", customBgColor(0, 0, 255));

    /* the three runtime switches, off then back on */
    debug_setTimestampEnabled(0);
    debug_setColorEnabled(0);
    debug_setFilenameLineEnabled(0);
    debug_info("all three switches off\r\n");
    debug_setTimestampEnabled(1);
    debug_setColorEnabled(1);
    debug_setFilenameLineEnabled(1);
}

void main(void)
{
    unsigned long t0;

    UART_Init();
    Timer0_Init();
    debug_init(1, 1, 1);        /* timestamp, color, file:line */
    EA = 1;

    /* wait 20 ms so the USB-UART bridge and the terminal are ready */
    t0 = _debug_tick_ms;
    while ((_debug_tick_ms - t0) < 20UL);

    print_all();

    for (;;) {
        if (UART_HAS_BYTE()) {
            rx_dummy = SBUF;
            RI = 0;
            print_all();
        }
    }
}
