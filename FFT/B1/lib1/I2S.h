#ifndef __I2S_H
#define __I2S_H

#include "stm32f4xx.h"

/*
 * ==============================================================================
 * SO Ð? K?T N?I 8 MICROPHONE INMP441 V?I STM32F411CEU6 (BLACKPILL 48 CHÂN)
 * ==============================================================================
 * Master Clock (PB12, PB13 qua IC d?m 74LVC244 c?p chung cho 8 mic và các Slave)
 *
 * 1. I2S2 (Master Rx) -> Mic 1 (Left) & Mic 2 (Right)
 *    - PB12 : I2S2_WS  (AF5) - Master Frame Sync
 *    - PB13 : I2S2_CK  (AF5) - Master Bit Clock
 *    - PB15 : I2S2_SD  (AF5) - Serial Data
 *
 * 2. I2S1 (Slave Rx)  -> Mic 3 (Left) & Mic 4 (Right)
 *    - PA4  : I2S1_WS  (AF5) - Nh?n WS t? d?m
 *    - PA5  : I2S1_CK  (AF5) - Nh?n SCK t? d?m
 *    - PA7  : I2S1_SD  (AF5) - Serial Data
 *
 * 3. I2S3 (Slave Rx)  -> Mic 5 (Left) & Mic 6 (Right)
 *    - PA15 : I2S3_WS  (AF6) - Nh?n WS t? d?m (T?t JTAG, gi? SWD)
 *    - PB3  : I2S3_CK  (AF6) - Nh?n SCK t? d?m (T?t JTAG, gi? SWD)
 *    - PB5  : I2S3_SD  (AF6) - Serial Data
 *
 * 4. I2S5 (Slave Rx)  -> Mic 7 (Left) & Mic 8 (Right)
 *    - PB1  : I2S5_WS  (AF6) - Nh?n WS t? d?m
 *    - PB0  : I2S5_CK  (AF6) - Nh?n SCK t? d?m
 *    - PB8  : I2S5_SD  (AF6) - Serial Data
 * ==============================================================================
 */

// Kích thu?c b? d?m (m?i sample 24-bit chi?m 2 uint16_t trong frame 32-bit)
// 512 samples/frame * 2 kênh (L/R) * 2 half-words * 2 (Ping-Pong) = 4096
#define I2S_RX_BUFFER_SIZE  4096 

// 4 B? d?m DMA cho 4 c?p micro
extern uint16_t i2s1_rx_buffer[I2S_RX_BUFFER_SIZE]; // Mic 3 & 4
extern uint16_t i2s2_rx_buffer[I2S_RX_BUFFER_SIZE]; // Mic 1 & 2 (Master)
extern uint16_t i2s3_rx_buffer[I2S_RX_BUFFER_SIZE]; // Mic 5 & 6
extern uint16_t i2s5_rx_buffer[I2S_RX_BUFFER_SIZE]; // Mic 7 & 8

// C? báo ng?t DMA Half-Transfer (n?a d?u) và Transfer-Complete (n?a sau)
extern volatile uint8_t i2s1_half, i2s1_full;
extern volatile uint8_t i2s2_half, i2s2_full;
extern volatile uint8_t i2s3_half, i2s3_full;
extern volatile uint8_t i2s5_half, i2s5_full;

// Kh?i t?o Clock PLL riêng cho I2S
void I2S_PLLI2S_Init(void);

// Kh?i t?o ph?n c?ng riêng l? t?ng b? (chua b?t ch?y)
void I2S_Config(SPI_TypeDef *SPIx, uint16_t *rx_buffer);

// Hàm kích ho?t toàn b? 8 Micro d?ng b? tuy?t d?i (Hardware Lock-step)
void INMP441_8Mic_Init(void);

#endif /* __I2S_H */