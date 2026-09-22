#include <stdbool.h>
#include "Button.h"
#include "stm32f4xx_hal.h"
#include "board.h"
#include "Feed.h"
#include "FreeRTOS.h"
#include "task.h"

/* Sample faster than the shortest press, so no press is missed.
 * A human's press lasts far longer than 20 ms. */
#define BUTTON_POLL_MS   20

static bool was_pressed;

void Button_Init(void)
{
	GPIO_InitTypeDef USER_BUTTON  = {
			.Pin   = USER_BUTTON_PIN,
			.Mode  = GPIO_MODE_INPUT,
			.Pull  = GPIO_PULLUP, /* active-low, HIGH when idle */
			.Speed = GPIO_SPEED_FREQ_LOW,
	};

	/* 1. Clock Enable */
	__HAL_RCC_GPIOC_CLK_ENABLE();

	/* 2. pin configuration */
	HAL_GPIO_Init(USER_BUTTON_PORT, &USER_BUTTON);

	/* 3. Read immediately in case button is already pressed during startup */
	was_pressed = !HAL_GPIO_ReadPin(USER_BUTTON_PORT, USER_BUTTON_PIN);
}

void Button_Task(void *arg)
{
	while (1)
	{
		bool pressed = !HAL_GPIO_ReadPin(USER_BUTTON_PORT, USER_BUTTON_PIN); /* PC13 is active low: pressed reads 0, so invert it */

		/* detects when button is released */
		if (was_pressed && !pressed){
			Feed_Request(FEED_BUTTON);
		}

		was_pressed = pressed;

		/* sleep, then sample again next period */
		vTaskDelay(pdMS_TO_TICKS(BUTTON_POLL_MS));
	}
}
