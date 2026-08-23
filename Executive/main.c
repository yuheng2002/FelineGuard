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
#include "Button.h"
#include "RTC_CTRL.h"

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
	Button_Init();
	if (!RTC_Init())
	{
		Comms_SendResponse("RTC clock failed to initialize");
	}

	if (!RTC_IsTimeSet())
	{
		Comms_SendResponse("Time not set");
	}

	RTC_SetTime(14, 30);

	RTC_TimeTypeDef t;
	RTC_DateTypeDef d;
	HAL_RTC_GetTime(&RTC_Handle, &t, RTC_FORMAT_BIN);
	HAL_RTC_GetDate(&RTC_Handle, &d, RTC_FORMAT_BIN);

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
		Button_Poll();
	}
}
