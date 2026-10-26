#ifndef AUDIO_I2S_H
#define AUDIO_I2S_H
#include "stm32f4xx.h"
#define AUDIO_SAMPLE_RATE 16000U
#define AUDIO_CHANNELS 8U
#define AUDIO_BLOCK_FRAMES 512U
#define I2S_RX_BUFFER_SIZE (AUDIO_BLOCK_FRAMES * 4U * 2U)
/* Channel order: I2S2 L/R, I2S1 L/R, I2S3 L/R, I2S5 L/R.
 * Master: PB12 WS, PB13 CK, PB15 SD (AF5).
 * Slaves: PA4/PA5/PA7 AF5; PA15/PB3/PB5 AF6; PB1/PB0/PB8 AF6.
 * Wire shared WS/CK physically to every slave and microphone.
 */
extern volatile uint16_t i2s1_rx_buffer[I2S_RX_BUFFER_SIZE];
extern volatile uint16_t i2s2_rx_buffer[I2S_RX_BUFFER_SIZE];
extern volatile uint16_t i2s3_rx_buffer[I2S_RX_BUFFER_SIZE];
extern volatile uint16_t i2s5_rx_buffer[I2S_RX_BUFFER_SIZE];
/* bits 0..3 DMA errors (I2S2,1,3,5), 4..7 I2S OVR,
 * bit 8 overwritten/out-of-order block, 9 timeout, 10 WS wiring/startup.
 */
extern volatile uint32_t audio_faults;
void INMP441_8Mic_Init(void);
void I2S_Stop(void);
/* 1 copied, 0 not ready, -1 fatal. expected starts at 1. */
int I2S_CopyBlock(uint32_t expected, int16_t *output);
#endif
