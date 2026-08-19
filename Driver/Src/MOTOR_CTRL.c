#include <stdint.h>
#include "stm32f4xx_hal.h"
#include "MOTOR_CTRL.h"
#include "board.h"

#define TIM2_PRESCALER    63
#define TIM2_ARR          999
#define TIM2_PWM_PULSE    ( (TIM2_ARR + 1) / 2 ) /* CCR: duty cycle = 50% */

/* PWM = 250Hz
 * Math:
 *
 * 		freq = 16MHz / ( (PSC + 1) x (ARR + 1) )
 *
 * 16,000,000 / ( (63 + 1) x (999 + 1) ) = 250 */
static TIM_HandleTypeDef TIM2_Handle = {
		.Instance = TIM2,
		.Init     = {
				.Prescaler         = TIM2_PRESCALER,
				.CounterMode       = TIM_COUNTERMODE_UP,
				.Period            = TIM2_ARR,
				.ClockDivision     = TIM_CLOCKDIVISION_DIV1 ,
				/* NO RepetitionCounter */
				.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE
		}
};

static const TIM_OC_InitTypeDef TIM2_OC_Config = {
		.OCMode     = TIM_OCMODE_PWM1,
		.Pulse      = TIM2_PWM_PULSE,
		.OCPolarity = TIM_OCPOLARITY_HIGH, 				 /* HIGH: CNT < CCR */
		.OCFastMode = TIM_OCFAST_DISABLE,
		/* OCNPolarity / OCIdleState / OCNIdleState are for advanced timers and thus not configured here */
};

void MOTOR_Init(void){
	GPIO_InitTypeDef MOTOR_DIR  = {
			.Pin   = MOTOR_DIR_PIN,
			.Mode  = GPIO_MODE_OUTPUT_PP,
			.Pull  = GPIO_PULLDOWN,
			.Speed = GPIO_SPEED_FREQ_LOW,
			/* No AF used for DIR */
	};

	GPIO_InitTypeDef MOTOR_STEP = {
			.Pin       = MOTOR_STEP_PIN,
			.Mode      = GPIO_MODE_AF_PP,
			.Pull      = GPIO_PULLDOWN,
			.Speed     = GPIO_SPEED_FREQ_LOW,
			.Alternate = GPIO_AF1_TIM2     /* datasheet Table 11 (p.58) */
	};

	GPIO_InitTypeDef MOTOR_EN  = {
			.Pin   = MOTOR_EN_PIN,
			.Mode  = GPIO_MODE_OUTPUT_PP,
			.Pull  = GPIO_NOPULL,
			.Speed = GPIO_SPEED_FREQ_LOW,
	};


	/* 1. Clock Enable */
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_TIM2_CLK_ENABLE();

	/* 2. pin configuration */
	HAL_GPIO_Init(MOTOR_STEP_PORT, &MOTOR_STEP);

	HAL_GPIO_Init(MOTOR_DIR_PORT, &MOTOR_DIR);
	HAL_GPIO_WritePin(MOTOR_DIR_PORT, MOTOR_DIR_PIN, GPIO_PIN_SET);

	HAL_GPIO_Init(MOTOR_EN_PORT, &MOTOR_EN);
	HAL_GPIO_WritePin(MOTOR_EN_PORT, MOTOR_EN_PIN, GPIO_PIN_SET);

	/* 3. TIM2 & PWM Init */
	HAL_TIM_PWM_Init(&TIM2_Handle);
	HAL_TIM_PWM_ConfigChannel(&TIM2_Handle, &TIM2_OC_Config, TIM_CHANNEL_1);
}

/* A4988 EN is active-low: driving it low enables the coil drivers.
 * Enable before starting the PWM, so the first STEP edge is not lost;
 * stop the PWM before disabling, so no edge lands on a disabled driver. */
void MOTOR_Start(void){
	HAL_GPIO_WritePin(MOTOR_EN_PORT, MOTOR_EN_PIN, GPIO_PIN_RESET);
	HAL_TIM_PWM_Start(&TIM2_Handle, TIM_CHANNEL_1);
}

void MOTOR_Stop(void){
	HAL_TIM_PWM_Stop(&TIM2_Handle, TIM_CHANNEL_1);
	HAL_GPIO_WritePin(MOTOR_EN_PORT, MOTOR_EN_PIN, GPIO_PIN_SET);
}
