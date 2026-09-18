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
	MOTOR_Init();
	TIMER_Init();
	Button_Init();
	if (!RTC_Init())
	{
		Comms_SendResponse("RTC clock failed to initialize");
	}

	// IWDG_Init();
}

static void blink_task(void *arg)
{
	while (1)
	{
		HAL_GPIO_TogglePin(LD2_PORT, LD2_PIN);
		vTaskDelay(pdMS_TO_TICKS(500)); /* moves this task to the Blocked state and gives up the CPU; unlike HAL_Delay,
         	 	 	 	 	 	 	 	 * which busy-waits, other tasks run during this time */
	}
}

int main(void)
{
	init_all();

	if (IWDG_WasResetByWatchdog())
	{
		Comms_SendResponse("Recovered from crash");
	}

    /* LD2 currently has no owning module; configure it here for the test */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef led = {
        .Pin   = LD2_PIN,
        .Mode  = GPIO_MODE_OUTPUT_PP,
        .Pull  = GPIO_NOPULL,
        .Speed = GPIO_SPEED_FREQ_LOW
    };
    HAL_GPIO_Init(LD2_PORT, &led);
    /*
     * 1. blink_task -> task function, must be void f(void *) and must never return
     * 2. "blink" -> name, used for debugging: this is what arrives as pcTaskName in vApplicationStackOverflowHook
     * 3. 128 -> stack size in words, so 128 x 4 = 512 bytes. This is also what configMINIMAL_STACK_SIZE is set to
     * 4. NULL -> argument passed to blink_task, nothing needed here
     * 5. 1 -> priority. Unlike NVIC priorities, a larger number is higher priority in FreeRTOS. 0 is where the idle task runs
     * 6. NULL -> where to store the task handle. Only needed to suspend, resume or delete the task later, which this test does not do
     *
     * This only registers the task. Nothing runs until the scheduler starts.
     */
    xTaskCreate(blink_task, "blink", 128, NULL, 1, NULL);

    /* 1. creates the idle task (priority 0)
     * 2. creates the timer task, because configUSE_TIMERS is 1
     * 3. configures SysTick to generate ticks at configTICK_RATE_HZ
     * 4. switches to the highest priority ready task
     *
     * Does not return. Everything after this line is unreachable unless the scheduler failed to start. */
    vTaskStartScheduler();

    while (1) { }   /* unreachable unless the scheduler fails to start */
}
