#ifndef INC_RTC_CTRL_H_
#define INC_RTC_CTRL_H_

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    RTC_SLOT_A,
    RTC_SLOT_B
} RTC_AlarmSlot;

/* Returns false if the LSE crystal failed to start.
 * Every other Init in this project returns void, because writing configuration registers rarely fails.
 * This one waits on a physical oscillator, which genuinely can fail.
 * When it fails the RTC has no clock; the calendar never advances and alarms never fire.
 * The rest of the system is unaffected -- UART, button, and IWDG all keep working, but scheduled feeding is lost */
bool RTC_Init(void);
bool RTC_IsTimeSet(void);
bool RTC_SetAlarm(RTC_AlarmSlot slot, uint8_t hour, uint8_t minute);

/* Returns true if either alarm has fired since the last call, and clears both flags.
 * A single bool by design: if the device was down long enough to miss both alarms,
 * that still counts as one make-up feed (Protocol 5.3). */
bool RTC_TakeAlarm(void);
bool RTC_SetTime(uint8_t hour, uint8_t minute);


#endif /* INC_RTC_CTRL_H_ */
