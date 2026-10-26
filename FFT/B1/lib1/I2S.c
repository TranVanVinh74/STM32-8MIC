#include "I2S.h"

uint16_t i2s1_rx_buffer[I2S_RX_BUFFER_SIZE]; 
uint16_t i2s2_rx_buffer[I2S_RX_BUFFER_SIZE]; 
uint16_t i2s3_rx_buffer[I2S_RX_BUFFER_SIZE]; 
uint16_t i2s5_rx_buffer[I2S_RX_BUFFER_SIZE]; 

volatile uint8_t i2s1_half = 0, i2s1_full = 0;
volatile uint8_t i2s2_half = 0, i2s2_full = 0;
volatile uint8_t i2s3_half = 0, i2s3_full = 0;
volatile uint8_t i2s5_half = 0, i2s5_full = 0;

void I2S_PLLI2S_Init(void) {
    // T?t PLLI2S tru?c khi ghi dè c?u hình
    RCC->CR &= ~(1 << 26);
    while (RCC->CR & (1 << 27)); 

    // C?u hình: PLLI2SN = 213, PLLI2SR = 2 (Chu?n cho audio 16kHz/32kHz/48kHz)
    RCC->PLLI2SCFGR &= ~((0x1FF << 6) | (0x7 << 28)); 
    RCC->PLLI2SCFGR |= (213 << 6) | (2 << 28);      
    
    // B?t PLLI2S và ch? c? s?n sàng (Ready)
    RCC->CR |= (1 << 26);
    while (!(RCC->CR & (1 << 27))); 
}

void I2S_Config(SPI_TypeDef *SPIx, uint16_t *rx_buffer) {
    DMA_Stream_TypeDef *DMA_Stream;
    IRQn_Type irqn;
    uint32_t dma_channel = 0;

    if (SPIx == SPI2) { // ========== I2S2 (MASTER RECEIVE) ==========
        RCC->AHB1ENR |= (1 << 1) | (1 << 21); // Clock GPIOB, DMA1
        RCC->APB1ENR |= (1 << 14);            // Clock SPI2

        // PB12 (WS), PB13 (CK), PB15 (SD) -> AF5
        GPIOB->MODER &= ~((0x3 << 24) | (0x3 << 26) | (0x3 << 30));
        GPIOB->MODER |=  ((0x2 << 24) | (0x2 << 26) | (0x2 << 30));
        GPIOB->OSPEEDR |= ((0x3 << 24) | (0x3 << 26) | (0x3 << 30)); // T?c d? t?i da
        GPIOB->AFR[1] &= ~((0xF << 16) | (0xF << 20) | (0xF << 28));
        GPIOB->AFR[1] |=  ((0x5 << 16) | (0x5 << 20) | (0x5 << 28)); 

        DMA_Stream = DMA1_Stream3;
        dma_channel = 0; // DMA1_Stream3 Channel 0
        irqn = DMA1_Stream3_IRQn;

    } else if (SPIx == SPI1) { // ========== I2S1 (SLAVE RECEIVE) ==========
        RCC->AHB1ENR |= (1 << 0) | (1 << 22); // Clock GPIOA, DMA2
        RCC->APB2ENR |= (1 << 12);            // Clock SPI1

        // PA4 (WS), PA5 (CK), PA7 (SD) -> AF5
        GPIOA->MODER &= ~((0x3 << 8) | (0x3 << 10) | (0x3 << 14));
        GPIOA->MODER |=  ((0x2 << 8) | (0x2 << 10) | (0x2 << 14));
        GPIOA->OSPEEDR |= ((0x3 << 8) | (0x3 << 10) | (0x3 << 14));
        GPIOA->AFR[0] &= ~((0xF << 16) | (0xF << 20) | (0xF << 28));
        GPIOA->AFR[0] |=  ((0x5 << 16) | (0x5 << 20) | (0x5 << 28));

        DMA_Stream = DMA2_Stream2;
        dma_channel = 3; // DMA2_Stream2 Channel 3
        irqn = DMA2_Stream2_IRQn;

    } else if (SPIx == SPI3) { // ========== I2S3 (SLAVE RECEIVE) ==========
        RCC->AHB1ENR |= (1 << 0) | (1 << 1) | (1 << 21); // Clock GPIOA, GPIOB, DMA1
        RCC->APB1ENR |= (1 << 15);                       // Clock SPI3

        // 1. Chân PA15 (WS) -> AF6 (T? gi?i phóng JTDI c?a JTAG)
        GPIOA->MODER &= ~(0x3 << 30);     
        GPIOA->MODER |=  (0x2 << 30);
        GPIOA->OSPEEDR |= (0x3 << 30);    
        GPIOA->AFR[1] &= ~(0xF << 28);    
        GPIOA->AFR[1] |=  (0x6 << 28);    
        
        // 2. Chân PB3 (CK) và PB5 (SD) -> AF6 (PB3 gi?i phóng JTDO)
        GPIOB->MODER &= ~((0x3 << 6) | (0x3 << 10)); 
        GPIOB->MODER |=  ((0x2 << 6) | (0x2 << 10));
        GPIOB->OSPEEDR |= ((0x3 << 6) | (0x3 << 10));
        GPIOB->AFR[0] &= ~((0xF << 12) | (0xF << 20)); 
        GPIOB->AFR[0] |=  ((0x6 << 12) | (0x6 << 20)); 

        DMA_Stream = DMA1_Stream0;
        dma_channel = 0; // DMA1_Stream0 Channel 0
        irqn = DMA1_Stream0_IRQn;

    } else if (SPIx == SPI5) { // ========== I2S5 (SLAVE RECEIVE) ==========
        RCC->AHB1ENR |= (1 << 1) | (1 << 22); // Clock GPIOB, DMA2
        RCC->APB2ENR |= (1 << 20);            // Clock SPI5

        // PB0 (CK), PB1 (WS) -> AF6 n?m trong AFR[0]
        GPIOB->MODER &= ~((0x3 << 0) | (0x3 << 2));
        GPIOB->MODER |=  ((0x2 << 0) | (0x2 << 2));
        GPIOB->OSPEEDR |= ((0x3 << 0) | (0x3 << 2));
        GPIOB->AFR[0] &= ~((0xF << 0) | (0xF << 4));
        GPIOB->AFR[0] |=  ((0x6 << 0) | (0x6 << 4));

        // PB8 (SD) -> AF6 n?m trong AFR[1] (chân 8 -> index 0)
        GPIOB->MODER &= ~(0x3 << 16);
        GPIOB->MODER |=  (0x2 << 16);
        GPIOB->OSPEEDR |= (0x3 << 16);
        GPIOB->AFR[1] &= ~(0xF << 0);
        GPIOB->AFR[1] |=  (0x6 << 0);

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
        SPIx->I2SCFGR = (1 << 11) | (0x3 << 8) | (0x0 << 4) | (0x1 << 1) | (1 << 0);
        SPIx->I2SPR = 49; // Prescaler ch? c?n cài d?t cho Master d? phát dúng xung nh?p
    } else {
        // Slave Receive (I2SCFG = 01 -> Bit 9:8 = 0x1)
        SPIx->I2SCFGR = (1 << 11) | (0x1 << 8) | (0x0 << 4) | (0x1 << 1) | (1 << 0);
        SPIx->I2SPR = 2;  // M?c d?nh cho Slave
    }
    
    // B?t yêu c?u DMA phía ngo?i vi SPI/I2S (RXDMAEN)
    SPIx->CR2 |= (1 << 0); 

    // C?u hình DMA Stream
    DMA_Stream->CR &= ~(1 << 0);
    while (DMA_Stream->CR & (1 << 0)); // Ch? DMA d?ng hoàn toàn
    DMA_Stream->CR = 0; 

    // C?u hình thanh ghi DMA CR:
    // Channel selection, MSIZE=16-bit (01), PSIZE=16-bit (01), MINC=1, CIRC=1, Priority Very High (11)
    DMA_Stream->CR |= (dma_channel << 25) | (0x3 << 16) | (0x1 << 13) | (0x1 << 11) | (1 << 10) | (1 << 8);
    
    DMA_Stream->PAR = (uint32_t)&(SPIx->DR);     
    DMA_Stream->M0AR = (uint32_t)rx_buffer;        
    DMA_Stream->NDTR = I2S_RX_BUFFER_SIZE;

    // B?t ng?t Half-Transfer (HTIE - bit 3) và Transfer-Complete (TCIE - bit 4)
    DMA_Stream->CR |= (1 << 3) | (1 << 4); 
    NVIC_SetPriority(irqn, 1);
    NVIC_EnableIRQ(irqn);
}

void INMP441_8Mic_Init(void) {
    // 1. C?p xung PLLI2S
    I2S_PLLI2S_Init();
    
    // 2. Kh?i t?o thanh ghi GPIO, I2S và c?u hình s?n DMA cho c? 4 b?
    I2S_Config(SPI1, i2s1_rx_buffer); // Slave 1
    I2S_Config(SPI3, i2s3_rx_buffer); // Slave 2
    I2S_Config(SPI5, i2s5_rx_buffer); // Slave 3
    I2S_Config(SPI2, i2s2_rx_buffer); // Master

    // 3. KÍCH HO?T DMA TRU?C (S?n sàng nh?n d? li?u vào RAM)
    DMA2_Stream2->CR |= (1 << 0); // SPI1 DMA
    DMA1_Stream0->CR |= (1 << 0); // SPI3 DMA
    DMA2_Stream3->CR |= (1 << 0); // SPI5 DMA
    DMA1_Stream3->CR |= (1 << 0); // SPI2 DMA

    // 4. KÍCH HO?T 3 B? SLAVE TRU?C (Ð?ng ? tr?ng thái ch? xung SCK/WS t? ngoài)
    // Bit 10: I2SE = 1
    SPI1->I2SCFGR |= (1 << 10);
    SPI3->I2SCFGR |= (1 << 10);
    SPI5->I2SCFGR |= (1 << 10);

    // 5. KÍCH HO?T MASTER SAU CÙNG (Ngay khi I2SE c?a SPI2 b?t lên, xung b?t d?u phát ra)
    // Toàn b? 8 mic và 4 b? I2S b?t d?u thu nh?n ngay t?i c?nh xung d?u tiên!
    SPI2->I2SCFGR |= (1 << 10);
}

// ==================== CÁC TRÌNH PH?C V? NG?T DMA ====================

// I2S1 (Mic 3 & 4)
void DMA2_Stream2_IRQHandler(void) {
    if (DMA2->LISR & (1 << 20)) { DMA2->LIFCR = (1 << 20); i2s1_half = 1; }
    if (DMA2->LISR & (1 << 21)) { DMA2->LIFCR = (1 << 21); i2s1_full = 1; }
    DMA2->LIFCR = (1 << 19) | (1 << 18) | (1 << 16);
}

// I2S2 (Mic 1 & 2 - Master)
void DMA1_Stream3_IRQHandler(void) {
    if (DMA1->LISR & (1 << 26)) { DMA1->LIFCR = (1 << 26); i2s2_half = 1; }
    if (DMA1->LISR & (1 << 27)) { DMA1->LIFCR = (1 << 27); i2s2_full = 1; }
    DMA1->LIFCR = (1 << 25) | (1 << 24) | (1 << 22);
}

// I2S3 (Mic 5 & 6)
void DMA1_Stream0_IRQHandler(void) {
    if (DMA1->LISR & (1 << 4)) { DMA1->LIFCR = (1 << 4); i2s3_half = 1; }
    if (DMA1->LISR & (1 << 5)) { DMA1->LIFCR = (1 << 5); i2s3_full = 1; }
    DMA1->LIFCR = (1 << 3) | (1 << 2) | (1 << 0);
}

// I2S5 (Mic 7 & 8)
void DMA2_Stream3_IRQHandler(void) {
    if (DMA2->LISR & (1 << 26)) { DMA2->LIFCR = (1 << 26); i2s5_half = 1; }
    if (DMA2->LISR & (1 << 27)) { DMA2->LIFCR = (1 << 27); i2s5_full = 1; }
    DMA2->LIFCR = (1 << 25) | (1 << 24) | (1 << 22);
}