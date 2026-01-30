/*
 * stm32f446xx_watchdog_driver.h
 *
 *  Created on: 2026/1/29
 *      Author: Yuheng
 */

#ifndef SOURCES_STM32F446XX_WATCHDOG_DRIVER_H_
#define SOURCES_STM32F446XX_WATCHDOG_DRIVER_H_

#include "stm32f446xx.h"
#include <stdint.h>
/*
 * ==========================================
 * IWDG Functional Overview (3.21.4 datasheet)
 * ==========================================
 *
 * The independent watchdog is based on a 12-bit downcounter and 8-bit prescaler.
 * It is clocked from an independent 32 kHz internal RC and as it operates independently
 * from the main clock, it can operate in Stop and Standby modes.
 * It can be used either as a watchdog to reset the device when a problem occurs,
 * or as a free-running timer for application timeout management.
 * It is hardware- or software-configurable through the option bytes.
 */

/*
 * ==========================================
 * Functional Description (20.3 in RM0390)
 * ==========================================
 * When the independent watchdog is started by writing the value 0xCCCC in
 * the Key register (IWDG_KR), the counter starts counting down from the reset value of 0xFFF.
 * When it reaches the end of count value (0x000) a reset signal is generated (IWDG reset).
 * Whenever the key value 0xAAAA is written in the IWDG_KR register, the IWDG_RLR value
 * is reloaded in the counter and the watchdog reset is prevented.
 */

/*
 * ==========================================
 * 1. Clock Enable Macro
 * ==========================================
 *
 * NOTE:
 * I read from the [2.2.2 Memory map] that:
 * IWDG is on the APB1 bus with an offset -> 0x3000
 *
 * However, in [6.3.13 RCC APB1 peripheral clock enable register]
 * I was NOT able to find the bit to enable IWDG
 *
 * Then, I read [20.1 IWDG introduction], and it says:
 * "The independent watchdog (IWDG) is clocked by its own dedicated low-speed clock (LSI)
 * and thus stays active even if the main clock fails."
 *
 * This make sense, because the point of using IWDG is to make sure that,
 * as long as the system is still powered, the watchdog runs **Independently**,
 * so it runs on its own.
 * If it needs to be enabled by the RCC registers,
 * it will fail when RCC goes down.
 *
 * This explains why I was not able to locate IWDG in the RCC APB1 Clock Enable Register
 */

/*
 * ==========================================
 * 2. Configuration Structures
 * ==========================================
 */

/*
 * For most other peripherals I have used previously (GPIO, TIM, USART...)
 * I always defined a member variable xxx__RegDef_t to allow flexibility
 * because I would not know which specific GPIO port or TIM or USART peripheral I might use
 *
 * However, for IWDG, which is unique (the one and only) in this STM32 MCU
 * It seems a little unnecessary to do the same
 * { I could do it just to satisfy my OCD, but it seems meaningless }
 */
typedef struct{
	/* * Key is NOT needed here. The driver handles the unlocking internally.
	 * Status is read-only, so not needed here. (purely controller by hardware)
	 */
	uint32_t IWDG_Prescaler;  // Use IWDG_PRESCALER_x macros
	uint32_t IWDG_Counter;     // 12-bit value (0x000 to 0xFFF)
} IWDG_Config_t;


/*
 * ==========================================
 * 3. Configuration Macros (USART Specific)
 * ==========================================
 * Macros to prevent user errors (magic numbers).
 */
/* @IWDG Magic Keys (Register KR values)*/
#define IWDG_KEY_ACCESS   0x5555U // Unlocks write access to PR and RLR
#define IWDG_KEY_ENABLE   0xCCCCU // Starts the watchdog
#define IWDG_KEY_FEED   0xAAAAU // Refreshes the counter (Feed the dog)

/* * @IWDG Prescaler Values
 * NOTE: Values correspond to the 3 bits in IWDG_PR register
*/
#define IWDG_PRESCALER_4         (0u)
#define IWDG_PRESCALER_8         (1u)
#define IWDG_PRESCALER_16        (2u)
#define IWDG_PRESCALER_32        (3u)
#define IWDG_PRESCALER_64        (4u)
#define IWDG_PRESCALER_128       (5u)
#define IWDG_PRESCALER_256       (6u)

/*
 * ==========================================
 * 		4. Function Prototypes
 * ==========================================
 */
/* Initialize and Start the Watchdog */
void IWDG_Init(IWDG_Config_t *pIWDG_Config);

/* Feed the dog (Call this in main loop) */
void IWDG_FEED(void);

#endif /* SOURCES_STM32F446XX_WATCHDOG_DRIVER_H_ */
