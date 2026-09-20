#ifndef INC_COMMS_H_
#define INC_COMMS_H_

/* Size of the buffer a caller must provide to Comms_GetCommand().
 * The longest command is "SCHED A 08:00" = 13 chars, so 16 data bytes plus a null terminator is enough.
 * This has to be in the header now, because the caller allocates the buffer the line is copied into. */
#define COMMS_LINE_BUF_SIZE    17

void Comms_Init(void);
void Comms_Task(void *arg);
void Comms_GetCommand(char *out);
void Comms_SendResponse(const char *response);

#endif /* INC_COMMS_H_ */
