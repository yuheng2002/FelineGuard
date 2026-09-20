#include <stdint.h>
#include <stdbool.h>
#include "Feed.h"
#include "MOTOR_CTRL.h"
#include "Comms.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define FEED_TIME_MS   5000

typedef enum {
	FEED_STATE_IDLE,
	FEED_STATE_FEEDING,
	FEED_STATE_PENDING
}Feed_State;

static Feed_State curr_state = FEED_STATE_IDLE;


/* Guards curr_state.
 *
 * In v1 the three sources were called in sequence from the main loop,
 * so only one of them could be inside Feed_Request() at a time.
 * They are separate tasks now and any of them can be interrupted part way through,
 * so reading curr_state, deciding on it and writing it back has to be held together. */
static SemaphoreHandle_t state_mutex;

/* Tells Feed_Task that a feed has just started and the five seconds begin now.
 *
 * Given by whichever source task accepted the request, taken by Feed_Task.
 * No data is handed over, only the fact that it happened. */
static SemaphoreHandle_t feed_started;

void Feed_Init(void){
	state_mutex  = xSemaphoreCreateMutex();
	feed_started = xSemaphoreCreateBinary();

	if (state_mutex == NULL || feed_started == NULL)
	{
		/* Not enough room in the FreeRTOS heap. */
		while (1) { }
	}
}

bool Feed_Request(Feed_Source src){
	bool accepted;
	bool motor_started = false;

	xSemaphoreTake(state_mutex, portMAX_DELAY);

	switch (curr_state){
		case FEED_STATE_IDLE:
			MOTOR_Start();
			curr_state = FEED_STATE_FEEDING;
			accepted = true;
			motor_started = true;
			break;

		case FEED_STATE_FEEDING:
			/* only RTC request can be deferred */
			if (src == FEED_RTC){
				curr_state = FEED_STATE_PENDING;
				accepted = true;
			}else{
				accepted = false;
			}
			break;

		case FEED_STATE_PENDING:
			/* only one pending alarm; any request is dropped */
			accepted = false;
			break;

		default:
			accepted = false;
			break;
	}

	xSemaphoreGive(state_mutex);

	/* Only an idle-to-feeding transition starts the clock.
	 * A deferred request does not: Feed_Task is already awake and will pick the pending one up on its own when the current feed ends.
	 *
	 * Given after the mutex is released, because Feed_Task wants that same mutex as soon as it wakes. */
	if (motor_started)
	{
		xSemaphoreGive(feed_started);
	}

	return accepted;
}

/* Ends the feed that just finished and moves the state on.
 * Returns true if a scheduled feed was deferred into it and should start now. */
static bool finish_feed(void)
{
	bool pending;

	xSemaphoreTake(state_mutex, portMAX_DELAY);

	if (curr_state == FEED_STATE_PENDING)
	{
		curr_state = FEED_STATE_FEEDING;
		pending = true;
	}
	else
	{
		curr_state = FEED_STATE_IDLE;
		pending = false;
	}

	xSemaphoreGive(state_mutex);

	return pending;
}

/* Times each feed and stops the motor when it is done.
 *
 * This replaces Feed_Poll(), and the TIMER module with it.
 * In v1 the wait had to be split into "start a timer, return, come back later and check a flag",
 * because the main loop was not allowed to stop.
 *
 * A task can stop, so the five seconds are just a line in the middle of the function. */
void Feed_Task(void *arg)
{
	/* Wait for the first accepted request. */
	xSemaphoreTake(feed_started, portMAX_DELAY);

	while (1)
	{
		vTaskDelay(pdMS_TO_TICKS(FEED_TIME_MS));

		MOTOR_Stop();
		Comms_SendResponse("Feed complete");

		if (finish_feed())
		{
			/* A scheduled feed arrived during this one. Run it straight away. */
			MOTOR_Start();
			Comms_SendResponse("Deferred feed started");
		}
		else
		{
			/* Nothing waiting. Sleep until a source starts the next feed. */
			xSemaphoreTake(feed_started, portMAX_DELAY);
		}
	}
}
