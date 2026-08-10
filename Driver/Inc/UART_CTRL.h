#ifndef INC_UART_CTRL_H_
#define INC_UART_CTRL_H_

#include <stdint.h>
#include <stdbool.h>

/* 31 + 1 = 32 = 2^5
 * making it power of 2 so mod calculation can be optimized to a bitwise AND
 * e.g. mod 32 is extract the lowest 5 bits ( & 0b11111) */
#define IN_BUF_MAX_LENGTH 		31

typedef struct{
	volatile uint8_t front;
	volatile uint8_t rear;

	/* using an extra index to avoid keeping a size variable,
	 * a size variable may lead to race condition
	 * because both the producer (ISR) and consumer (main loop) needs to maintain it at the same time */
	volatile uint8_t buffer[IN_BUF_MAX_LENGTH + 1];
}ring_buffer;

void UART_CTRL_Init(void);
bool UART_CTRL_ReadByte(uint8_t *out);
void UART_CTRL_Write(const uint8_t *data, uint16_t len);

#endif /* INC_UART_CTRL_H_ */
