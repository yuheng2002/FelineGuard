#include <stdint.h>
#include "stm32f4xx_hal.h"
#include "board.h"
#include "UART_CTRL.h"
#include "MOTOR_CTRL.h"

void SysTick_Handler(void)
{
	HAL_IncTick();
}

void blocking_delay(void){
	volatile uint32_t i;
	for (i = 0; i < 500000; i++){ }
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

	uint8_t b;

	while (1){
		/* Stage 2: verify the UART_CTRL driver */
		if (UART_CTRL_ReadByte(&b)){
			if (b == '1'){
				UART_CTRL_Write(&b, 1);
				HAL_GPIO_WritePin(LD2_PORT, LD2_PIN, GPIO_PIN_SET);
				blocking_delay();
				HAL_GPIO_WritePin(LD2_PORT, LD2_PIN, GPIO_PIN_RESET);
				blocking_delay();
				MOTOR_Start();
			}

			else if (b == '2'){
				MOTOR_Stop();
				UART_CTRL_Write(&b, 1);
			}
		}
	}
}
