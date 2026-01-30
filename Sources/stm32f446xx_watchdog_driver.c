/*
 * stm32f446xx_watchdog_driver.c
 *
 *  Created on: 2026/1/29
 *      Author: Yuheng
 */
#include "stm32f446xx.h"
#include "stm32f446xx_watchdog_driver.h"
#include <stdint.h>

void IWDG_Init(IWDG_Config_t *IWDG_Config){
	/*
	 * we use predefined IWDG pointer directly, as it is a unique peripheral
	*/
	uint32_t Prescaler = IWDG_Config->IWDG_Prescaler;
	uint32_t ReloadValue = IWDG_Config->IWDG_Counter;

	// 1. Enable Write Access to IWDG_PR and IWDG_RLR
	// MUST write 0x5555 to KR before you can touch PR or RLR
	IWDG->KR = IWDG_KEY_ACCESS;

	// 2. Set Prescaler
	IWDG->PR = Prescaler;

	// 3. Set Reload Value
	// only Bits 11:0 are valid
	uint32_t mask = (1u << 12) - 1;
	IWDG->RLR = ( ReloadValue & mask ); // just in case user passes a huge number

	// 4. Reload the counter immediately with the new value (Pre-load)
	IWDG->KR = IWDG_KEY_FEED; // Feed the dog with 0xAAAAU per instruction

	// 5. Start the Watchdog
	// Writing the key value CCCCh starts the watchdog
	// (except if the hardware watchdog option is selected)
	IWDG->KR = IWDG_KEY_ENABLE;
}

/*
 * [These bits must be written by software at regular intervals with the key value AAAAh,
 * otherwise the watchdog generates a reset when the counter reaches 0.]
 */
void IWDG_FEED(void){
	// Write 0xAAAA to KR to reload the counter with the value in RLR
	IWDG->KR = IWDG_KEY_FEED;
}
