#include "stm32f411xe.h"
#include <stdint.h>

// ============================================================================
// 1. PACKET ARCHITECTURE (9 Bytes Total)
// ============================================================================
#pragma pack(push, 1)
typedef struct {
    uint16_t header; // 0x55AA (Transmitted as 0xAA, 0x55 due to little-endian)
    uint8_t  seq;    // 0-255 rolling counter
    float    temp;   // 4-byte IEEE 754 float
    uint16_t crc;    // 2-byte CCITT-False checksum
} SensorPacket;
#pragma pack(pop)

// ============================================================================
// 2. HARDWARE DRIVERS (Bare-Metal USART2 on PA2)
// ============================================================================
void sys_init(void) {
    // A. Ensure High-Speed Internal (HSI) clock is ON and ready (16MHz)
    RCC->CR |= (1 << 0); 
    while (!(RCC->CR & (1 << 1)));

    // B. Enable GPIOA and USART2 clocks
    RCC->AHB1ENR |= (1 << 0);   // GPIOA
    RCC->APB1ENR |= (1 << 17);  // USART2

    // C. Configure PA2 (TX) for Alternate Function 7 (USART2)
    GPIOA->MODER &= ~(3 << (2 * 2)); // Clear mode for PA2
    GPIOA->MODER |=  (2 << (2 * 2)); // Set AF mode for PA2
    GPIOA->AFRL  &= ~(0xF << (2 * 4)); // Clear AF bits
    GPIOA->AFRL  |=  (0x7 << (2 * 4)); // Set AF7 (0111)

    // D. Configure Baud Rate: 115200 @ 16MHz
    // BRR = 16MHz / (16 * 115200) = 8.6805 -> Mantissa=8, Fraction=11
    USART2->BRR = (8 << 4) | 11;

    // E. Enable USART and Transmitter
    USART2->CR1 |= (1 << 13) | (1 << 3); // UE (USART Enable) and TE (Transmit Enable)
}

void uart_write(uint8_t c) {
    // Wait for Transmit Data Register Empty (TXE) flag
    while (!(USART2->SR & (1 << 7))); 
    USART2->DR = c;
}

// ============================================================================
// 3. DATA INTEGRITY & TRANSMISSION
// ============================================================================
// Calculates CRC-16-CCITT (Polynomial: 0x1021, Initial: 0xFFFF)
uint16_t calculate_crc16(const uint8_t *data, uint16_t length) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < length; i++) {
        crc ^= (uint16_t)(data[i] << 8);
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else crc <<= 1;
        }
    }
    return crc;
}

void blast_packet(uint8_t seq, float temp) {
    SensorPacket pkt;
    pkt.header = 0x55AA;
    pkt.seq = seq;
    pkt.temp = temp;
    
    // Calculate CRC over the sequence and payload (Total 5 bytes)
    // We do NOT include the header in the CRC calculation.
    pkt.crc = calculate_crc16((uint8_t*)&pkt.seq, sizeof(pkt.seq) + sizeof(pkt.temp));

    // Blast the 9 bytes over the wire sequentially
    uint8_t *ptr = (uint8_t*)&pkt;
    for (uint32_t i = 0; i < sizeof(SensorPacket); i++) {
        uart_write(ptr[i]);
    }
}

// ============================================================================
// 4. MAIN EXECUTION LOOP
// ============================================================================
int main(void) {
    sys_init(); // Boot the hardware
    
    uint8_t sequence_num = 0;
    float current_temp = 24.5f;

    while (1) {
        // Build and transmit the packet
        blast_packet(sequence_num++, current_temp);
        
        // Simulate sensor changes
        current_temp += 0.1f;
        if (current_temp > 30.0f) current_temp = 24.5f;

        // Deterministic delay (~500ms at 16MHz)
        for (volatile int i = 0; i < 500000; i++) {
            __asm__("nop");
        }
    }
    
    return 0;
}
