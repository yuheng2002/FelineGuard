#ifndef INC_RTC_CTRL_H_
#define INC_RTC_CTRL_H_

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"

/* Returns false if the LSE crystal failed to start.
 * Every other Init in this project returns void, because writing configuration registers rarely fail.
 * This one waits on a physical oscillator, which genuinely can fail.
 * When it fails the RTC has no clock; the calendar never advances and alarms never fire.
 * The rest of the system is unaffected -- UART, button, and IWDG all keep working, but scheduled feeding is lost */
bool RTC_Init(void);
bool RTC_IsTimeSet(void);
bool RTC_SetTime(uint8_t hour, uint8_t minute);
bool RTC_SetAlarm(uint8_t index, uint8_t hour, uint8_t minute);
bool RTC_TakeAlarm(void);

RTC_HandleTypeDef RTC_Handle;

#endif /* INC_RTC_CTRL_H_ */
