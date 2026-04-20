#ifndef STM32F411XE_H
#define STM32F411XE_H

#include <stdint.h>

// --- GPIO ---
typedef struct {
    volatile uint32_t MODER;    // 0x00
    volatile uint32_t OTYPER;   // 0x04
    volatile uint32_t OSPEEDR;  // 0x08
    volatile uint32_t PUPDR;    // 0x0C
    volatile uint32_t IDR;      // 0x10
    volatile uint32_t ODR;      // 0x14
    volatile uint32_t BSRR;     // 0x18
    volatile uint32_t LCKR;     // 0x1C
    volatile uint32_t AFRL;     // 0x20
    volatile uint32_t AFRH;     // 0x24
} GPIO_Typedef;

// --- RCC ---
typedef struct {
    volatile uint32_t CR;       // 0x00
    volatile uint32_t PLLCFGR;  // 0x04
    volatile uint32_t CFGR;     // 0x08
    volatile uint32_t CIR;      // 0x0C
    volatile uint32_t AHB1RSTR; // 0x10
    volatile uint32_t AHB2RSTR; // 0x14
    volatile uint32_t RESERVED0[2];
    volatile uint32_t APB1RSTR; // 0x20
    volatile uint32_t APB2RSTR; // 0x24
    volatile uint32_t RESERVED1[2];
    volatile uint32_t AHB1ENR;  // 0x30
    volatile uint32_t AHB2ENR;  // 0x34
    volatile uint32_t RESERVED2[2];
    volatile uint32_t APB1ENR;  // 0x40 - Target for USART2
    volatile uint32_t APB2ENR;  // 0x44 - Target for USART1/6
} RCC_Typedef;

// --- USART ---
typedef struct {
    volatile uint32_t SR;       // 0x00
    volatile uint32_t DR;       // 0x04
    volatile uint32_t BRR;      // 0x08
    volatile uint32_t CR1;      // 0x0C
    volatile uint32_t CR2;      // 0x10
    volatile uint32_t CR3;      // 0x14
} USART_Typedef;

#define GPIOA_BASE   0x40020000
#define RCC_BASE     0x40023800
#define USART2_BASE  0x40004400 // On APB1

#define GPIOA   ((GPIO_Typedef *) GPIOA_BASE)
#define RCC     ((RCC_Typedef *) RCC_BASE)
#define USART2  ((USART_Typedef *) USART2_BASE)

#endif
