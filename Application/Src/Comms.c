#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "Comms.h"
#include "UART_CTRL.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"

#define COMMAND_LINE_MAX    16 /* longest command is "SCHED A 08:00" = 13 chars */

/* 1 is enough for now.
 *
 * If CmdProc is still busy, Comms blocks on the send but bytes keep arriving into the receive queue meanwhile,
 * because that one is filled by the ISR. Nothing is lost, the assembler just waits. */
#define LINE_QUEUE_LENGTH   1

/* Holds completed command lines, one per slot. */
static QueueHandle_t line_queue;

/* Guards the UART transmitter.
 *
 * HAL_UART_Transmit() waits on TXE and writes DR one byte at a time, so two tasks sending at once would interleave their characters.
 * In v1 only the main loop ever sent anything; now CmdProc and Feed both do. */
static SemaphoreHandle_t tx_mutex;

/* Creates the line queue.
 *
 * Called from init_all(), before the scheduler starts, so that the queue exists no matter which of the two tasks runs first. */
void Comms_Init(void){
	line_queue = xQueueCreate(LINE_QUEUE_LENGTH, COMMS_LINE_BUF_SIZE);
	tx_mutex   = xSemaphoreCreateMutex();

	if (line_queue == NULL || tx_mutex == NULL)
	{
		/* Not enough room in the FreeRTOS heap. */
		while (1) { }
	}
}

static UBaseType_t comms_stack_left;   /* TODO: diagnostic, remove when done */

/* ---------- Assembles incoming bytes into command lines. ----------
 *
 * Runs as a task. It blocks in UART_ReadByte() until a byte arrives.
 * When it sees a '\n', it sends the finished line to the line queue instead of returning it the way v1 did.
 *
 * '\r' is dropped, so both "\r\n" and "\n" terminators work.
 * An empty line is ignored.
 * The buffer holds COMMAND_LINE_MAX data bytes plus a terminator,
 * so a 17th data byte means the line is too long:
 * the rest of it is discarded up to the next '\n', per Protocol section 4.5.
 *
 * In v1 the index and the discarding flag had to be `static`, because the function returned after every byte and the state had to survive.
 * This one never returns, so they are ordinary locals living on the task's stack. */
void Comms_Task(void *arg){
	char    command_line[COMMS_LINE_BUF_SIZE];
	uint8_t command_line_idx = 0;
	bool    discarding = false;
	uint8_t byte;

	while (1)
	{
		/* Blocks here until the ISR puts a byte in the receive queue. */
		UART_ReadByte(&byte);

		comms_stack_left = uxTaskGetStackHighWaterMark(NULL);   /* TODO: diagnostic */

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

					/* Hand the line to CmdProc.
					 * This only blocks when the queue is already full, which means CmdProc has not finished the previous line yet.
					 * Waiting here is safe: the ISR keeps filling rx_queue, so no incoming bytes are lost. */
					xQueueSend(line_queue, command_line, portMAX_DELAY);
				}
				break;   /* the send above is conditional; an empty line falls through to here */

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
}

/* Copies the next completed command line into the caller's buffer.
 *
 * Blocks until one is available. `out` must point to at least COMMS_LINE_BUF_SIZE bytes. */
void Comms_GetCommand(char *out){
	xQueueReceive(line_queue, out, portMAX_DELAY);
}

/* Sends one response line.
 *
 * The text and its terminator go out as one unit, so a response from another task cannot appear in the middle of this one. */
void Comms_SendResponse(const char *response){
	xSemaphoreTake(tx_mutex, portMAX_DELAY);

	UART_Write((const uint8_t *)response, (uint16_t)strlen(response));
	UART_Write((const uint8_t *)"\n", 1);

	xSemaphoreGive(tx_mutex);
}
