#ifndef MOCK_STM32_H
#define MOCK_STM32_H
#include <stdint.h>
typedef struct { volatile uint32_t CR,NDTR,PAR,M0AR,FCR; } DMA_Stream_TypeDef;
typedef struct { volatile uint32_t I2SCFGR,I2SPR,CR2,SR,DR; } SPI_TypeDef;
typedef struct { volatile uint32_t AHB1ENR,APB1ENR,APB2ENR,APB1RSTR,APB2RSTR,CR,CFGR,PLLI2SCFGR; } RCC_TypeDef;
typedef struct { volatile uint32_t MODER,OSPEEDR,AFR[2],IDR,BSRR; } GPIO_TypeDef;
typedef struct { volatile uint32_t LISR,LIFCR; } DMA_TypeDef;
typedef int IRQn_Type;
static RCC_TypeDef rcc;
static GPIO_TypeDef gpioa,gpiob;
static DMA_TypeDef dma1,dma2;
static SPI_TypeDef spi1,spi2,spi3,spi5;
static DMA_Stream_TypeDef s13,s10,s22,s23;
#define RCC (&rcc)
#define GPIOA (&gpioa)
#define GPIOB (&gpiob)
#define DMA1 (&dma1)
#define DMA2 (&dma2)
#define SPI1 (&spi1)
#define SPI2 (&spi2)
#define SPI3 (&spi3)
#define SPI5 (&spi5)
#define DMA1_Stream3 (&s13)
#define DMA1_Stream0 (&s10)
#define DMA2_Stream2 (&s22)
#define DMA2_Stream3 (&s23)
#define DMA1_Stream3_IRQn 0
#define DMA1_Stream0_IRQn 1
#define DMA2_Stream2_IRQn 2
#define DMA2_Stream3_IRQn 3
#define NVIC_SetPriority(a,b) ((void)(a),(void)(b))
#define NVIC_EnableIRQ(a) ((void)(a))
#define __DMB() ((void)0)
#define __DSB() ((void)0)
#endif
