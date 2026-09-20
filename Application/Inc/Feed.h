#ifndef INC_FEED_H_
#define INC_FEED_H_

#include <stdbool.h>

typedef enum{
	FEED_CMD,
	FEED_BUTTON,
	FEED_RTC
}Feed_Source;

void Feed_Init(void);
void Feed_Task(void *arg);
bool Feed_Request(Feed_Source src);

#endif /* INC_FEED_H_ */
