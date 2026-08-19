#include <stdint.h>
#include "stm32f4xx_hal.h"
#include "board.h"
#include "UART_CTRL.h"
#include "MOTOR_CTRL.h"
#include "Feed.h"
#include "CmdProc.h"
#include "TIMER.h"
#include "IWDG_CTRL.h"
#include "Comms.h"

void SysTick_Handler(void)
{
	HAL_IncTick();
}

void init_all(void)
{
	HAL_Init();

	UART_Init();
	MOTOR_Init();
	TIMER_Init();
	IWDG_Init();
}

int main(void)
{
	init_all();

	if (IWDG_WasResetByWatchdog())
	{
		Comms_SendResponse("Recovered from crash");
	}

	while (1)
	{
		IWDG_Refresh();
		Feed_Poll();
		CmdProc_Process();
	}
}
