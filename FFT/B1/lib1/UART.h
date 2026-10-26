#ifndef __UART_H
#define __UART_H

#include "stm32f4xx.h"

// T?c d? 4 Mbps (4.000.000 baud) trên USART1 (APB2 = 100 MHz, sai s? 0.00%)
void USART1_Init(void);
void USART1_WriteByte(uint8_t data);
void USART1_WriteBuffer(const uint8_t *buffer, uint32_t length);

#endif /* __UART_H */