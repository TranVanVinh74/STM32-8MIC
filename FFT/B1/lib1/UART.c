#include "UART.h"

void USART1_Init(void)
{
    RCC->AHB1ENR |= 1U;
    RCC->APB2ENR |= 1U << 4;
    RCC->APB2RSTR |= 1U << 4;
    RCC->APB2RSTR &= ~(1U << 4);
    GPIOA->MODER = (GPIOA->MODER & ~((3U << 18) | (3U << 20)))
                   | (2U << 18) | (2U << 20);
    GPIOA->OSPEEDR |= 3U << 18;
    GPIOA->OTYPER &= ~(1U << 9);
    GPIOA->PUPDR &= ~((3U << 18) | (3U << 20));
    GPIOA->AFR[1] = (GPIOA->AFR[1] & ~((15U << 4) | (15U << 8)))
                    | (7U << 4) | (7U << 8);
    USART1->CR1 = 0U; /* oversampling 16, 8 data bits, no parity */
    USART1->CR2 = 0U; /* one stop bit */
    USART1->CR3 = 0U; /* no flow control */
    USART1->BRR = 25U; /* APB2=100MHz / 4,000,000 baud */
    USART1->CR1 = (1U << 13) | (1U << 3) | (1U << 2);
}
void USART1_WriteByte(uint8_t data)
{
    while (!(USART1->SR & (1U << 7))) {}
    USART1->DR = data;
}
void USART1_WriteBuffer(const uint8_t *buffer, uint32_t length)
{
    while (length--) USART1_WriteByte(*buffer++);
}
