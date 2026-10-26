#include "stm32f4xx.h"
#include "I2S.h"
#include "UART.h"

extern void SystemClock_100MHz_HSI(void);

// S? m?u stereo m?i n?a chu k? DMA: (4096 / 2) / 4 = 512 m?u
#define SAMPLES_PER_HALF_BLOCK  512

// B? d?m ch?a 8 kênh PCM 16-bit: 512 * 8 * 2 = 8192 bytes
static int16_t audio_frame_8ch[SAMPLES_PER_HALF_BLOCK * 8];

// Ð?ng b? khung gói tin b?ng 2 byte nh?n di?n: 0xAA, 0x55
static const uint8_t SYNC_HEADER[2] = {0xAA, 0x55};

static void Stream_8Ch_Audio_Block(uint16_t offset) {
    uint16_t *p_i2s2 = &i2s2_rx_buffer[offset]; // Mic 1 (L), Mic 2 (R) - Master
    uint16_t *p_i2s1 = &i2s1_rx_buffer[offset]; // Mic 3 (L), Mic 4 (R)
    uint16_t *p_i2s3 = &i2s3_rx_buffer[offset]; // Mic 5 (L), Mic 6 (R)
    uint16_t *p_i2s5 = &i2s5_rx_buffer[offset]; // Mic 7 (L), Mic 8 (R)

    uint32_t out_idx = 0;

    // Tách 16-bit MSB c?a t?ng micro và dóng gói xen k?: Mic 1 -> Mic 8
    for (uint16_t i = 0; i < SAMPLES_PER_HALF_BLOCK; i++) {
        uint16_t src = i * 4;

        audio_frame_8ch[out_idx++] = (int16_t)p_i2s2[src];     // Mic 1
        audio_frame_8ch[out_idx++] = (int16_t)p_i2s2[src + 2]; // Mic 2
        
        audio_frame_8ch[out_idx++] = (int16_t)p_i2s1[src];     // Mic 3
        audio_frame_8ch[out_idx++] = (int16_t)p_i2s1[src + 2]; // Mic 4
        
        audio_frame_8ch[out_idx++] = (int16_t)p_i2s3[src];     // Mic 5
        audio_frame_8ch[out_idx++] = (int16_t)p_i2s3[src + 2]; // Mic 6
        
        audio_frame_8ch[out_idx++] = (int16_t)p_i2s5[src];     // Mic 7
        audio_frame_8ch[out_idx++] = (int16_t)p_i2s5[src + 2]; // Mic 8
    }

    // 1. G?i Sync Header
    USART1_WriteBuffer(SYNC_HEADER, 2);

    // 2. B?n toàn b? 8192 bytes PCM thô qua UART 4 Mbps (ch? m?t ~20.5ms)
    USART1_WriteBuffer((uint8_t *)audio_frame_8ch, SAMPLES_PER_HALF_BLOCK * 8 * sizeof(int16_t));
}

int main(void) {
    // 1. Kích ho?t xung nh?p 100 MHz
    SystemClock_100MHz_HSI();

    // 2. Kh?i t?o USART1 t?c d? 4 Mbps
    USART1_Init();

    // 3. Kh?i t?o và d?ng b? 4 I2S thu 8 micro INMP441
    INMP441_8Mic_Init();

    while (1) {
        // N?a d?u buffer DMA (Ping)
        if (i2s2_half) {
            i2s2_half = 0;
            i2s1_half = 0;
            i2s3_half = 0;
            i2s5_half = 0;

            Stream_8Ch_Audio_Block(0);
        }

        // N?a sau buffer DMA (Pong)
        if (i2s2_full) {
            i2s2_full = 0;
            i2s1_full = 0;
            i2s3_full = 0;
            i2s5_full = 0;

            Stream_8Ch_Audio_Block(I2S_RX_BUFFER_SIZE / 2);
        }
    }
}