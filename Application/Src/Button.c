#include <stdbool.h>
#include "Button.h"
#include "stm32f4xx_hal.h"
#include "board.h"
#include "Feed.h"

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

void Button_Poll(void)
{
	bool pressed = !HAL_GPIO_ReadPin(USER_BUTTON_PORT, USER_BUTTON_PIN); /* PC13 is active low: pressed reads 0, so invert it */

	if (was_pressed && !pressed){
		Feed_Request(FEED_BUTTON);
	}

	was_pressed = pressed;
}
