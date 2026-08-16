#ifndef INC_FEED_H_
#define INC_FEED_H_

#include <stdbool.h>

typedef enum{
	FEED_CMD,
	FEED_BUTTON,
	FEED_RTC
}Feed_Source;

bool Feed_Request(Feed_Source src);
void Feed_Poll(void);

#endif /* INC_FEED_H_ */
