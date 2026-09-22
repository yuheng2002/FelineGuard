#include "Schedule.h"
#include "RTC_CTRL.h"
#include "Feed.h"
#include "FreeRTOS.h"
#include "task.h"

/* Check the alarm once a second. The flag stays set until taken, so a slower poll would only delay a feed, never miss one. */
#define SCHEDULE_POLL_MS   1000

void Schedule_Task(void *arg)
{
	while (1)
	{
	    /* Check once a second; the check itself takes microseconds. */
		if (RTC_IsTimeSet() && RTC_TakeAlarm())
		{
			Feed_Request(FEED_RTC);
		}

		vTaskDelay(pdMS_TO_TICKS(SCHEDULE_POLL_MS));
	}
}
