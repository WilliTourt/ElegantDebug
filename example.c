/*******************************************************************************
 * @file    example_8051.c
 * @brief   8051 (Keil C51) usage example for ElegantDebug.
 * 
 * Note: Due to the extremely limited RAM (128 bytes) on standard 8051, 
 * complex string formatting (like vsnprintf) should be avoided. 
 * This example demonstrates how to use the color macros and prefixes 
 * provided by ElegantDebug with the standard C51 `printf`.
 ******************************************************************************/

#include <REGX52.H>
#include <stdio.h>
#include "ElegantDebug.h"

// ============================================================================
// 1. 必须提供 putchar 重定向，让 printf 能通过串口输出
// ============================================================================
char putchar(char c) {
    // 假设你的串口发送函数叫 UART_SendByte
    // UART_SendByte(c); 
    
    // 这里为了演示，假设直接操作寄存器
    SBUF = c;
    while(!TI);
    TI = 0;
    
    return c;
}

// 假设的串口初始化函数（波特率 9600 @ 11.0592MHz）
void Uart1_Init(void) {
    PCON &= 0x7F; 
    SCON = 0x50;  
    TMOD &= 0x0F; 
    TMOD |= 0x20; 
    TL1 = 0xFD;   
    TH1 = 0xFD;
    TR1 = 1;
    TI = 1;       
}

void main(void) {
    Uart1_Init();
    
    // 2. 初始化 ElegantDebug (C51 版本不需要传 UART 句柄)
    debug_init(); 
    
    // 3. 使用库中定义的 ANSI 颜色宏
    printf(COLOR_GREEN "[OK] 8051 System Booted!\r\n" CLR);
    
    while(1) {
        // 演示 1：模拟你的作业输出（温度）
        // 注意：不要在 51 里用 %f，用整数拆分成整数和小数部分
        int temp_int = 25;
        int temp_dec = 30;
        
        printf(INFO_TYPE "[TEMP] %d.%02dC @1000ms\r\n", temp_int, temp_dec);
        
        // 演示 2：模拟错误信息
        printf(ERROR_TYPE "Sensor disconnected!\r\n");
        
        // 模拟延时
        // Delay_ms(1000); 
    }
}