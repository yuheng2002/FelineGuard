/*
 * stm32f446xx.h
 *
 * Created on: 2026/1/3
 * Author: Yuheng
 *
 * Description:
 * MCU Specific Header File for STM32F446RE.
 * This file contains all the Base Addresses, Register Definition Structures,
 * and Generic Macros that are common across all drivers (GPIO, Timer, I2C, etc.).
 * Think of this as the "Dictionary" for the microcontroller.
 */

#ifndef SOURCES_STM32F446XX_H_
#define SOURCES_STM32F446XX_H_

#include <stdint.h>

/*
 * ==========================================
 * 1. Generic Macros for Bit Manipulation (newly added in Project 2)
 * ==========================================
 * These are used by all drivers.
 * NOTE: Added parentheses around arguments for safety (e.g., (REG))
 * improves code readability
 * NOTE: there can NOT be any space between the name and the parentheses
 * for example: SET_BIT(REG, BIT) is correct, but SET_BIT (REG, BIT) is not safe
 */
#define SET_BIT(REG, BIT)    ((REG) |= (1U << (BIT)))
#define CLEAR_BIT(REG, BIT)  ((REG) &= ~(1U << (BIT)))
#define READ_BIT(REG, BIT)   ((REG) & (1U << (BIT)))
#define TOGGLE_BIT(REG, BIT) ((REG) ^= (1U << (BIT)))

#define ENABLE 			1
#define DISABLE 		0
#define SET 			ENABLE
#define RESET 			DISABLE
#define GPIO_PIN_SET    SET
#define GPIO_PIN_RESET  RESET

/*
 * ==========================================
 * 2. Base Addresses
 * ==========================================
 * Memory Map from STM32F446 Reference Manual.
 */

/*
 * Bus Peripheral Base Addresses
 * NOTE: the address for each bus labeled in the memory map goes from bottom to top
 * the base address in each section is at the very bottom
 */
#define PERIPH_BASEADDR     0x40000000U
#define APB1_BASEADDR       (PERIPH_BASEADDR)
#define APB2_BASEADDR       0x40010000U
#define AHB1_BASEADDR       0x40020000U
#define AHB2_BASEADDR       0x50000000U

/*
 * AHB1 Peripherals
 */
#define GPIOA_BASEADDR      (AHB1_BASEADDR + 0x0000U)
#define GPIOB_BASEADDR      (AHB1_BASEADDR + 0x0400U)
#define GPIOC_BASEADDR      (AHB1_BASEADDR + 0x0800U)
#define GPIOD_BASEADDR      (AHB1_BASEADDR + 0x0C00U)
#define GPIOE_BASEADDR      (AHB1_BASEADDR + 0x1000U)
#define GPIOF_BASEADDR      (AHB1_BASEADDR + 0x1400U)
#define GPIOG_BASEADDR      (AHB1_BASEADDR + 0x1800U)
#define GPIOH_BASEADDR      (AHB1_BASEADDR + 0x1C00U)

/* RCC (Reset and Clock Control) Base Address.
 * We need this address to enable the clock for the GPIO peripherals.
 * Without enabling the clock in the RCC registers, GPIOs remain dead (powered down).
 */
#define RCC_BASEADDR        (AHB1_BASEADDR + 0x3800U) //0x40023800

/*
 * APB1 Peripherals (where TIM2 lives!)
 */
#define TIM2_BASEADDR       (APB1_BASEADDR) // 0x40000000
// #define TIM3_BASEADDR    (APB1_BASEADDR + 0x0400U) // For future use
// #define I2C1_BASEADDR    (APB1_BASEADDR + 0x5400U) // For future use
#define USART2_BASEADDR     (APB1_BASEADDR + 0x4400U) // USART2: 0x40004400

/*
 * APB2 Peripherals
 */
#define SYSCFG_BASEADDR     (APB2_BASEADDR + 0x3800U) // 0x40013800
#define EXTI_BASEADDR       (APB2_BASEADDR + 0x3C00U) // 0x40013C00
#define TIM1_BASEADDR       (APB2_BASEADDR + 0x0000U) // Advanced Timer


/*
 * ==========================================
 * 3. Register Definition Structures
 * ==========================================
 * Note on Logic:
 * We do not manually define offsets (e.g., +0x04).
 * Since C struct members are stored sequentially in memory and
 * uint32_t occupies exactly 4 bytes, the compiler automatically
 * aligns these members to the hardware offsets (0x00, 0x04, 0x08...)
 * relative to the base address.
 */
typedef struct{
	volatile uint32_t MODER;    // Mode Register: Configures the pin direction (Input, Output, etc.)
	volatile uint32_t OTYPER;   // Output Type: Chooses between Push-Pull (strong drive) or Open-Drain.
	volatile uint32_t OSPEEDR;  // Output Speed: Determines how fast the pin can toggle states (slew rate).
	volatile uint32_t PUPDR;    // Pull-up/Pull-down: internal resistors to stabilize floating pins.
	volatile uint32_t IDR;      // Input Data: Read this to get the current signal level on the pin.
	volatile uint32_t ODR;      // Output Data: Write to this to set the pin level (High/Low).
	volatile uint32_t BSRR;     // Bit Set/Reset: Atomic way to set/reset pins (safer than ODR in interrupts).
	volatile uint32_t LCKR;     // Lock Register: Freezes the configuration to prevent accidental changes.
	volatile uint32_t AFR[2]; // Offset: 0x20
							  // AFR[0] is AFRL (Offset 0x20) for pins 0-7
							  // AFR[1] is AFRH (Offset 0x24) for pins 8-15
} GPIO_RegDef_t;

/* RCC Register Def (Important for Timer Driver!) */
typedef struct{
	volatile uint32_t CR; // clock control register,           offset: 0x00
	volatile uint32_t PLLCFGR; // PLL configuration register,  offset: 0x04
	volatile uint32_t CFGR; // clock configuration register,   offset: 0x08
	volatile uint32_t CIR; // clock interrupt register,        offset: 0x0C
	volatile uint32_t AHB1RSTR; // AHB1 reset register,        offset: 0x10
	volatile uint32_t AHB2RSTR; // AHB2 reset register,        offset: 0x14
	volatile uint32_t AHB3RSTR; // AHB3 reset register,        offset: 0x18
	uint32_t Reserved0; //placeholder between 0x18 (24) and 0x20 (32), offset: 0x1C
	volatile uint32_t APB1RSTR; // APB1 reset register,        offset: 0x20
	volatile uint32_t APB2RSTR; // APB2 reset register,        offset: 0x24
	uint32_t Reserved1[2]; // placeholder between 0x24 (36) and 0x30 (48), offset: 0x28 - 0x2C
	volatile uint32_t AHB1ENR; // AHB1 clock enable register,  offset: 0x30
							   // GPIO Clock is here
	volatile uint32_t AHB2ENR; // AHB2 clock enable register,  offset: 0x34
	volatile uint32_t AHB3ENR; // AHB3 clock enable register,  offset: 0x38
	uint32_t Reserved3; // placeholder between 0x38 (36) and 0x40 (48), offset: 0x3C
	volatile uint32_t APB1ENR; // APB1 clock enable register,  offset: 0x40
							   // TIM2 Clock is here!
	volatile uint32_t APB2ENR; // APB2 clock enable register,  offset: 0x44
	// ... there are more registers, but this is enough for now
} RCC_RegDef_t;

/*
 * ==========================================
 * EXTI Register Definition Structure
 * ==========================================
 */
typedef struct{
	volatile uint32_t IMR; // interrupt mask register -> offset 0x00
	volatile uint32_t EMR; // event mask register -> offset 0x04
	volatile uint32_t RTSR; // rising trigger selection register -> offset 0x08
	volatile uint32_t FTSR; // falling trigger selection register -> offset 0x0C
	volatile uint32_t SWIER; // software interrupt event register -> offset 0x10
	volatile uint32_t PR; // pending register -> offset 0x14
} EXTI_RegDef_t;

/*
 * ==========================================
 * SYSCFG Register Definition Structure
 * ==========================================
 */
typedef struct{
	volatile uint32_t MEMRMP; // memory remap register -> offset 0x00
	volatile uint32_t PMC; // peripheral mode configuration register -> offset 0x04
	volatile uint32_t EXTICR[4]; // external interrupt configuration register 1 -> offset 0x08
					  // external interrupt configuration register 2 -> offset 0x0C
					  // external interrupt configuration register 3 -> offset 0x10
					  // external interrupt configuration register 4 -> offset 0x14
} SYSCFG_RegDef_t;

/*
 * ==========================================
 * TIM (General Purpose Timer) Register Structure
 * Applicable for TIM2 - TIM5 (General Purpose)
 * Refer to RM0390 - 17.4.21 TIMx register map
 * ==========================================
 */
typedef struct {
    volatile uint32_t CR1;      // Control register 1,              Offset: 0x00
    volatile uint32_t CR2;      // Control register 2,              Offset: 0x04
    volatile uint32_t SMCR;     // Slave mode control register,     Offset: 0x08
    volatile uint32_t DIER;     // DMA/Interrupt enable register,   Offset: 0x0C
    volatile uint32_t SR;       // Status register,                 Offset: 0x10
    volatile uint32_t EGR;      // Event generation register,       Offset: 0x14
    volatile uint32_t CCMR1;    // Capture/compare mode register 1, Offset: 0x18
    volatile uint32_t CCMR2;    // Capture/compare mode register 2, Offset: 0x1C
    volatile uint32_t CCER;     // Capture/compare enable register, Offset: 0x20
    volatile uint32_t CNT;      // Counter,                         Offset: 0x24
    volatile uint32_t PSC;      // Prescaler,                       Offset: 0x28
    volatile uint32_t ARR;      // Auto-reload register,            Offset: 0x2C
    volatile uint32_t Reserved1;//                                  Offset: 0x30
    volatile uint32_t CCR1;     // Capture/compare register 1,      Offset: 0x34
    volatile uint32_t CCR2;     // Capture/compare register 2,      Offset: 0x38
    volatile uint32_t CCR3;     // Capture/compare register 3,      Offset: 0x3C
    volatile uint32_t CCR4;     // Capture/compare register 4,      Offset: 0x40
    volatile uint32_t Reserved2;//                                  Offset: 0x44
    volatile uint32_t DCR;      // DMA control register,            Offset: 0x48
    volatile uint32_t DMAR;     // DMA address for full transfer,   Offset: 0x4C
} TIM_RegDef_t;

/*
 * ==========================================
 * NVIC (Nested Vectored Interrupt Controller) Register Structure Definition
 * ==========================================
 * Cortex-M4 Specific Registers (Refer to PM0214 Section 4.3.2))
 *
 * The Cortex-M4 generic user guide defines 8 ISER registers (ISER0 to ISER7).
 * Each register is 32-bits wide and controls 32 interrupts.
 * Total supported interrupts = 8 * 32 = 256.
 * (STM32F446 only uses about 90 of them, so ISER3-7 might be reserved/unused, but mapping is standard).
 * NVIC_ISER0 bits 0 to 31 are for interrupt 0 to 31, respectively
 * NVIC_ISER1 bits 0 to 31 are for interrupt 32 to 63, respectively
 * ....
 * NVIC_ISER6 bits 0 to 31 are for interrupt 192 to 223, respectively
 * NVIC_ISER7 bits 0 to 15 are for interrupt 224 to 239, respectively
 */
typedef struct{
	volatile uint32_t ISER[8]; // ISER[0] -> IRQ 0-31
							   // ISER[1] -> IRQ 32-63
							   // ...
} NVIC_ISER_RegDef_t;

// NVIC ISER (Interrupt Set-Enable Registers) Base Address
// 0xE000E100 - 0xE000E11F -> NVIC_ISER0 - NVIC_ISER7
#define NVIC_ISER_BASE_ADDR 0xE000E100U // according to pm0214 manual

/*
 * ==========================================
 * 			USART Register Map
 * ==========================================
 * using USART2 for testing purposes through serial print
 * following um1724 manual's recommendation (page 26)
 */
typedef struct{
	volatile uint32_t SR;   // Status register, 				   Offset: 0x00
	volatile uint32_t DR;   // Data register,   				   Offset: 0x04
	volatile uint32_t BRR;  // Baud rate register,   			   Offset: 0x08
	volatile uint32_t CR1;  // Control register 1,   			   Offset: 0x0C
	volatile uint32_t CR2;  // Control register 2,   			   Offset: 0x10
	volatile uint32_t CR3;  // Control register 3,   			   Offset: 0x14
	volatile uint32_t GTPR; // Guard time and pre-scalar register, Offset: 0x18
} USART_RegDef_t;

/*
 * ==========================================
 * 4. Peripheral Definitions (Typecasting)
 * ==========================================
 * Creating "Objects" for our peripherals.
 * Converts the raw address (uint32_t) into a pointer to our structure.
 * This acts as the "key" to access the hardware.
 * Usage: GPIOA->MODER = ...
 */

#define GPIOA   ((GPIO_RegDef_t*)GPIOA_BASEADDR)
#define GPIOB   ((GPIO_RegDef_t*)GPIOB_BASEADDR)
#define GPIOC   ((GPIO_RegDef_t*)GPIOC_BASEADDR)
#define GPIOD   ((GPIO_RegDef_t*)GPIOD_BASEADDR)
#define GPIOE   ((GPIO_RegDef_t*)GPIOE_BASEADDR)
#define GPIOF   ((GPIO_RegDef_t*)GPIOF_BASEADDR)
#define GPIOG   ((GPIO_RegDef_t*)GPIOG_BASEADDR)
#define GPIOH   ((GPIO_RegDef_t*)GPIOH_BASEADDR)

/*
 * Converts the raw address (uint32_t) into a pointer to our structure.
 * This acts as the "key" to access the hardware.
 * Usage: EXTI->IMR = ...
 *        SYSCFG->PMC...
 * Note: MUST add outer parentheses to enforce order of operations.
 * Without ( ), the '->' operator would try to access the member of the
 * raw address value (0x40013C00) before casting, causing a compile error.
 */
#define RCC     ((RCC_RegDef_t*)RCC_BASEADDR)
#define EXTI    ((EXTI_RegDef_t*)EXTI_BASEADDR)
#define SYSCFG  ((SYSCFG_RegDef_t*)SYSCFG_BASEADDR)
#define NVIC_ISER ((NVIC_ISER_RegDef_t*)NVIC_ISER_BASE_ADDR)

// Project 2: Timer definition
#define TIM2    ((TIM_RegDef_t*)TIM2_BASEADDR)
// We will define TIM_RegDef_t in Timer driver or here later

/*
 * ==========================================
 *   USART2 (added for testing purposes)
 * ==========================================
 * Reference: RM0390 Reference Manual - USART Register map
 */
#define USART2  ((USART_RegDef_t*)USART2_BASEADDR)

/*
 * ==========================================
 * 5. Interrupt Macros
 * EXTI Line [15:10] interrupts.
// Pin 13 shares this IRQ line with Pin 10, 11, 12, 14, 15.
// See RM0390 Vector Table (Position 40)
 * ==========================================
 */
#define EXTI15_10_IRQ (40)

#endif /* SOURCES_STM32F446XX_H_ */
