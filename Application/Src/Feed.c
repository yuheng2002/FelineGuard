#include <stdint.h>
#include "Feed.h"
#include "MOTOR_CTRL.h"
#include "TIMER.h"
#include "Comms.h"

#define FEED_TIME_MS   5000

typedef enum {
	FEED_STATE_IDLE,
	FEED_STATE_FEEDING,
	FEED_STATE_PENDING
}Feed_State;

static Feed_State curr_state = FEED_STATE_IDLE;

static void start_feed(void){
	MOTOR_Start();
	TIMER_StartTimeout(FEED_TIME_MS);
}

bool Feed_Request(Feed_Source src){
	switch (curr_state){
		case FEED_STATE_IDLE:
			start_feed();
			curr_state = FEED_STATE_FEEDING;
			return true;

		case FEED_STATE_FEEDING:
			/* only RTC request can be deferred */
			if (src == FEED_RTC){
				curr_state = FEED_STATE_PENDING;
				return true;
			}else{
				return false;
			}

		case FEED_STATE_PENDING:
			/* only one pending alarm; any request is dropped */
			return false;

		default:
			return false;
	}
}

void Feed_Poll(void){
	/* do nothing if idle */
	if (curr_state == FEED_STATE_IDLE) return;

	/* do nothing if last feed is still running */
	if (TIMER_InProg())
	{
		return;
	}
	/* if neither of above, a feed just completed */
	MOTOR_Stop();
	if (curr_state == FEED_STATE_PENDING){
		Comms_SendResponse("Feed complete");
		start_feed();
		curr_state = FEED_STATE_FEEDING;
		Comms_SendResponse("Deferred feed started");
	}
	else if (curr_state == FEED_STATE_FEEDING){
		curr_state = FEED_STATE_IDLE;
		Comms_SendResponse("Feed complete");
	}
}
