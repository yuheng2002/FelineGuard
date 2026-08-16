#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "Comms.h"
#include "UART_CTRL.h"

#define COMMAND_LINE_MAX    16 /* longest command is "SCHED A 08:00" = 13 chars */

/* `static` gives these internal linkage: no other file can refer to them by name.
 * Comms_Poll_Command still hands out the address of command_line,
 * so callers can read it -- but the return type is `const char *`, so they cannot write to it,
 * and they cannot touch the index or the discarding flag at all. */
static char command_line[COMMAND_LINE_MAX + 1]; /* plus 1 to reserve room for null-terminator */
static uint8_t command_line_idx;
static bool discarding;


/* ---------- Assembles incoming bytes into one command line. ----------
 *
 * Returns a pointer to the completed line on '\n',
 * or NULL if no full line is available yet.
 * Whatever is left in the ring buffer stays there for the next pass.
 *
 * '\r' is dropped, so both "\r\n" and "\n" terminators work.
 * An empty line is ignored.
 * The buffer holds COMMAND_LINE_MAX data bytes plus a terminator,
 * so a 17th data byte means the line is too long:
 * the rest of it is discarded up to the next '\n', per Protocol section 4.5.
 *
 * The returned pointer is valid until the next call. */
const char* Comms_PollCommand(void){
	uint8_t byte;

	/* UART_CTRL_ReadByte does two things simultaneously
	 * 1. returns false if receive buffer is empty,
	 * 2. or returns true if a byte is loaded from the receive buffer to the passed in buffer */
	while (UART_CTRL_ReadByte(&byte))
	{
		if (discarding)
		{
			if (byte == '\n'){
				discarding = false;
				command_line_idx = 0;
			}
			continue;   /* continue applies to the nearest enclosing loop;
						   `if` is not one, so this skips the switch and reads the next byte */
		}

		switch (byte){
			case '\r':
				break;

			case '\n':
				/* if only '\n' is sent, do nothing */
				if (command_line_idx != 0){
					command_line[command_line_idx] = '\0';
					command_line_idx = 0; /* reset index ptr */
					return command_line;
				}
				break;   /* the return above is conditional; an empty line falls through to here */

			default:
				if (command_line_idx < COMMAND_LINE_MAX){
					command_line[command_line_idx] = (char)byte;
					command_line_idx++;
				}else{
					discarding = true;   /* index 16 is reserved for '\0',
											so there is no room for a 17th data byte,
											the line is over length */
				}

				break;
		}
	}

	return NULL;
}

void Comms_SendResponse(const char* response){
	UART_CTRL_Write((const uint8_t *)response, (uint16_t)strlen(response));
	UART_CTRL_Write((const uint8_t *)"\n", 1);
}
