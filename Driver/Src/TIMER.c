#include <stdint.h>
#include <stdbool.h>
#include "board.h"
#include "TIMER.h"
#include "stm32f4xx_hal.h" /* this includes the entire HAL */

#define TIM6_PRESCALER   	  15
#define TIM6_ARR       		  999

/* Tick period: 1 ms, so 1 tick = 1 ms and the caller's `ms` argument is the tick count directly.
 *
 * update frequency = f_tim / ((PSC + 1) * (ARR + 1))
 * TIM6 runs off PCLK1, which is 16 MHz here since no clock tree is configured
 * and all the prescalers are 1. For a 1 ms tick the update frequency is 1000 Hz:
 *
 *     (PSC + 1) * (ARR + 1) = 16,000,000 / 1000 = 16,000
 *
 * ARR is 16 bits, so it holds at most 65535.
 * Pushing ARR as high as possible would give the best resolution,
 * but that only matters for PWM duty cycle,
 * which this timer does not care about.
 *
 * Solving for the largest ARR here gives
 * PSC + 1 = 16,000,000 / (1000 * 65536) = 0.24, which is not even a valid prescaler.
 *
 * So instead of maximizing ARR, pick a factor pair of 16,000 that divides evenly and reads well:
 *
 *     16,000 = 16 * 1000  ->  PSC = 15, ARR = 999
 */
static TIM_HandleTypeDef TIM6_Handle = {
		.Instance = TIM6,
		.Init = {
				.Prescaler = TIM6_PRESCALER,
				.CounterMode = TIM_COUNTERMODE_UP,
				.Period = TIM6_ARR,
				.ClockDivision = TIM_CLOCKDIVISION_DIV1,
				/* No RepetitionCounter needed here, use default */
				.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE
		}
		/* rest is default */
};

static volatile uint32_t numTicks;
static volatile bool in_progress;

void TIMER_Init(void){
	/* 1. Clock Enable, otherwise TIM6 is dead */
	__HAL_RCC_TIM6_CLK_ENABLE();

	/* 2. TIM6 Init */
	HAL_TIM_Base_Init(&TIM6_Handle);

	/* 3. enable ISR & NVIC */
	__HAL_TIM_ENABLE_IT(&TIM6_Handle, TIM_IT_UPDATE);
	HAL_NVIC_SetPriority(TIM6_DAC_IRQn, PRIO_TIM6, 0); /* lower PreemptPriority than UART */
	HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
}

void TIM6_DAC_IRQHandler(void){
	/* clear flag
	 * if UIF is not cleared, CPU gets stuck in a loop */
	__HAL_TIM_CLEAR_IT(&TIM6_Handle, TIM_IT_UPDATE);

	if (numTicks > 0)
	{
		numTicks = numTicks - 1;

		if (numTicks == 0)
		{
			HAL_TIM_Base_Stop(&TIM6_Handle);
			in_progress = false;
			/* HAL_TIM_Base_Stop does not reset CNT: do it manually */
			__HAL_TIM_SET_COUNTER(&TIM6_Handle, 0);
		}
	}
}

void TIMER_StartTimeout(uint32_t ms){
	numTicks = ms; /* does not do any math, just for semantics */

	if (numTicks > 0)
	{
		in_progress = true;
		HAL_TIM_Base_Start_IT(&TIM6_Handle);
	}
}

bool TIMER_InProg(void){
	return in_progress;
}
