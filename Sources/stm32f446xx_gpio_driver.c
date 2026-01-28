/*
 * stm32f446xx_gpio_driver.c
 *
 *  Created on: 2025/12/23
 *      Author: Yuheng
 */

#include "stm32f446xx_gpio_driver.h"
#include <stdint.h>
#include <stdio.h>

/*
 * Peripheral Clock Setup
 * Must be called FIRST before using any GPIO Port.
 * 'EnableOrDisable' should be ENABLE or DISABLE macros.
 */
void GPIO_PeriClockControl(GPIO_RegDef_t *pGPIOx, uint8_t EnableOrDisable){
	if (EnableOrDisable == ENABLE){
		if (pGPIOx == GPIOA){
			GPIOA_PCLK_EN();
		}
		else if (pGPIOx == GPIOB){
			GPIOB_PCLK_EN();
		}
		else if (pGPIOx == GPIOC){
			GPIOC_PCLK_EN();
		}
		else if (pGPIOx == GPIOD){
			GPIOD_PCLK_EN();
		}
		else if (pGPIOx == GPIOE){
			GPIOE_PCLK_EN();
		}
		else if (pGPIOx == GPIOF){
			GPIOF_PCLK_EN();
		}
		else if (pGPIOx == GPIOG){
			GPIOG_PCLK_EN();
		}
		else if (pGPIOx == GPIOH){
			GPIOH_PCLK_EN();
		}
	}
	else if (EnableOrDisable == DISABLE){
		if (pGPIOx == GPIOA){
			GPIOA_PCLK_DIS();
		}
		else if (pGPIOx == GPIOB){
			GPIOB_PCLK_DIS();
		}
		else if (pGPIOx == GPIOC){
			GPIOC_PCLK_DIS();
		}
		else if (pGPIOx == GPIOD){
			GPIOD_PCLK_DIS();
		}
		else if (pGPIOx == GPIOE){
			GPIOE_PCLK_DIS();
		}
		else if (pGPIOx == GPIOF){
			GPIOF_PCLK_DIS();
		}
		else if (pGPIOx == GPIOG){
			GPIOG_PCLK_DIS();
		}
		else if (pGPIOx == GPIOH){
			GPIOH_PCLK_DIS();
		}
	}
	// if it gets here, argument EnableOrDisable passed in is invalid input
	else{
		return;
	}
}

/*
 * Helper function: Calculates Port Code (0 for A, 1 for B, 2 for C...)
 * Logic: (Current_Addr - Base_Addr) / Block_Size
 * Inspiration comes from Leetcode 242. Valid Anagram
 * ==========================================
 * Note: static in C functions to encapsulate
 * it means "Private", which is the opposite of what it means in Java (global)
 * since Get_Port_Code is just a calculation tool
 * it does not need to be exposed to the user (main.c)
 */
static uint8_t Get_Port_Code(GPIO_RegDef_t *pGPIOx){
	return ((uint32_t)pGPIOx - (uint32_t)GPIOA_BASEADDR) / 0x400;
}

void GPIO_SYSCFG_Config(GPIO_RegDef_t* pGPIOx, uint8_t PinNumber){
	// Enable SYSCFG
	SYSCFG_PCLK_EN();

	// calculate where the target Pin fits in EXTICR
	// e.g. Pin 13 -> 13 / 4 = 3
	// Pin 13 is at bits {4,5,6,7} in EXTICR4
	// EXTICR is 0-based index, but these registers are named starting at 1
	uint8_t register_num = PinNumber / 4;

	// calculate the bits shift amount
	// (PinNumber % 4) * 4
	// e.g. Pin 13 -> (13 % 4) * 4 = 1 * 4 = 4
	uint8_t shift_amount = (PinNumber % 4) * 4;

	// Clearing mask
	// 0xF -> 15U -> 0b1111 (4 bits)
	uint32_t mask = ~(0xF << shift_amount);

	// reset target bits using mask
	SYSCFG->EXTICR[register_num] &= mask;

	// now figure out which port is it to set the proper value
	// 0000: PA[x] pin
	// 0001: PB[x] pin
	// ...
	// 0110: PG[x] pin
	// using GPIOA as the base
	// the difference between pGPIOx and GPIOA, divided by 0x0400U, gives the Port number
	// e.g. GPIOA - GPIOA = 0 -> 0000
	// e.g. (GPIOB - GPIOA) / 0x400U = 0x40020400 - 0x40020000 = 1 -> 0001
	uint8_t portCode = Get_Port_Code(pGPIOx);

	// set the correct value
	SYSCFG->EXTICR[register_num] |= (portCode << shift_amount);
}

void NVIC_ISER_Config(uint8_t IRQNumber){
	/*
	 * 1. Locate the Register
	 * Calculate which ISER register (0-7) contains the target IRQ.
	 * Since each register holds 32 bits (32 IRQs):
	 * Index = IRQ / 32
	 * e.g. IRQ = 40 (Pin 13) -> 40 / 32 = 1
	 * so, IRQ 40 is in NVIC_ISER[1]
	*/
	uint8_t register_num = IRQNumber / 32;

	/*
	 * 2. Locate the Bit
	 * Calculate the bit position within that 32-bit register.
	 * [one bit controls one IRQ]
	 * (IRQNumber % 32) * 1
	 * e.g. IRQ 40 -> (40 % 32) * 1 = 8 * 1 = 8 -> Target is Bit 8
	 */
	uint8_t shift_amount = (IRQNumber % 32) * 1;

	/*
	 * 3. Enable the Interrupt
	 * NOTE: NVIC_ISER is a "Write 1 to Enable" register.
	 * Writing 0 has NO EFFECT. (Refer to PM0214 4.3.2)
	 * Therefore, we do NOT need to clear the bit with '&=' first.
	 * We simply write a 1 to the specific bit position.
	 *
	 * Why not just '=' ?
	 * Using |= is technically a Read-Modify-Write, but since writing 0 to other bits
	 * (which happens during the Write back) has no effect, existing enabled interrupts stay enabled.
	 * It is safe.
	 */
	NVIC_ISER->ISER[register_num] |= (1U << shift_amount);
}

/*
 * Initialization
 * GPIO_Init takes the Handle structure to configure settings.
 * Note: IDR and ODR are taken care of in Read/Write functions
 * IDR and ODR are not part of initialization
 */
void GPIO_Init(GPIO_Handle_t *pGPIOHandle){
	GPIO_RegDef_t *pGPIOx = pGPIOHandle->pGPIOx;
	GPIO_PinConfig_t GPIO_PinConfig = pGPIOHandle->GPIO_PinConfig;
	uint8_t GPIO_PinNumber = GPIO_PinConfig.GPIO_PinNumber; // GPIO_PinConfig is NOT a pointer, use .
	uint8_t GPIO_PinMode = GPIO_PinConfig.GPIO_PinMode; // same
	uint8_t GPIO_PinSpeed = GPIO_PinConfig.GPIO_PinSpeed; // ...
	uint8_t GPIO_PinPuPdControl = GPIO_PinConfig.GPIO_PinPuPdControl; // ...
	uint8_t GPIO_PinOPType = GPIO_PinConfig.GPIO_PinOPType; // ...
	uint8_t GPIO_PinAltFunMode = GPIO_PinConfig.GPIO_PinAltFunMode; // ...

	/*
	 * =================================
	 * set MODER
	 * MODER offset: 0x00 (already taken care of in struct GPIO_RegDef_t type definition)
	 * =================================
	 */
	if (GPIO_PinMode <= GPIO_MODE_ANALOG){ // security check, ONLY 4 valid modes
		uint8_t shift_amount = 2 * GPIO_PinNumber; // Bits 2y:2y+1 controls MODER of Pin y
	    uint32_t bits_set_zero = ~(3U << shift_amount);
		pGPIOx->MODER &= bits_set_zero; // sets the two target bits to 0 first
		pGPIOx->MODER |= (GPIO_PinMode << shift_amount); // sets the desired value
	}else{
		/*
		 * ==========================================
		 * set MODE to interrupt mode
		 * ==========================================
		 * Falling edge -> 4
		 * Rising edge -> 5
		 * Rising & Falling edge -> 6
		 * For my purpose, I need PC13 to receive interrupt signals
		 * First -> Enable SYSCFG clock, and assign line 13 specifically to Port C
		 * Second -> Enable interrupt request from line 13 (similar logic to enabling clock)
		 * Last -> set trigger selection mode
		 */

		/*
		* Interrupt Mode Configuration
		*/

		// 1. SYSCFG Configuration
		GPIO_SYSCFG_Config(pGPIOx, GPIO_PinNumber);

		// 2. EXTI Interrupt Mask Configuration
		// Unmask the interrupt line to let the CPU "hear" the signal
		int mask = 1U << GPIO_PinNumber;
		EXTI->IMR |= mask; // 1: interrupt request from line x is not masked

		// 3. Trigger Selection (FTSR / RTSR)
		// Safety Step: Clear the bits first to avoid having both set accidentally
		// if the user switches modes dynamically.
		EXTI->FTSR &= ~(1U << GPIO_PinNumber);
		EXTI->RTSR &= ~(1U << GPIO_PinNumber);

		// if MODE is 4, Enable falling trigger selection register for input line
		// if MODE is 5, Enable rising trigger selection register for input line
		// if MODE is 6, Enable both FTSR and RTSR for input line
		// Use Macros (GPIO_MODE_IT_FT) instead of magic numbers 4,5,6
		if (GPIO_PinMode == GPIO_MODE_IT_FT){
			EXTI->FTSR |= (1U << GPIO_PinNumber);
		}
		else if (GPIO_PinMode == GPIO_MODE_IT_RT){
			EXTI->RTSR |= (1U << GPIO_PinNumber);
		}
		else if (GPIO_PinMode == GPIO_MODE_IT_RFT){
			EXTI->RTSR |= (1U << GPIO_PinNumber);
			EXTI->FTSR |= (1U << GPIO_PinNumber);
		}

		/*
		 * 4. NVIC Configuration (CPU Level)
		 * Enable the IRQ line in the Nested Vectored Interrupt Controller.
		 * Without this, the signal reaches NVIC but is blocked from reaching the CPU core.
		 *
		 * TODO: In a generic driver, we should map PinNumber to IRQNumber dynamically.
		 * For Project 2, we specifically need EXTI15_10_IRQ (IRQ 40) for PC13.
		 */
		if (GPIO_PinNumber >= 10 && GPIO_PinNumber <= 15){
			NVIC_ISER_Config(EXTI15_10_IRQ);
		}
	}

	/*
	 * =================================
	 * set OSPEEDR
	 * OSPEEDR offset: 0x08
	 * bits 2y:2y+1 controls Pin y
	 * 00 -> Low speed
	 * 01: Medium speed
	 * 10: Fast speed
	 * 11: High speed
	 * =================================
	 */
	if (GPIO_PinSpeed <= GPIO_SPEED_VERY_HIGH){
		uint8_t shift_amount = 2 * GPIO_PinNumber;
		uint32_t bits_set_zero = ~(3U << shift_amount);
		pGPIOx->OSPEEDR &= bits_set_zero;
		pGPIOx->OSPEEDR |= (GPIO_PinSpeed << shift_amount);
	}

	/*
	 * =================================
	 * set PUPDR
	 * PUPDR offset: 0x0C
	 * bits 2y:2y+1 controls Pin y
	 * 00 -> No pull-up, pull-down
	 * 01 -> Pull-up
	 * 10 -> Pull-down
	 * 11 -> Reserved
	 * =================================
	 */
	if (GPIO_PinPuPdControl <= GPIO_PIN_PD){
		uint8_t shift_amount = 2 * GPIO_PinNumber;
		uint32_t bits_set_zero = ~(3U << shift_amount);
		pGPIOx->PUPDR &= bits_set_zero;
		pGPIOx->PUPDR |= (GPIO_PinPuPdControl << shift_amount);
	}


	/*
	 * =================================
	 * set OTYPER (output type register)
	 * TYPER offset: 0x04
	 * bits 0-15 controls Pin 0-15
	 * 1 -> open-drain
	 * 0 -> push-pull (reset state)
	 * =================================
	 */
	if (GPIO_PinOPType <= GPIO_OP_TYPE_OD){ // security check, ONLY 2 valid modes (PP or OD)
		pGPIOx->OTYPER &= ~(1U << GPIO_PinNumber); // set the target bit to 0
		pGPIOx->OTYPER |= (GPIO_PinOPType << GPIO_PinNumber);
	}

	/*
	 * =================================
	 * set Alternate function registers
	 * AFRL (alternate function low register) offset: 0x20
	 * AFRH (alternate function high register) offset: 0x24
	 * Only required if Mode is set to Alt Function (Mode = 2)
	 * 4 bits control a Pin
	 * 0000 -> AF0
	 * ...
	 * 1111 -> AF15
	 * =================================
	 */
	if (GPIO_PinMode == GPIO_MODE_ALTN){
		// calculate whether the target Pin fits in low or high register
		// e.g. Pin 5 -> 5/8 = 0 (AFRL)
		// e.g. Pin 10-> 10/8 = 1 (AFRH)
		uint8_t L_OR_H = GPIO_PinNumber / 8;

		// calculate the bits shift amount
		// (PinNumber % 8) * 4
		// e.g. Pin 5 -> (5 % 8) * 4 = 5 * 4 = 20
		// e.g. Pin 10 -> (10 % 8) * 4 = 2 * 4 = 8
		uint8_t shift_amount = (GPIO_PinNumber % 8) * 4;

		// Clearing
		// 0xF -> 15U -> 0b1111 (4 bits)
		uint32_t bits_set_zero = ~(0xF << shift_amount);

		pGPIOx->AFR[L_OR_H] &= bits_set_zero;
		pGPIOx->AFR[L_OR_H] |= (GPIO_PinAltFunMode << shift_amount);
	}
}

/*
 * AHB1 Bus Reset Macros Implementation
 */
void GPIOA_REG_RESET(void){
	*RCC_AHB1RSTR_ADDR |= (1U << 0);
	*RCC_AHB1RSTR_ADDR &= ~(1U << 0);
}
void GPIOB_REG_RESET(void){
	*RCC_AHB1RSTR_ADDR |= (1U << 1);
	*RCC_AHB1RSTR_ADDR &= ~(1U << 1);
}
void GPIOC_REG_RESET(void){
	*RCC_AHB1RSTR_ADDR |= (1U << 2);
	*RCC_AHB1RSTR_ADDR &= ~(1U << 2);
}
void GPIOD_REG_RESET(void){
	*RCC_AHB1RSTR_ADDR |= (1U << 3);
	*RCC_AHB1RSTR_ADDR &= ~(1U << 3);
}
void GPIOE_REG_RESET(void){
	*RCC_AHB1RSTR_ADDR |= (1U << 4);
	*RCC_AHB1RSTR_ADDR &= ~(1U << 4);
}
void GPIOF_REG_RESET(void){
	*RCC_AHB1RSTR_ADDR |= (1U << 5);
	*RCC_AHB1RSTR_ADDR &= ~(1U << 5);
}
void GPIOG_REG_RESET(void){
	*RCC_AHB1RSTR_ADDR |= (1U << 6);
	*RCC_AHB1RSTR_ADDR &= ~(1U << 6);
}
void GPIOH_REG_RESET(void){
	*RCC_AHB1RSTR_ADDR |= (1U << 7);
	*RCC_AHB1RSTR_ADDR &= ~(1U << 7);
}

/*
 * De-Initialization
 * GPIO_DeInit resets the port to its default state.
 */
void GPIO_DeInit(GPIO_RegDef_t *pGPIOx){
	if (pGPIOx == GPIOA){
		GPIOA_REG_RESET();
	}
	else if (pGPIOx == GPIOB){
		GPIOB_REG_RESET();
	}
	else if (pGPIOx == GPIOC){
		GPIOC_REG_RESET();
	}
	else if (pGPIOx == GPIOD){
		GPIOD_REG_RESET();
	}
	else if (pGPIOx == GPIOE){
		GPIOE_REG_RESET();
	}
	else if (pGPIOx == GPIOF){
		GPIOF_REG_RESET();
	}
	else if (pGPIOx == GPIOG){
		GPIOG_REG_RESET();
	}
	else if (pGPIOx == GPIOH){
		GPIOH_REG_RESET();
	}
}

/*
 * Data Read
 * Reading from Input Pin: return 0 or 1
 * IDR offset: 0x10
 */
uint8_t GPIO_ReadFromInputPin(GPIO_RegDef_t *pGPIOx, uint8_t PinNumber){
	uint8_t read = (uint8_t)(pGPIOx->IDR >> PinNumber) & 1; // if the value is 1, it will be 1
															   // if the value is 0, it will be 0
	return read;
}

/*
 * Data Read
 * Reading from Input Port: returns the 16-bit value of the whole port
 * bits 0-15 are for pins 0-15 (since only need 1 bit for readings 0 or 1)
 * bits 16-31 are reserved
 */
uint16_t GPIO_ReadFromInputPort(GPIO_RegDef_t *pGPIOx){
	uint16_t read = (uint16_t)pGPIOx->IDR;
	return read;
}

/*
 * Writing to Output Pin: sets a specific pin to 0 or 1
 * * [UPDATED Implementation using BSRR]
 * Instead of Read-Modify-Write using ODR (which is not interrupt-safe),
 * we use the BSRR (Bit Set/Reset Register) for atomic operations.
 * * BSRR Logic:
 * - Writing '1' to bits 0-15 sets the corresponding pin (BSy).
 * - Writing '1' to bits 16-31 resets the corresponding pin (BRy).
 */
void GPIO_WriteToOutputPin(GPIO_RegDef_t *pGPIOx, uint8_t PinNumber, uint8_t Value){
    if (Value == GPIO_PIN_SET)
    {
        // Write to the lower 16 bits (BSy) to SET the pin
        pGPIOx->BSRR = (1U << PinNumber);
    }
    else
    {
        // Write to the upper 16 bits (BRy) to RESET the pin
        // Note: The Reset bits start at bit position 16.
        pGPIOx->BSRR = (1U << (PinNumber + 16));
    }
}

/*
 * Writing to Output Port: writes to the entire port at once
 * RM390 states that "Reserved bits Must be kept at reset value (0)"
 * which means that it is fine to set them to 0
 * Alternatively, we can just write pGPIO->ODR = Value
 * since Value is a 16 bit unsigned integer, its 16-31 bits will be 0
 * However, I am keeping a good habit of taking safety precautions to manually keep those at original state
 */
void GPIO_WriteToOutputPort(GPIO_RegDef_t *pGPIOx, uint16_t Value){
	uint32_t mask = ~(0xFFFF); // this way avoid changing the reserved bits (16-31)
	pGPIOx->ODR &= mask;
	pGPIOx->ODR |= Value;

	// pGPIO->ODR = Value
}

/*
 * Toggle: flips the state of the pin (0->1 or 1->0)
 */
void GPIO_ToggleOutputPin(GPIO_RegDef_t *pGPIOx, uint8_t PinNumber){
	pGPIOx->ODR ^= (1 << PinNumber); // use XOR to flip the value
}

/*
 * Saved for later
 */
void GPIO_IRQConfig(uint8_t IRQNumber, uint8_t EnableOrDisable){

}
