/* STC89C52RC, 11.0592 MHz, 12T. UART mode 1: 9600 baud, 8N1. */
#include <8052.h>
#include "ElegantDebug.h"

int putchar(int c) {
    SBUF = (unsigned char)c;
    while (!TI) { }
    TI = 0;
    return c;
}

void main(void) {
    PCON &= 0x7f;
    TMOD = (TMOD & 0x0f) | 0x20;
    TH1 = TL1 = 0xfd;
    SCON = 0x50;
    TR1 = 1;

    debug_init(false, false, false);
    debug_info("UART ready at %u baud\r\n", 9600U);
    debug_ok("LED %u\r\n", 64U);
    for (;;) { }
}
