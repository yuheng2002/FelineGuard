#ifndef INC_TIMER_H_
#define INC_TIMER_H_

#include <stdbool.h>
#include <stdint.h>

/* This module configures Basic timers (TIM6)
 * it counts time inside the chip and does not require output pins */

void TIMER_Init(void);
void TIMER_StartTimeout(uint32_t ms); /* ms must be > 0 */
bool TIMER_InProg(void);

#endif /* INC_TIMER_H_ */
