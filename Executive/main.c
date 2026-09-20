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
#include "Schedule.h"
#include "FreeRTOS.h"
#include "task.h"

extern void xPortSysTickHandler(void);

void SysTick_Handler(void)
{
    xPortSysTickHandler();
}

/* Callback function freeRTOS uses to deal with stack overflow */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    /* A task overflowed its stack. Break here and read pcTaskName to see which. */
    taskDISABLE_INTERRUPTS();
    while (1) { }
}

void init_all(void)
{
	HAL_Init();

	UART_Init();
	Comms_Init();
	MOTOR_Init();
	TIMER_Init();
	Button_Init();
	if (!RTC_Init())
	{
		Comms_SendResponse("RTC clock failed to initialize");
	}

	// IWDG_Init();
}

int main(void)
{
	init_all();

	if (IWDG_WasResetByWatchdog())
	{
		Comms_SendResponse("Recovered from crash");
	}

	/* Comms runs higher: if the receive queue fills, bytes are dropped. A line waiting to be parsed just waits. */
    if (xTaskCreate(Comms_Task, "comms", 128, NULL, 2, NULL) != pdPASS)
    {
        while (1) { }
    }

    if (xTaskCreate(CmdProc_Task, "cmdproc", 128, NULL, 1, NULL) != pdPASS)
    {
        while (1) { }
    }

    /* 1. creates the idle task (priority 0)
     * 2. creates the timer task, because configUSE_TIMERS is 1
     * 3. configures SysTick to generate ticks at configTICK_RATE_HZ
     * 4. switches to the highest priority ready task
     *
     * Does not return. Everything after this line is unreachable unless the scheduler failed to start. */
    vTaskStartScheduler();

    while (1) { }   /* unreachable unless the scheduler fails to start */
}
