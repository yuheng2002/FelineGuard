#include <stdint.h>
#include "stm32f4xx_hal.h" /* this includes the entire HAL */
#include "board.h"
#include "UART_CTRL.h"

/* declared static so they are explicitly only visible to this file */
static UART_HandleTypeDef USART2_Handle = {
		.Instance = USART2,
		.Init     = {
			.BaudRate     = 115200,
			.WordLength   = UART_WORDLENGTH_8B,
			.StopBits     = UART_STOPBITS_1,
			.Parity       = UART_PARITY_NONE,
			.Mode         = UART_MODE_TX_RX,     /* both are used */
			.HwFlowCtl    = UART_HWCONTROL_NONE,
			.OverSampling = UART_OVERSAMPLING_16
		}
		/* rest will be default settings */
};

static ring_buffer receive_buf = {
		.front  = 0,
		.rear   = 0
};

/* Physical size of receive buffer */
static const uint8_t rcvf_buf_cap = (uint8_t)sizeof(receive_buf.buffer);

/* HAL_UART_Init() configures the specific UART bus and general specs such as BaudRate, WordLength...
 * But it does not configure pins, so this function does it */
void UART_CTRL_Init(void){
	GPIO_InitTypeDef USART2_TX = {
			.Pin       = USART2_TX_PIN,
			.Mode      = GPIO_MODE_AF_PP,
			.Pull      = GPIO_NOPULL,
			.Speed     = GPIO_SPEED_FREQ_MEDIUM,
			.Alternate = GPIO_AF7_USART2 /* from Table 11. Alternate function in datasheet (p.61) */
	};

	GPIO_InitTypeDef USART2_RX = {
			.Pin       = USART2_RX_PIN,
			.Mode      = GPIO_MODE_AF_PP,
			.Pull      = GPIO_NOPULL,
			.Speed 	   = GPIO_SPEED_FREQ_MEDIUM,
			.Alternate = GPIO_AF7_USART2
	};

	/* 1. pin configuration */
	__HAL_RCC_GPIOA_CLK_ENABLE(); /* allow pins to be configured */
	HAL_GPIO_Init(USART2_TX_PORT, &USART2_TX);
	HAL_GPIO_Init(USART2_RX_PORT, &USART2_RX);

	/* 2. UART clock enable */
	__HAL_RCC_USART2_CLK_ENABLE();

	/* 3. UART Init */
	HAL_UART_Init(&USART2_Handle);

	/* 4. enable ISR & NVIC */
	__HAL_UART_ENABLE_IT(&USART2_Handle, UART_IT_RXNE);
	HAL_NVIC_SetPriority(USART2_IRQn, PRIO_USART2, 0);
	HAL_NVIC_EnableIRQ(USART2_IRQn);
}

/* pass data to comms buffer */
bool UART_CTRL_ReadByte(uint8_t *out){
	/* check if buffer is empty */
	if (receive_buf.front == receive_buf.rear){
		return false;
	}

	*out = receive_buf.buffer[receive_buf.front];
	receive_buf.front = (receive_buf.front + 1) % rcvf_buf_cap;
	return true;
}

void UART_CTRL_Write(const uint8_t *data, uint16_t len){
	HAL_UART_Transmit(&USART2_Handle, data, len, HAL_MAX_DELAY);
}

static bool ringBufIsFull(ring_buffer *ring_buf){
	uint8_t front = ring_buf->front;
	uint8_t rear = ring_buf->rear;
	if ((rear + 1) % (rcvf_buf_cap) == front){
		return true;
	}

	return false;
}

void USART2_IRQHandler(void){
	uint32_t sr = USART2->SR; /* snapshot */

	/* refer to __HAL_UART_GET_FLAG in stm32f4xx_hal_uart.h
	 * if (reg & MACRO): bitwise operation to get flag */
	if (sr & (USART_SR_ORE | USART_SR_RXNE)){
		uint8_t data = USART2->DR;

		/* valid data */
		if (sr & USART_SR_RXNE){
			/* drop byte if buffer if full to prevent corruption */
			if (!ringBufIsFull(&receive_buf)){
				/* NOTE: must write first then advance rear
				 * rear points to the next available index */
				receive_buf.buffer[receive_buf.rear] = data;
				receive_buf.rear = (receive_buf.rear + 1) % rcvf_buf_cap;
			}
		}

		/* When ORE is set, the byte already in DR is still valid (RM0390: "the RDR
		 * register content will not be lost"). The byte that was waiting in the shift
		 * register is the one lost, overwritten by whatever arrives next. Reading DR
		 * above clears both flags, so nothing more to do here. */
	}
}
