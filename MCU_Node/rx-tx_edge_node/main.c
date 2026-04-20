#include "stm32f411xe.h"
#include <stdint.h>

volatile uint8_t cooling_active = 0;

// --- Packet Architecture ---
#pragma pack(push, 1)
typedef struct {
    uint16_t header; // 0x55AA
    uint8_t  seq;
    float    temp;
    uint16_t crc;
} SensorPacket;
#pragma pack(pop)

// --- Hardware Initialization ---
void sys_init(void) {
    // 1. HSI Clock (16MHz)
    RCC->CR |= (1 << 0); 
    while (!(RCC->CR & (1 << 1)));

    // 2. Enable Clocks: GPIOA (UART), GPIOB (External LED), USART2
    // Bit 0 = GPIOA, Bit 1 = GPIOB
    RCC->AHB1ENR |= (1 << 0) | (1 << 1); 
    RCC->APB1ENR |= (1 << 17);  

    // 3. Configure PB0 as General Purpose Output
    GPIOB->MODER &= ~(3 << (0 * 2)); // Clear mode for PB0
    GPIOB->MODER |=  (1 << (0 * 2)); // Set PB0 to Output mode
    GPIOB->BSRR = (1 << (0 + 16));   // Turn LED OFF initially (Write 1 to upper 16 bits to reset/clear)

    // 4. Configure PA2 (TX) and PA3 (RX) for Alternate Function 7
    GPIOA->MODER &= ~((3 << (2 * 2)) | (3 << (3 * 2))); 
    GPIOA->MODER |=  ((2 << (2 * 2)) | (2 << (3 * 2))); 
    GPIOA->AFRL  &= ~((0xF << (2 * 4)) | (0xF << (3 * 4))); 
    GPIOA->AFRL  |=  ((0x7 << (2 * 4)) | (0x7 << (3 * 4))); 

    // 5. Baud Rate: 115200 @ 16MHz
    USART2->BRR = (8 << 4) | 11;

    // 6. Enable USART, TX, RX, and RXNE Interrupt
    USART2->CR1 |= (1 << 13) | (1 << 3) | (1 << 2) | (1 << 5);

    // 7. Enable USART2 Interrupt in the NVIC (Position 38)
    NVIC->ISER[1] |= (1 << (38 - 32)); 
}

// --- The Hardware Interrupt Routine (RX) ---
void USART2_IRQHandler(void) {
    static uint8_t rx_state = 0;
    
    if (USART2->SR & (1 << 5)) { 
        uint8_t byte = USART2->DR; 
        
        if (rx_state == 0 && byte == 0xBB) rx_state = 1;
        else if (rx_state == 1 && byte == 0x66) rx_state = 2;
        else if (rx_state == 2) {
            cooling_active = byte; 
            rx_state = 0;          
            
            // Actuate the External Relay (LED on PB0 - Active HIGH)
            if (cooling_active == 0x01) {
                GPIOB->BSRR = (1 << 0);          // LED ON  (Set bit 0)
            } else {
                GPIOB->BSRR = (1 << (0 + 16));   // LED OFF (Reset bit 0)
            }
        } else {
            rx_state = 0;
        }
    }
}


// --- Telemetry Transmission ---
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
    pkt.crc = calculate_crc16((uint8_t*)&pkt.seq, 5);

    uint8_t *ptr = (uint8_t*)&pkt;
    for (uint32_t i = 0; i < sizeof(SensorPacket); i++) {
        while (!(USART2->SR & (1 << 7))); 
        USART2->DR = ptr[i];
    }
}

// --- Main Loop ---
int main(void) {
    sys_init(); 
    uint8_t sequence_num = 0;
    float current_temp = 24.5f;

    while (1) {
        blast_packet(sequence_num++, current_temp);
        
        current_temp += 0.1f;
        if (current_temp > 30.0f) current_temp = 24.5f;

        for (volatile int i = 0; i < 500000; i++) __asm__("nop"); 
    }
    return 0;
}
