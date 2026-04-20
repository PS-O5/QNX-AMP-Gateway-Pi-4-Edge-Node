#include "stm32f411xe.h"

void sys_init(void) {
    // 1. Ensure HSI is ON and ready (Internal 16MHz)
    RCC->CR |= (1 << 0); 
    while (!(RCC->CR & (1 << 1)));

    // 2. Enable GPIOA and USART2 clocks
    RCC->AHB1ENR |= (1 << 0);   // GPIOA
    RCC->APB1ENR |= (1 << 17);  // USART2

    // 3. Configure PA2 (TX) for AF7 (USART2)
    // Mode: Alternate Function (10)
    GPIOA->MODER &= ~(3 << (2 * 2));
    GPIOA->MODER |=  (2 << (2 * 2));
    // AF7 (0111) in AFRL register (PA2 is pin 2)
    GPIOA->AFRL &= ~(0xF << (2 * 4));
    GPIOA->AFRL |=  (0x7 << (2 * 4));

    // 4. Configure Baud Rate: 115200 @ 16MHz
    // BRR = 16MHz / (16 * 115200) = 8.6805
    // Mantissa = 8, Fraction = 0.6805 * 16 = 11 (0x0B)
    USART2->BRR = (8 << 4) | 11;

    // 5. Enable USART and Transmitter
    USART2->CR1 |= (1 << 13) | (1 << 3);
}

void uart_write(char c) {
    while (!(USART2->SR & (1 << 7))); // Wait for TXE (Transmit Data Register Empty)
    USART2->DR = c;
}

void print_sensor(const char *s) {
    while (*s) uart_write(*s++);
}

int main(void) {
    sys_init();
    while (1) {
        print_sensor("TEMP:24.5C\r\n");
        for (int i = 0; i < 1000000; i++) __asm__("nop"); // Spin delay
    }
}
