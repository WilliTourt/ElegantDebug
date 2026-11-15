/**
 * @file    example_8051.c
 * @brief   Example usage of ElegantDebug for 8051 (STC89C52RC).
 *
 * Hardware Setup:
 *   - MCU: STC89C52RC (or AT89C52)
 *   - Crystal: 11.0592 MHz
 *   - UART: 9600 baud, 8N1. Connect MCU TXD(P3.1) to CH340 RXD.
 *   - Timer0: 1ms interrupt for timestamp generation.
 *
 * Notes:
 *   Compile with Keil C51. Ensure the "Small" memory model is selected.
 *   The terminal must support ANSI escape sequences (e.g., VS Code Serial Monitor).
 *
 * @author  Jian0635
 * @date    2026-11-15
 */

#include <reg52.h>
#include "ElegantDebug.h"

void delay_ms(unsigned int ms) {
    unsigned int i, j;
    for(i = 0; i < ms; i++)
        for(j = 0; j < 110; j++); 
}

void Timer0_Init() {
    TMOD &= 0xF0;
    TMOD |= 0x01; 
    TH0 = 0xFC;   
    TL0 = 0x66;
    ET0 = 1;      
    EA = 1;       
    TR0 = 1;      
}

void Timer0_ISR() interrupt 1 {
    TH0 = 0xFC;   
    TL0 = 0x66;
    debug_tick(); 
}

void main() {
    delay_ms(200); 
    debug_init();  
    Timer0_Init(); 
    delay_ms(50); 

    // 1. Basic logs
    debug_log("Hello 8051!\r\n");
    debug_info("Sys OK\r\n");
    debug_ok("UART OK\r\n");
    
    // 2. Warnings and Errors
    debug_warning_fileline("main.c", 42, "Low memory!\r\n");
    debug_error_fileline("main.c", 43, "Test error!\r\n");
    
    // 3. Custom type and colors
    debug_logWithType("TST", COLOR_DARK_CYAN, "TypeOK\r\n");
    debug_log("%sRed Text%s\r\n", customTextColor(255, 0, 0), CLR);
    debug_log("%sBG Color%s\r\n", customBgColor(0, 255, 0), CLR);

    // 4. Runtime switches
    debug_setTimestampEnabled(false);
    debug_log("TS off\r\n");
    debug_setTimestampEnabled(true);

    debug_setColorEnabled(false);
    debug_log("Clr off\r\n");
    debug_setColorEnabled(true);
    
    debug_setFilenameLineEnabled(true);
    debug_error_fileline("m.c", 42, "F/L\r\n");
    debug_setFilenameLineEnabled(false);

    // Main loop
    while (1) {
        debug_success("Alive\r\n"); 
        delay_ms(1000); 
    }
}
