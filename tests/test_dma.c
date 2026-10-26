/* Exercises the actual I2S_CopyBlock/IRQ code with fake registers.
 * Does not simulate real clocks, interrupts racing the CPU, or I2S framing. */
#include <assert.h>
#include <string.h>
#include "../FFT/B1/lib1/I2S.c"
void Delay_ms(uint32_t ms) { (void)ms; }
uint32_t millis(void) { return 0; }
static int16_t output[512*8];
static void reset_state(void) {
    audio_faults=0;
    for (unsigned i=0;i<4;++i) {
        completed[i]=0;
        streams[i]->NDTR=4096;
        peripherals[i]->SR=0;
    }
}
int main(void) {
    reset_state();
    completed[0]=1; streams[0]->NDTR=2040;
    assert(I2S_CopyBlock(1,output)==0); /* cannot read before slaves finish */
    for(unsigned pair=0;pair<4;++pair) {
        completed[pair]=1; streams[pair]->NDTR=2040;
        for(unsigned n=0;n<512;++n) {
            buffers[pair][4*n]=(uint16_t)(-1000+(int)(n*8+pair*2));
            buffers[pair][4*n+1]=0x1234; /* low words must not leak */
            buffers[pair][4*n+2]=(uint16_t)(-1000+(int)(n*8+pair*2+1));
            buffers[pair][4*n+3]=0x5678;
        }
    }
    assert(I2S_CopyBlock(1,output)==1);
    for(unsigned n=0;n<4096;++n) assert(output[n]==-1000+(int)n);
    for(unsigned pair=0;pair<4;++pair) {
        completed[pair]=2; streams[pair]->NDTR=4000;
        for(unsigned n=0;n<2048;++n) buffers[pair][2048+n]=buffers[pair][n];
    }
    assert(I2S_CopyBlock(2,output)==1);
    for(unsigned n=0;n<4096;++n) assert(output[n]==-1000+(int)n);
    streams[1]->NDTR=1000; /* next half is already writing the requested data */
    assert(I2S_CopyBlock(2,output)==-1 && (audio_faults & 256));
    reset_state();
    DMA1->LISR=1U<<26; DMA1_Stream3_IRQHandler();
    assert(completed[0]==1 && !audio_faults);
    assert(DMA1->LIFCR==(0x3DU<<22));
    DMA1->LISR=1U<<27; DMA1_Stream3_IRQHandler();
    assert(completed[0]==2 && !audio_faults);
    DMA2->LISR=1U<<21; DMA2_Stream2_IRQHandler(); /* TC before HT */
    assert(audio_faults & 256);
    reset_state();
    DMA2->LISR=(1U<<26)|(1U<<27); DMA2_Stream3_IRQHandler();
    assert(audio_faults & 256);
    reset_state();
    DMA1->LISR=1U<<3; DMA1_Stream0_IRQHandler(); /* transfer error I2S3 */
    assert(audio_faults==4);
    reset_state(); spi5.SR=1U<<6;
    assert(I2S_CopyBlock(1,output)==-1 && audio_faults==128);
    reset_state(); completed[2]=2;
    assert(I2S_CopyBlock(1,output)==-1 && (audio_faults & 256));
    return 0;
}
