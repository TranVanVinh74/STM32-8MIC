#include "I2S.h"
#include "SYSTICK.h"
volatile uint16_t i2s1_rx_buffer[I2S_RX_BUFFER_SIZE];
volatile uint16_t i2s2_rx_buffer[I2S_RX_BUFFER_SIZE];
volatile uint16_t i2s3_rx_buffer[I2S_RX_BUFFER_SIZE];
volatile uint16_t i2s5_rx_buffer[I2S_RX_BUFFER_SIZE];
volatile uint32_t audio_faults;
static volatile uint32_t completed[4]; /* I2S2,1,3,5 */

static void I2S_PLLI2S_Init(void)
{
    RCC->CR &= ~(1U << 26);
    while (RCC->CR & (1U << 27)) {}
    RCC->CFGR &= ~(1U << 23); /* PLLI2S source, not external I2S_CKIN */
    /* HSI 16MHz / M16 * N256 / R5 = 51.2MHz; separate from CPU PLL. */
    RCC->PLLI2SCFGR = (16U << 0) | (256U << 6) | (5U << 28);
    RCC->CR |= (1U << 26);
    while (!(RCC->CR & (1U << 27))) {}
}

static void I2S_Config(SPI_TypeDef *SPIx, volatile uint16_t *rx_buffer) {
    DMA_Stream_TypeDef *DMA_Stream;
    IRQn_Type irqn;
    uint32_t dma_channel = 0;

    if (SPIx == SPI2) { // ========== I2S2 (MASTER RECEIVE) ==========
        RCC->AHB1ENR |= (1U << 1) | (1U << 21); // Clock GPIOB, DMA1
        RCC->APB1ENR |= (1U << 14);            // Clock SPI2

        // PB12 (WS), PB13 (CK), PB15 (SD) -> AF5
        GPIOB->MODER &= ~((0x3U << 24) | (0x3U << 26) | (0x3U << 30));
        GPIOB->MODER |=  ((0x2U << 24) | (0x2U << 26) | (0x2U << 30));
        GPIOB->OSPEEDR |= ((0x3U << 24) | (0x3U << 26) | (0x3U << 30)); // T?c d? t?i da
        GPIOB->AFR[1] &= ~((0xFU << 16) | (0xFU << 20) | (0xFU << 28));
        GPIOB->AFR[1] |=  ((0x5U << 16) | (0x5U << 20) | (0x5U << 28));

        DMA_Stream = DMA1_Stream3;
        dma_channel = 0; // DMA1_Stream3 Channel 0
        irqn = DMA1_Stream3_IRQn;

    } else if (SPIx == SPI1) { // ========== I2S1 (SLAVE RECEIVE) ==========
        RCC->AHB1ENR |= (1U << 0) | (1U << 22); // Clock GPIOA, DMA2
        RCC->APB2ENR |= (1U << 12);            // Clock SPI1

        // PA4 (WS), PA5 (CK), PA7 (SD) -> AF5
        GPIOA->MODER &= ~((0x3U << 8) | (0x3U << 10) | (0x3U << 14));
        GPIOA->MODER |=  ((0x2U << 8) | (0x2U << 10) | (0x2U << 14));
        GPIOA->OSPEEDR |= ((0x3U << 8) | (0x3U << 10) | (0x3U << 14));
        GPIOA->AFR[0] &= ~((0xFU << 16) | (0xFU << 20) | (0xFU << 28));
        GPIOA->AFR[0] |=  ((0x5U << 16) | (0x5U << 20) | (0x5U << 28));

        DMA_Stream = DMA2_Stream2;
        dma_channel = 3; // DMA2_Stream2 Channel 3
        irqn = DMA2_Stream2_IRQn;

    } else if (SPIx == SPI3) { // ========== I2S3 (SLAVE RECEIVE) ==========
        RCC->AHB1ENR |= (1U << 0) | (1U << 1) | (1U << 21); // Clock GPIOA, GPIOB, DMA1
        RCC->APB1ENR |= (1U << 15);                       // Clock SPI3

        // 1. Chân PA15 (WS) -> AF6 (T? gi?i phóng JTDI c?a JTAG)
        GPIOA->MODER &= ~(0x3U << 30);
        GPIOA->MODER |=  (0x2U << 30);
        GPIOA->OSPEEDR |= (0x3U << 30);
        GPIOA->AFR[1] &= ~(0xFU << 28);
        GPIOA->AFR[1] |=  (0x6U << 28);

        // 2. Chân PB3 (CK) và PB5 (SD) -> AF6 (PB3 gi?i phóng JTDO)
        GPIOB->MODER &= ~((0x3U << 6) | (0x3U << 10));
        GPIOB->MODER |=  ((0x2U << 6) | (0x2U << 10));
        GPIOB->OSPEEDR |= ((0x3U << 6) | (0x3U << 10));
        GPIOB->AFR[0] &= ~((0xFU << 12) | (0xFU << 20));
        GPIOB->AFR[0] |=  ((0x6U << 12) | (0x6U << 20));

        DMA_Stream = DMA1_Stream0;
        dma_channel = 0; // DMA1_Stream0 Channel 0
        irqn = DMA1_Stream0_IRQn;

    } else if (SPIx == SPI5) { // ========== I2S5 (SLAVE RECEIVE) ==========
        RCC->AHB1ENR |= (1U << 1) | (1U << 22); // Clock GPIOB, DMA2
        RCC->APB2ENR |= (1U << 20);            // Clock SPI5

        // PB0 (CK), PB1 (WS) -> AF6 n?m trong AFR[0]
        GPIOB->MODER &= ~((0x3U << 0) | (0x3U << 2));
        GPIOB->MODER |=  ((0x2U << 0) | (0x2U << 2));
        GPIOB->OSPEEDR |= ((0x3U << 0) | (0x3U << 2));
        GPIOB->AFR[0] &= ~((0xFU << 0) | (0xFU << 4));
        GPIOB->AFR[0] |=  ((0x6U << 0) | (0x6U << 4));

        // PB8 (SD) -> AF6 n?m trong AFR[1] (chân 8 -> index 0)
        GPIOB->MODER &= ~(0x3U << 16);
        GPIOB->MODER |=  (0x2U << 16);
        GPIOB->OSPEEDR |= (0x3U << 16);
        GPIOB->AFR[1] &= ~(0xFU << 0);
        GPIOB->AFR[1] |=  (0x6U << 0);

        DMA_Stream = DMA2_Stream3;
        dma_channel = 2; // DMA2_Stream3 Channel 2
        irqn = DMA2_Stream3_IRQn;

    } else {
        return;
    }

    // Reset I2SCFGR
    SPIx->I2SCFGR = 0;

    // C?u hình thanh ghi I2SCFGR:
    // Bit 11: I2SMOD = 1 (Ch? d? I2S)
    // Bit 5-4: I2SSTD = 00 (Chu?n Philips)
    // Bit 2-1: DATLEN = 01 (D? li?u 24-bit)
    // Bit 0: CHLEN = 1 (Khung truy?n 32-bit m?i channel L/R)
    if (SPIx == SPI2) {
        // Master Receive (I2SCFG = 11 -> Bit 9:8 = 0x3)
        SPIx->I2SCFGR = (1U << 11) | (0x3U << 8) | (0x0U << 4) | (0x1U << 1) | (1U << 0);
        SPIx->I2SPR = 25U; // Prescaler ch? c?n cài d?t cho Master d? phát dúng xung nh?p
    } else {
        // Slave Receive (I2SCFG = 01 -> Bit 9:8 = 0x1)
        SPIx->I2SCFGR = (1U << 11) | (0x1U << 8) | (0x0U << 4) | (0x1U << 1) | (1U << 0);
        SPIx->I2SPR = 2;  // M?c d?nh cho Slave
    }

    // B?t yêu c?u DMA phía ngo?i vi SPI/I2S (RXDMAEN)
    SPIx->CR2 = (1U << 0);

    // C?u hình DMA Stream
    DMA_Stream->CR &= ~(1U << 0);
    while (DMA_Stream->CR & (1U << 0)); // Ch? DMA d?ng hoàn toàn
    DMA_Stream->CR = 0;
    DMA_Stream->FCR = (1U << 7); /* direct mode, FIFO error IRQ */

    // C?u hình thanh ghi DMA CR:
    // Channel selection, MSIZE=16-bit (01), PSIZE=16-bit (01), MINC=1, CIRC=1, Priority Very High (11)
    DMA_Stream->CR |= (dma_channel << 25) | (0x3U << 16) | (0x1U << 13) | (0x1U << 11) | (1U << 10) | (1U << 8);

    DMA_Stream->PAR = (uint32_t)&(SPIx->DR);
    DMA_Stream->M0AR = (uint32_t)rx_buffer;
    DMA_Stream->NDTR = I2S_RX_BUFFER_SIZE;

    // B?t ng?t Half-Transfer (HTIE - bit 3) và Transfer-Complete (TCIE - bit 4)
    DMA_Stream->CR |= (1U << 1) | (1U << 2) | (1U << 3) | (1U << 4);
    NVIC_SetPriority(irqn, 1);
    NVIC_EnableIRQ(irqn);
}


static DMA_Stream_TypeDef * const streams[4] = {
    DMA1_Stream3, DMA2_Stream2, DMA1_Stream0, DMA2_Stream3
};
static SPI_TypeDef * const peripherals[4] = {SPI2, SPI1, SPI3, SPI5};
static volatile uint16_t * const buffers[4] = {
    i2s2_rx_buffer, i2s1_rx_buffer, i2s3_rx_buffer, i2s5_rx_buffer
};

void INMP441_8Mic_Init(void)
{
    uint32_t t;
    audio_faults = 0;
    for (unsigned i = 0; i < 4; ++i) completed[i] = 0;
    I2S_PLLI2S_Init();
    /* Reset receivers before configuring; no live I2S clocks yet. */
    RCC->APB1RSTR |= (1U << 14) | (1U << 15);
    RCC->APB1RSTR &= ~((1U << 14) | (1U << 15));
    RCC->APB2RSTR |= (1U << 12) | (1U << 20);
    RCC->APB2RSTR &= ~((1U << 12) | (1U << 20));
    I2S_Config(SPI1, i2s1_rx_buffer);
    I2S_Config(SPI3, i2s3_rx_buffer);
    I2S_Config(SPI5, i2s5_rx_buffer);
    I2S_Config(SPI2, i2s2_rx_buffer);

    /* Hold external WS high and CK low while arming Philips-I2S slaves.
     * ES0287 requires WS high when enabling slaves. GPIO latch is set
     * before switching modes. No clock edges are intentionally emitted.
     * Validate the first frame/slot on a logic analyzer on the actual board.
     */
    GPIOB->BSRR = (1U << 12) | (1U << (13 + 16));
    GPIOB->MODER = (GPIOB->MODER & ~((3U << 24) | (3U << 26)))
                    | (1U << 24) | (1U << 26);
    Delay_ms(1);
    t = millis();
    while (!(GPIOA->IDR & (1U << 4)) || !(GPIOA->IDR & (1U << 15))
           || !(GPIOB->IDR & (1U << 1))) {
        if ((uint32_t)(millis() - t) >= 10U) {
            audio_faults |= 1U << 10;
            return;
        }
    }
    /* Clear only flags belonging to our four streams. */
    DMA1->LIFCR = 0x0F400000U | 0x0000003DU;
    DMA2->LIFCR = 0x003D0000U | 0x0F400000U;
    for (unsigned i = 0; i < 4; ++i) streams[i]->CR |= 1U;
    SPI1->I2SCFGR |= 1U << 10;
    SPI3->I2SCFGR |= 1U << 10;
    SPI5->I2SCFGR |= 1U << 10;
    __DSB();
    /* Return WS/CK to the stopped master, then start clock generation. */
    GPIOB->MODER = (GPIOB->MODER & ~((3U << 24) | (3U << 26)))
                    | (2U << 24) | (2U << 26);
    SPI2->I2SCFGR |= 1U << 10;
}

void I2S_Stop(void)
{
    SPI2->I2SCFGR &= ~(1U << 10);
    for (unsigned i = 0; i < 4; ++i) {
        peripherals[i]->I2SCFGR &= ~(1U << 10);
        streams[i]->CR &= ~1U;
        while (streams[i]->CR & 1U) {}
    }
}

static void dma_event(unsigned index, uint32_t status, unsigned shift,
                      volatile uint32_t *clear)
{
    uint32_t f = (status >> shift) & 0x3DU;
    uint32_t events = f & 0x30U;
    *clear = 0x3DU << shift;
    if (f & 0x0DU) audio_faults |= 1U << index;
    /* Both pending implies IRQ service was delayed by at least a half-buffer. */
    if (events == 0x30U) {
        audio_faults |= 1U << 8;
    } else if (events) {
        uint32_t wanted = (completed[index] & 1U) ? 0x20U : 0x10U;
        if (events != wanted) audio_faults |= 1U << 8;
        __DMB();
        ++completed[index];
    }
}
void DMA1_Stream3_IRQHandler(void) { dma_event(0, DMA1->LISR, 22, &DMA1->LIFCR); }
void DMA2_Stream2_IRQHandler(void) { dma_event(1, DMA2->LISR, 16, &DMA2->LIFCR); }
void DMA1_Stream0_IRQHandler(void) { dma_event(2, DMA1->LISR, 0, &DMA1->LIFCR); }
void DMA2_Stream3_IRQHandler(void) { dma_event(3, DMA2->LISR, 22, &DMA2->LIFCR); }

static int block_safe(uint32_t expected)
{
    for (unsigned i = 0; i < 4; ++i) {
        uint32_t n = streams[i]->NDTR;
        if (completed[i] != expected || n == 0U) return 0;
        /* Odd completion: first half ready; DMA must be in second half. */
        if (expected & 1U) {
            if (n > I2S_RX_BUFFER_SIZE / 2U) return 0;
        } else if (n <= I2S_RX_BUFFER_SIZE / 2U) return 0;
    }
    return 1;
}

int I2S_CopyBlock(uint32_t expected, int16_t *output)
{
    int ready = 1;
    for (unsigned i = 0; i < 4; ++i) {
        if (peripherals[i]->SR & (1U << 6)) audio_faults |= 1U << (4U + i);
        if (completed[i] > expected) audio_faults |= 1U << 8;
        if (completed[i] < expected) ready = 0;
    }
    if (audio_faults) return -1;
    if (!ready) return 0;
    __DMB();
    if (!block_safe(expected)) { audio_faults |= 1U << 8; return -1; }
    uint32_t offset = (expected & 1U) ? 0U : I2S_RX_BUFFER_SIZE / 2U;
    for (unsigned n = 0; n < AUDIO_BLOCK_FRAMES; ++n) {
        for (unsigned pair = 0; pair < 4; ++pair) {
            output[n * 8U + pair * 2U] = (int16_t)buffers[pair][offset + n * 4U];
            output[n * 8U + pair * 2U + 1U] = (int16_t)buffers[pair][offset + n * 4U + 2U];
        }
    }
    __DMB();
    if (!block_safe(expected)) audio_faults |= 1U << 8;
    for (unsigned i = 0; i < 4; ++i)
        if (peripherals[i]->SR & (1U << 6)) audio_faults |= 1U << (4U + i);
    return audio_faults ? -1 : 1;
}
