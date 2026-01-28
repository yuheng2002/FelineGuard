/*
 * stm32f446xx_gpio_driver.h
 *
 * Created on: 2025/12/23
 * Author: Yuheng
 *
 * Updated for Project 2:
 * - Removed generic definitions (moved to stm32f446xx.h)
 * - Kept only GPIO-specific configurations and APIs
 */

#ifndef SOURCES_STM32F446XX_GPIO_DRIVER_H_
#define SOURCES_STM32F446XX_GPIO_DRIVER_H_

#include <stdint.h>
#include "stm32f446xx.h" // <--- The Master Header!


/*
 * ==========================================
 * 1. Configuration Structures
 * ==========================================
 */
/*
 * GPIO Pin Configuration Settings
 * User menu to configure a specific pin.
 */
typedef struct{
	uint8_t GPIO_PinNumber;         // Possible values: 0-15
	uint8_t GPIO_PinMode;           // Possible values: @GPIO_PIN_MODES
	uint8_t GPIO_PinSpeed;          // Possible values: @GPIO_PIN_SPEED
	uint8_t GPIO_PinPuPdControl;    // Possible values: @GPIO_PIN_PULL_UP_DOWN
	uint8_t GPIO_PinOPType;         // Possible values: @GPIO_PIN_OUTPUT_TYPES
	uint8_t GPIO_PinAltFunMode;     // Possible values: 0-15 (Only applicable if Mode == AltFn)
} GPIO_PinConfig_t;

/*
 * GPIO Handle Structure
 * This acts as the "Job Order".
 * It bundles the "Where" (pGPIOx base address) and the "How" (PinConfig).
 * We pass this single object to functions like GPIO_Init().
 */
typedef struct{
	GPIO_RegDef_t *pGPIOx;           // Pointer to the GPIO Port Base Address
	GPIO_PinConfig_t GPIO_PinConfig; // The configuration settings for the specific pin
} GPIO_Handle_t;

/*
 * ==========================================
 * 2. Configuration Macros (GPIO Specific)
 * ==========================================
 * Macros to prevent user errors (magic numbers).
 */
/* @GPIO_PIN_MODES */
#define GPIO_MODE_IN 		0
#define GPIO_MODE_OUT 		1
#define GPIO_MODE_ALTN 		2
#define GPIO_MODE_ANALOG 	3
#define GPIO_MODE_IT_FT 	4 	// Interrupt Falling EdgeS
#define GPIO_MODE_IT_RT 	5 	// Interrupt Rising Edge
#define GPIO_MODE_IT_RFT 	6 	// Interrupt Rising & Falling Edge

/* @GPIO_PIN_OUTPUT_TYPES */
#define GPIO_OP_TYPE_PP 	0 	// Push Pull (Default)
#define GPIO_OP_TYPE_OD 	1 	// Open Drain (Used for I2C)

/* @GPIO_PIN_SPEED */
#define GPIO_SPEED_LOW 			0
#define GPIO_SPEED_MEDIUM 		1
#define GPIO_SPEED_HIGH 		2
#define GPIO_SPEED_VERY_HIGH 	3

/* @GPIO_PIN_PULL_UP_DOWN */
#define GPIO_NO_PUPD 		0   // No Pull-up/Pull-down (Floating)
#define GPIO_PIN_PU 		1   // Pull-up resistor enabled
#define GPIO_PIN_PD 		2   // Pull-down resistor enabled

/* @GPIO_PIN_AF_MODES */
#define GPIO_AF_0            0
#define GPIO_AF_1            1
#define GPIO_AF_2            2
#define GPIO_AF_3            3
#define GPIO_AF_4            4
#define GPIO_AF_5            5
#define GPIO_AF_6            6
#define GPIO_AF_7            7
#define GPIO_AF_8            8
#define GPIO_AF_9            9
#define GPIO_AF_10           10
#define GPIO_AF_11           11
#define GPIO_AF_12           12
#define GPIO_AF_13           13
#define GPIO_AF_14           14
#define GPIO_AF_15           15


/*
 * ==========================================
 * 3. Clock Enable Macros
 * ==========================================
 * These macros toggle specific bits in the RCC_AHB1ENR register.
 * Logic: Base ADDR of RCC + Offset of AHB1ENR (0x30).
 * Bit 0 = GPIOA, Bit 1 = GPIOB, etc.
 * Kept here for backward compatibility with Project 1 code.
 * (Alternatively, could use RCC->AHB1ENR structure access)
 */
#define GPIOA_PCLK_EN()     ( RCC->AHB1ENR |= (1 << 0) )
#define GPIOB_PCLK_EN()     ( RCC->AHB1ENR |= (1 << 1) )
#define GPIOC_PCLK_EN()     ( RCC->AHB1ENR |= (1 << 2) )
#define GPIOD_PCLK_EN()     ( RCC->AHB1ENR |= (1 << 3) )
#define GPIOE_PCLK_EN()     ( RCC->AHB1ENR |= (1 << 4) )
#define GPIOF_PCLK_EN()     ( RCC->AHB1ENR |= (1 << 5) )
#define GPIOG_PCLK_EN()     ( RCC->AHB1ENR |= (1 << 6) )
#define GPIOH_PCLK_EN()     ( RCC->AHB1ENR |= (1 << 7) )

/*
 * SYSCFG Clock Enable
 * SYSCFG is connected to APB2 Bus
 * Bit 14 of RCC_APB2ENR controls the clock
 * */
#define RCC_APB2ENR_OFFSET 0x44U
#define RCC_APB2ENR_ADDR (RCC_BASEADDR + RCC_APB2ENR_OFFSET)
#define SYSCFG_PCLK_EN() ( *(volatile uint32_t*)(RCC_APB2ENR_ADDR) |= (1 << 14) )

/*
 * ==========================================
 * 4. Clock Disable Macros
 * ==========================================
 * These macros disable specific bits in the RCC_AHB1ENR register.
 * Logic: Base ADDR of RCC + Offset of AHB1ENR (0x30).
 * Bit 0 = GPIOA, Bit 1 = GPIOB, etc.
 * set to 0 -> disabled
 */
#define GPIOA_PCLK_DIS()    ( *(volatile uint32_t*)(RCC_BASEADDR + 0x30) &= ~(1 << 0) )
#define GPIOB_PCLK_DIS()    ( *(volatile uint32_t*)(RCC_BASEADDR + 0x30) &= ~(1 << 1) )
#define GPIOC_PCLK_DIS()    ( *(volatile uint32_t*)(RCC_BASEADDR + 0x30) &= ~(1 << 2) )
#define GPIOD_PCLK_DIS()    ( *(volatile uint32_t*)(RCC_BASEADDR + 0x30) &= ~(1 << 3) )
#define GPIOE_PCLK_DIS()    ( *(volatile uint32_t*)(RCC_BASEADDR + 0x30) &= ~(1 << 4) )
#define GPIOF_PCLK_DIS()    ( *(volatile uint32_t*)(RCC_BASEADDR + 0x30) &= ~(1 << 5) )
#define GPIOG_PCLK_DIS()    ( *(volatile uint32_t*)(RCC_BASEADDR + 0x30) &= ~(1 << 6) )
#define GPIOH_PCLK_DIS()    ( *(volatile uint32_t*)(RCC_BASEADDR + 0x30) &= ~(1 << 7) )

/* ==========================================
 * 5. AHB1 Bus Reset Macros
 * RCC AHB1 Peripheral Reset Register (RCC_AHB1RSTR)
 * offset: 0x10
 * 0 -> does not reset IO port x
 * 1 -> resets IO port x
 * RCC_BASEADDR + offset
 * bits 0-7 -> GPIOA-H
 *
 * Core Logic:
 * 1. Set bit to 1 (Assert Reset) -> Reset button is pressed
 * 2. Set bit to 0 (Clear Reset) -> Release the reset button (the user has to let go at some point)
 * ==========================================
 */
#define RCC_AHB1_RESET_OFFSET 0x10
#define RCC_AHB1RSTR_ADDR  ((volatile uint32_t*)(RCC_BASEADDR + RCC_AHB1_RESET_OFFSET))
void GPIOA_REG_RESET(void);
void GPIOB_REG_RESET(void);
void GPIOC_REG_RESET(void);
void GPIOD_REG_RESET(void);
void GPIOE_REG_RESET(void);
void GPIOF_REG_RESET(void);
void GPIOG_REG_RESET(void);
void GPIOH_REG_RESET(void);

/*
 * ==========================================
 * 6. API Function Prototypes
 * ==========================================
 * These are the services offered by the driver.
 */

/*
 * Peripheral Clock Setup
 * Must be called FIRST before using any GPIO Port.
 * 'EnableOrDisable' should be ENABLE or DISABLE macros.
 */
void GPIO_PeriClockControl(GPIO_RegDef_t *pGPIOx, uint8_t EnableOrDisable);

/*
 * Initialization and De-Initialization
 * GPIO_Init takes the Handle structure to configure settings.
 * GPIO_DeInit resets the port to its default state.
 */
void GPIO_Init(GPIO_Handle_t *pGPIOHandle);
void GPIO_DeInit(GPIO_RegDef_t *pGPIOx);

/*
 * Data Read and Write
 * Reading from Input Pin: returns 0 or 1.
 * Reading from Input Port: returns the 16-bit value of the whole port.
 */
uint8_t GPIO_ReadFromInputPin(GPIO_RegDef_t *pGPIOx, uint8_t PinNumber);
uint16_t GPIO_ReadFromInputPort(GPIO_RegDef_t *pGPIOx);

/*
 * Writing to Output Pin: sets a specific pin to 0 or 1.
 * Writing to Output Port: writes to the entire port at once.
 * Toggle: flips the state of the pin (0->1 or 1->0).
 */
void GPIO_WriteToOutputPin(GPIO_RegDef_t *pGPIOx, uint8_t PinNumber, uint8_t Value);
void GPIO_WriteToOutputPort(GPIO_RegDef_t *pGPIOx, uint16_t Value);
void GPIO_ToggleOutputPin(GPIO_RegDef_t *pGPIOx, uint8_t PinNumber);

/*
 * assign EXTI line (PinNumber) to a specific GPIO port
 * e.g. PC13 -> assign line 13 to Port C
 */
void GPIO_SYSCFG_Config(GPIO_RegDef_t *pGPIOx, uint8_t PinNumber);

// Function Prototype
void NVIC_ISER_Config(uint8_t IRQNumber);

#endif /* SOURCES_STM32F446XX_GPIO_DRIVER_H_ */
