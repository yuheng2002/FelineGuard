/*
 * stm32f446xx_timer_driver.c
 *
 *  Created on: 2026/1/2
 *      Author: Yuheng
 */
#include "stm32f446xx.h"
#include "stm32f446xx_timer_driver.h"
#include <stdint.h>
#include <stdio.h>

void TIM_PWM_Init(TIM_Handle_t *pTIMHandle){
	TIM_RegDef_t* pTIMx = pTIMHandle->pTIMx;
	TIM_Config_t TIM_Config = pTIMHandle->TIM_Config;

	// 1. Set PSC (Speed)
	pTIMx->PSC = TIM_Config.Prescaler; // TIM_Config is NOT a pointer (it is an object), use .

	// 2. Set ARR (Period/Duration)
	pTIMx->ARR = TIM_Config.Period;

	/*
	 * ==========================================
	 * 3. set CCMR1 to PWM mode 1
	 * ==========================================
	 * Bit 6:4 OC1M: Output compare 1 mode
	 * 110: PWM mode 1 - In upcounting, channel 1 is active as long as TIMx_CNT < TIMx_CCR1
	 * thus, we set bit 6 to 1, bit 5 to 1, bit 4 to 0
	 * Important: We should clear the bits before setting them to avoid corruption
     * if the register had garbage values or previous settings.
	 */

	// a) Clear bits 4, 5, 6 (OC1M field)
	// ~(7 << 4) means ~(111 << 4) -> ~(0000000001110000) -> 1111111110001111
	pTIMx->CCMR1 &= ~(7U << 4);

	// b) Set bits to 110 (PWM Mode 1)
	// 6 << 4 -> 110 << 4 -> 0000000001100000
	pTIMx->CCMR1 |= (6U << 4);

	/*
	 * 4. Enable Preload (OC1PE)
	 * Good practice for PWM. It ensures that if we change CCR1 while the timer is running,
	 * the new value only takes effect at the next update event (end of cycle),
	 * preventing "glitches" in the waveform.
	 */
	SET_BIT(pTIMx->CCMR1, 3);

	/*
	 * ==========================================
	 * 5. Enable CCER (capture/compare enable register)
	 * ==========================================
     * This connects the internal PWM signal to the physical Pin.
     * This bit determines if a capture of the counter value can actually be done into the input
     * capture/compare register 1 (TIMx_CCR1) or not.
	 */
	SET_BIT(pTIMx->CCER, 0);

	/*
	 * ==========================================
	 * 6. Enable Counter (in CR1 - Control Register 1)
	 * ==========================================
	 * won't work without Counter Enabled
	 * Bit 0 CEN: Counter enable
	 * -> 1: Counter enabled
	 * Note: External clock, gated mode and encoder mode can work only if the CEN bit has been
	 * previously set by software.
	 * However trigger mode can set the CEN bit automatically by hardware.
	 */
	SET_BIT(pTIMx->CR1, 0);
}

void TIM_SetCompare1(TIM_RegDef_t *pTIMx, uint32_t CaptureValue){
	// Writing to CCR1 changes the duty cycle (brightness)
	pTIMx->CCR1 = CaptureValue;
}

/*
 * NOTE:
 * I am currently still using the General TIM_RegDef_t struct defined in overall header filer
 * HOWEVER! I checked the TIM6&7 Register map
 *
 * They have far fewer registers available to use than General Purpose Timers (TIM2/3...)
 * I still use it because, for TIM 6:
 * DIER also has Offset: 0x0C
 * PSC also has Offset: 0x28
 * ARR also has Offset: 0x2C
 *
 * BUT I will most likely refractor later to make things more clear
 */
void TIM_Basic_Init(TIM_Handle_t *pTIMHandle){
	TIM_RegDef_t* pTIMx = pTIMHandle->pTIMx;
	TIM_Config_t TIM_Config = pTIMHandle->TIM_Config;

	// 1. Set PSC (Speed)
	pTIMx->PSC = TIM_Config.Prescaler;

	// 2. Set ARR (Period/Duration)
	pTIMx->ARR = TIM_Config.Period;

	// 3. Enable Update Interrupt (UIE) in DIER Register
	// Bit 0: UIE (Update Interrupt Enable)
	// 0: Update interrupt disabled.
	// 1: Update interrupt enabled.
	// When set, an interrupt is generated when the counter overflows/updates.
	SET_BIT(pTIMx->DIER, 0);

	// NOTE: We do NOT enable the Counter (CR1_CEN) here.
	// We want to start it manually in the main loop logic.
}

void TIM_IRQInterruptConfig(uint8_t IRQNumber, uint8_t EnableOrDisable){
	/*
	 * ==============================
	 * 	  1. Locate the register
	 * ==============================
	 * Calculate which ISER register (0-7) contains the target IRQ.
	 * ------------------------------
	 * Formula:
	 * Block = IRQ / 32
	 * ------------------------------
	 * e.g. IRQ = 38 (USART2 -> position 38)
	 * so, Block = 38 / 32 = 1
	 * it is indeed in NVIC_ISER[1]
	 */
	uint8_t register_num = IRQNumber / 32;

	/*
	 * ==============================
	 * 		2. Locate the Bit
	 * ==============================
	 * Each bit in each block controls 1 specific IRQ
	 * [one bit controls one IRQ]
	 * ------------------------------
	 * e.g. IRQ = 38
	 * so, it is at 38 % 32 = 6 -> Target is Bit 6
	 */
	uint8_t target_bit = IRQNumber % 32;

	/*
	 * ==============================
	 * 	  3. Enable the register
	 * ==============================
	 * Write:
	 * 0: No effect (Refer to PM0214 4.3.2)
	 * 1: Enable interrupt
	 * We simply write a 1 to the specific bit position
	 */
	SET_BIT(NVIC_ISER->ISER[register_num], target_bit);
}
