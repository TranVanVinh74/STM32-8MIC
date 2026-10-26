#ifndef AUDIO_PROTOCOL_H
#define AUDIO_PROTOCOL_H
#include <stdint.h>
#define AUDIO_HEADER_SIZE 32U
/* Little-endian header: magic[8], sequence u32, sample_rate u32,
 * channels u16, frames u16, payload_bytes u32, fault_flags u32, crc32 u32.
 * CRC-32/ISO-HDLC covers header bytes 0..27 followed by payload.
 * Matches Python zlib.crc32; NOT STM32 hardware CRC defaults.
 */
static uint32_t audio_crc_update(uint32_t crc, const uint8_t *p, uint32_t n)
{
    static const uint32_t table[16] = {
        0x00000000U,0x1DB71064U,0x3B6E20C8U,0x26D930ACU,
        0x76DC4190U,0x6B6B51F4U,0x4DB26158U,0x5005713CU,
        0xEDB88320U,0xF00F9344U,0xD6D6A3E8U,0xCB61B38CU,
        0x9B64C2B0U,0x86D3D2D4U,0xA00AE278U,0xBDBDF21CU
    };
    while (n--) {
        crc ^= *p++;
        crc = (crc >> 4) ^ table[crc & 15U];
        crc = (crc >> 4) ^ table[crc & 15U];
    }
    return crc;
}
static void audio_put16(uint8_t *p, uint16_t x)
{ p[0]=(uint8_t)x; p[1]=(uint8_t)(x>>8); }
static void audio_put32(uint8_t *p, uint32_t x)
{ for (unsigned i=0;i<4;++i) p[i]=(uint8_t)(x>>(i*8U)); }
static void audio_header(uint8_t *h, uint32_t seq, uint32_t rate,
                         uint16_t frames, uint32_t faults, const uint8_t *pcm)
{
    static const uint8_t magic[8] = {'8','M','I','C','P','C','M','1'};
    uint32_t bytes = (uint32_t)frames * 8U * 2U;
    for (unsigned i=0;i<8;++i) h[i]=magic[i];
    audio_put32(h+8,seq); audio_put32(h+12,rate);
    audio_put16(h+16,8U); audio_put16(h+18,frames);
    audio_put32(h+20,bytes); audio_put32(h+24,faults);
    uint32_t crc=audio_crc_update(0xFFFFFFFFU,h,28U);
    crc=audio_crc_update(crc,pcm,bytes)^0xFFFFFFFFU;
    audio_put32(h+28,crc);
}
#endif
