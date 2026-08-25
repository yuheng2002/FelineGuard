#include <stdint.h>
#include <string.h>
#include "CmdProc.h"
#include "Comms.h"
#include "Feed.h"
#include "RTC_CTRL.h"

/* ---------- helper functions ---------- */

/* returns true if a char is a digit, false otherwise
 * e.g. is_digit('9') returns true */
static bool is_digit(char c)
{
	return (c >= '0' && c <= '9');
}

/* converts exactly two digit characters into a number
 * e.g. "14" -> 14*/
static uint8_t two_chars_to_u8(const char *s)
{
	uint8_t val = (s[0] - '0') * 10 + (s[1] - '0');

	return val;
}

/* Validates that s points to exactly "hh:mm" and extracts the two numbers.
 * Reads s[0] through s[4]; the caller must guarantee those bytes exist. */
static bool parse_hhmm(const char *s, uint8_t *hour, uint8_t *minute)
{
	/* Correct format is hh:mm
	 * e.g. 14:30 where 14 and 30 are both digits in char type */
	if (!is_digit(s[0]) || !is_digit(s[1]) || s[2] != ':' || !is_digit(s[3]) || !is_digit(s[4])) return false;

	*hour = two_chars_to_u8(&s[0]);
	*minute = two_chars_to_u8(&s[3]);
	return true;
}

/* the inverse of two_digits_to_u8: writes a 0-99 value as two chars
 * e.g. 14 -> s[0] = '1', s[1] = '4' */
static void u8_to_two_chars(uint8_t val, char *s)
{
    s[0] = (char)('0' + (val / 10));
    s[1] = (char)('0' + (val % 10));
}

void CmdProc_Process(void){
	const char *command = Comms_PollCommand();

	if (!command) return;

	uint8_t hour;
	uint8_t minute;

	/* strcmp() takes in two char pointers
	 * returns 0 if strings are identical */
	if (strcmp(command, "FEED") == 0)
	{
		if (!Feed_Request(FEED_CMD))
		{
			Comms_SendResponse("Busy feeding");
		}
		else
		{
			Comms_SendResponse("Feeding started");
		}
	}
	else if (strcmp(command, "PING") == 0)
	{
		Comms_SendResponse("System ready");
	}

	else if (strcmp(command, "TIME?") == 0)
	{
	    char response_buf[9];   /* "hh:mm:ss" + '\0' */
	    uint8_t hour, minute, second;

	    if (!RTC_IsTimeSet())
	    {
	        Comms_SendResponse("Time not set");
	    }
	    else if (!RTC_GetTime(&hour, &minute, &second))
	    {
	        Comms_SendResponse("Invalid command");
	    }
	    else
	    {
	        u8_to_two_chars(hour, &response_buf[0]);
	        response_buf[2] = ':';

	        u8_to_two_chars(minute, &response_buf[3]);
	        response_buf[5] = ':';

	        u8_to_two_chars(second, &response_buf[6]);
	        response_buf[8] = '\0';

	        Comms_SendResponse(response_buf);
	    }
	}

	/* strcmp checks the entire string (stops until it sees a null-terminator)
	 * strncmp(a, b, n) checks the first n characters, which is good for prefix matching or fixed-size buffer checks */
	/* T  I  M  E  ␣  1  4  :  3  0  \0
	 * 0  1  2  3  4  5  6  7  8  9  10 */
	else if (strncmp(command, "TIME ", 5) == 0)
	{
		if (strlen(command) == 10 && parse_hhmm(&command[5], &hour, &minute))
		{
			if (RTC_SetTime(hour, minute)){
				Comms_SendResponse("Time set");
			}
			else
			{
				Comms_SendResponse("Invalid time");
			}
		}
		else
		{
			Comms_SendResponse("Invalid command");
		}
	}
	/* S  C  H  E  D  ␣  A  ␣  0  8  :  0  0  \0
	 * 0  1  2  3  4  5  6  7  8  9  10 11 12 13 */
	else if (strncmp(command, "SCHED ", 6) == 0)
	{
		if (strlen(command) == 13 && command[7] == ' ' && (command[6] == 'A' || command[6] == 'B') && parse_hhmm(&command[8], &hour, &minute))
		{
			RTC_AlarmSlot slot = (command[6] == 'A') ? RTC_SLOT_A : RTC_SLOT_B;
			if (RTC_SetAlarm(slot, hour, minute))
			{
			    Comms_SendResponse(command[6] == 'A' ? "Alarm A set" : "Alarm B set");
			}
			else
			{
				Comms_SendResponse("Invalid time");
			}
		}
		else
		{
			Comms_SendResponse("Invalid command");
		}
	}

#ifdef DEBUG
	else if (strcmp(command, "CRASH") == 0)
	{
		/* hangs CPU deliberately */
		while (1){}
	}
	else if (strcmp(command, "CRASHFEED") == 0)
	{
		Feed_Request(FEED_CMD);
		while (1){}
	}
#endif

	else
	{
		Comms_SendResponse("Invalid command");
	}
}
