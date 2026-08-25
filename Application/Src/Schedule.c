#include "Schedule.h"
#include "RTC_CTRL.h"
#include "Feed.h"

void Schedule_Poll(void)
{
	if (!RTC_IsTimeSet()) return;

	if (RTC_TakeAlarm())
	{
		Feed_Request(FEED_RTC);
	}
}
