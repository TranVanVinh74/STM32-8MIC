#include "UART.h"

void USART1_Init(void) {
    // 1. C?p xung cho GPIOA và USART1
    RCC->AHB1ENR |= (1 << 0);
    RCC->APB2ENR |= (1 << 4);

    // 2. C?u hình chân PA9 (TX) sang AF7
    GPIOA->MODER &= ~(0x3 << 18);
    GPIOA->MODER |=  (0x2 << 18);
    GPIOA->OSPEEDR |= (0x3 << 18); // T?c d? Very High Speed
    GPIOA->AFR[1] &= ~(0xF << 4);
    GPIOA->AFR[1] |=  (0x7 << 4);  // AF7

    // C?u hình chân PA10 (RX) sang AF7
    GPIOA->MODER &= ~(0x3 << 20);
    GPIOA->MODER |=  (0x2 << 20);
    GPIOA->AFR[1] &= ~(0xF << 8);
    GPIOA->AFR[1] |=  (0x7 << 8);

    // 3. T?c d? 4.000.000 Baud (100MHz / (16 * 4MHz) = 1.5625 -> Mantissa=1, Fraction=9)
    USART1->BRR = 0x19;

    // 4. B?t TE (bit 3), RE (bit 2) và UE (bit 13)
    USART1->CR1 = (1 << 3) | (1 << 2) | (1 << 13);
}

void USART1_WriteByte(uint8_t data) {
    while (!(USART1->SR & (1 << 7)));
    USART1->DR = data;
}

void USART1_WriteBuffer(const uint8_t *buffer, uint32_t length) {
    for (uint32_t i = 0; i < length; i++) {
        USART1_WriteByte(buffer[i]);
    }
}