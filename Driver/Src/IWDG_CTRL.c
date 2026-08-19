#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"
#include "IWDG_CTRL.h"

#define IWDG_RELOAD_VALUE     999

/*
 * [RM0390 p.631]
 *
 * Prescaler divider = 32 gives
 * Min timeout = 1 ms
 * Max timeout = 4096 ms
 *
 * Reload register (IWDG_RLR) is 12 bits wide [11:0], so its scale is 0...4096
 *
 * 		counter clock = 32 kHz / 32 = 1 kHz
 * 		1 count = 1 ms
 *
 * set RLR to 999, so it refreshes every 999 + 1 = 1000 ms (0-based)
 */
static IWDG_HandleTypeDef IWDG_Handle = {
		.Instance = IWDG,
		.Init     = {
				.Prescaler = IWDG_PRESCALER_32,
				.Reload    = IWDG_RELOAD_VALUE
		}
};

void IWDG_Init(void)
{
	HAL_IWDG_Init(&IWDG_Handle);
}

void IWDG_Refresh(void){
	HAL_IWDG_Refresh(&IWDG_Handle);
}

/* read RCC_CSR and clear flags
 * @return true if last reset was caused by IWDG
 * @return false if otherwise */
bool IWDG_WasResetByWatchdog(void)
{
	bool reset_by_iwdg = __HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST);
	__HAL_RCC_CLEAR_RESET_FLAGS(); /* clears RMVF */

	return reset_by_iwdg;
}
