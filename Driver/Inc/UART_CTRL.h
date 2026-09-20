#ifndef INC_UART_CTRL_H_
#define INC_UART_CTRL_H_

#include <stdint.h>

void UART_Init(void);
void UART_ReadByte(uint8_t *out);
void UART_Write(const uint8_t *data, uint16_t len);

#endif /* INC_UART_CTRL_H_ */
