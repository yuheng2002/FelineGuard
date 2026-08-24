#include <stdint.h>
#include <stdbool.h>
#include <stdint.h>
#include "stm32f4xx_hal.h"
#include "RTC_CTRL.h"

/* 32768 Hz / ( (127 + 1) * (255 + 1) ) = 1 Hz -> 1 tick every 1 second
 * 128 and 256 are powers of two, so 128 * 256 = 32768 exactly -- no rounding error */
#define RTC_ASYNCH_PREDIV    127
#define RTC_SYNCH_PREDIV     255

#define RTC_MAX_HOUR         23
#define RTC_MAX_MINUTE       59

#define RTC_DEFAULT_YEAR    1

static RTC_HandleTypeDef RTC_Handle = {
		.Instance = RTC,
		.Init = {
				.HourFormat     = RTC_HOURFORMAT_24,
				.AsynchPrediv   = RTC_ASYNCH_PREDIV,
				.SynchPrediv    = RTC_SYNCH_PREDIV,
				.OutPut = RTC_OUTPUT_DISABLE,
				/* OutPutPolarity and OutPutType only matter when OutPut is enabled;
				 * this project does not use the RTC_ALARM pin, so they stay at 0. */
		}
};

bool RTC_Init(void)
{
	/* 1. Enable PWR clock */
	__HAL_RCC_PWR_CLK_ENABLE(); /* RCC_APB1ENR.PWREN */

	/* 2. Unlock backup domain */
	HAL_PWR_EnableBkUpAccess(); /* PWR_CR.DBP (Disable backup domain write protection) */

	/* 3. Initialize LSE (32.768 kHz: precise compared to LSI) */
	RCC_OscInitTypeDef osc = {
			.OscillatorType = RCC_OSCILLATORTYPE_LSE,
			.LSEState	    = RCC_LSE_ON,
			.PLL.PLLState   = RCC_PLL_NONE
	};

	HAL_StatusTypeDef status = HAL_RCC_OscConfig(&osc);
	if (status != HAL_OK){
		return false;
	}

	/* 4. Let RTC use LSE */
	RCC_PeriphCLKInitTypeDef pclk = {
			.PeriphClockSelection = RCC_PERIPHCLK_RTC,
			.RTCClockSelection    = RCC_RTCCLKSOURCE_LSE
	};
	HAL_RCCEx_PeriphCLKConfig(&pclk); /* RCC_BDCR.RTCSEL (RCC backup domain control register - RTC clock source selection */

	/* 5. Enable RTC clock */
	__HAL_RCC_RTC_ENABLE(); /* RCC_BDCR.RTCEN */

	/* 6. Init RTC */
	HAL_RTC_Init(&RTC_Handle);

	return true;
}

bool RTC_IsTimeSet(void)
{
	 /* check INITS: Initialization status flag (RM0390 p.665)
	  * "this bit is set by hardware when the calendar year field is different from 0 (backup domain reset value state)"
	  * thus, the year field must not be left at 0 */
	return __HAL_RTC_IS_CALENDAR_INITIALIZED(&RTC_Handle);
}

bool RTC_SetTime(uint8_t hour, uint8_t minute)
{
	if (hour > RTC_MAX_HOUR || minute > RTC_MAX_MINUTE) return false;

	RTC_DateTypeDef date = {
			/* WeekDay, Month and Date are irrelevant here; any valid value will do */
		    .WeekDay = RTC_WEEKDAY_MONDAY,
		    .Month   = RTC_MONTH_JANUARY,
		    .Date    = 1,
			.Year 	 = RTC_DEFAULT_YEAR /* any non-zero value; INITS compares year against 0 */
	};

	HAL_StatusTypeDef status = HAL_RTC_SetDate(&RTC_Handle, &date, RTC_FORMAT_BIN);
	if (status != HAL_OK) return false;

	RTC_TimeTypeDef time = {
			.Hours   = hour,
			.Minutes = minute,
			.Seconds = 0, 	/* not necessary for this project */
	};
	status = HAL_RTC_SetTime(&RTC_Handle, &time, RTC_FORMAT_BIN);
	if (status != HAL_OK) return false;

	return true;
}

bool RTC_SetAlarm(RTC_AlarmSlot slot, uint8_t hour, uint8_t minute)
{
	if (hour > RTC_MAX_HOUR || minute > RTC_MAX_MINUTE) return false;

	uint32_t alarm_id;

	switch (slot)
	{
		case RTC_SLOT_A:
			alarm_id = RTC_ALARM_A;
			break;

		case RTC_SLOT_B:
			alarm_id = RTC_ALARM_B;
			break;

		default:
			return false;
	}

	RTC_AlarmTypeDef alarm = {
			.AlarmTime = {
					.Hours 		 = hour,
					.Minutes	 = minute,
					.Seconds 	 = 0
			},
			.AlarmMask 			 = RTC_ALARMMASK_DATEWEEKDAY, /* ignore date; hour, minute, second must all match */
			.AlarmSubSecondMask  = RTC_ALARMSUBSECONDMASK_ALL,
			.AlarmDateWeekDaySel = RTC_ALARMDATEWEEKDAYSEL_DATE,
			.AlarmDateWeekDay 	 = 1, /* already masked, just put any valid value */
			.Alarm 				 = alarm_id
	};

	HAL_StatusTypeDef status = HAL_RTC_SetAlarm(&RTC_Handle, &alarm, RTC_FORMAT_BIN);
	return status == HAL_OK;
}

bool RTC_TakeAlarm(void)
{
	bool alarm_fire = false;

	if (__HAL_RTC_ALARM_GET_FLAG(&RTC_Handle, RTC_FLAG_ALRAF) == 1U)
	{
		__HAL_RTC_ALARM_CLEAR_FLAG(&RTC_Handle, RTC_FLAG_ALRAF);
		alarm_hit = true;
	}

	if (__HAL_RTC_ALARM_GET_FLAG(&RTC_Handle, RTC_FLAG_ALRBF) == 1U)
	{
		__HAL_RTC_ALARM_CLEAR_FLAG(&RTC_Handle, RTC_FLAG_ALRBF);
		alarm_hit = true;
	}

	return alarm_fire;

}
