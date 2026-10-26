#include "stm32f4xx.h"
#include "I2S.h"
#include "UART.h"
#include "RCC.h"
#include "SYSTICK.h"
#include "audio_protocol.h"

/* Raw capture only: no FFT, overlap, AI, gain, filtering or TFT. */
static int16_t audio_frame[AUDIO_BLOCK_FRAMES * AUDIO_CHANNELS];
#define STARTUP_DISCARD_BLOCKS 16U /* 512ms for mic startup; validate on hardware */

static void send_packet(uint32_t sequence, uint32_t faults)
{
    uint8_t header[AUDIO_HEADER_SIZE];
    uint16_t frames = faults ? 0U : AUDIO_BLOCK_FRAMES;
    audio_header(header, sequence, AUDIO_SAMPLE_RATE, frames, faults,
                 (const uint8_t *)audio_frame);
    USART1_WriteBuffer(header, sizeof(header));
    if (frames) USART1_WriteBuffer((const uint8_t *)audio_frame, sizeof(audio_frame));
}

int main(void)
{
    uint32_t expected = 1U;
    SystemClock_100MHz_HSI();
    SysTick_Init();
    USART1_Init();
    INMP441_8Mic_Init();
    uint32_t last_block = millis();
    while (1) {
        int state = I2S_CopyBlock(expected, audio_frame);
        if (state == 0 && (uint32_t)(millis() - last_block) >= 250U) {
            audio_faults |= 1U << 9;
            state = -1;
        }
        if (state < 0) {
            I2S_Stop();
            /* Stop rather than silently concatenate blocks from different times.
             * Repeat the fault so a PC opening the port later can diagnose it.
             * Reset the board after repairing the fault.
             */
            while (1) {
                send_packet(expected - 1U, audio_faults);
                Delay_ms(250U);
            }
        }
        if (state == 1) {
            last_block = millis();
            if (expected > STARTUP_DISCARD_BLOCKS)
                send_packet(expected - 1U, 0U);
            ++expected;
        }
    }
}
