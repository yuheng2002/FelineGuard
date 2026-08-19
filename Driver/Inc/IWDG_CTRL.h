#ifndef INC_IWDG_CTRL_H_
#define INC_IWDG_CTRL_H_

#include <stdbool.h>
#include "stm32f4xx_hal.h"

void IWDG_Init(void);
void IWDG_Refresh(void);
bool IWDG_WasResetByWatchdog(void);

#endif /* INC_IWDG_CTRL_H_ */
