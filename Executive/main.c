#include <stdint.h>
#include "stm32f4xx_hal.h"
#include "board.h"
#include "UART_CTRL.h"
#include "MOTOR_CTRL.h"
#include "Feed.h"
#include "CmdProc.h"
#include "TIMER.h"

void SysTick_Handler(void)
{
	HAL_IncTick();
}

int main(void)
{
	HAL_Init();

	/* Stage 1: verify the HAL GPIO path by turning on/off the on-board User LD2 */
	__HAL_RCC_GPIOA_CLK_ENABLE();

	GPIO_InitTypeDef LD2_Config = {
			.Pin = LD2_PIN,
			.Mode = GPIO_MODE_OUTPUT_PP,
			.Pull = GPIO_NOPULL,
			.Speed = GPIO_SPEED_FREQ_LOW
			// no AF
	};

	HAL_GPIO_Init(LD2_PORT, &LD2_Config);

	UART_CTRL_Init();

	MOTOR_Init();

	TIMER_Init();

	while (1){
		Feed_Poll();
		CmdProc_process();
	}
}
