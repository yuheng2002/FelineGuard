#include <stdint.h>
#include "stm32f4xx_hal.h" /* this includes the entire HAL */
#include "board.h"
#include "UART_CTRL.h"
#include "FreeRTOS.h"
#include "queue.h"

/* Length of the receive queue, in bytes.
 * Same size as the ring buffer it replaces: enough to hold a second command while the first is being handled. */
#define RX_QUEUE_LENGTH		32

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

/* Replaces the hand-written ring buffer from v1 */
static QueueHandle_t rx_queue;

/* HAL_UART_Init() configures the specific UART bus and general specs such as BaudRate, WordLength...
 * But it does not configure pins, so this function does it */
void UART_Init(void){
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

	/* 4. create the receive queue.
	 * This has to happen before the interrupt is enabled: the ISR writes to rx_queue,
	 * and the first byte can arrive the moment the NVIC line goes live. */
	rx_queue = xQueueCreate(RX_QUEUE_LENGTH, sizeof(uint8_t));
	if (rx_queue == NULL)
	{
		/* Not enough room in the FreeRTOS heap. Nothing below would work. */
		while (1) { }
	}

	/* 5. enable ISR & NVIC */
	__HAL_UART_ENABLE_IT(&USART2_Handle, UART_IT_RXNE);
	HAL_NVIC_SetPriority(USART2_IRQn, PRIO_USART2, 0);
	HAL_NVIC_EnableIRQ(USART2_IRQn);
}

/* Takes one byte out of the receive queue.
 *
 * Blocks until a byte is available -- portMAX_DELAY means there is no timeout.
 * While blocked this task is removed from the ready list entirely,
 * so the CPU goes to whoever else is ready rather than spinning on an empty buffer. */
void UART_ReadByte(uint8_t *out){
	xQueueReceive(rx_queue, out, portMAX_DELAY);
}

void UART_Write(const uint8_t *data, uint16_t len){
	HAL_UART_Transmit(&USART2_Handle, data, len, HAL_MAX_DELAY);
}

void USART2_IRQHandler(void){
	uint32_t sr = USART2->SR; /* snapshot */

	/* refer to __HAL_UART_GET_FLAG in stm32f4xx_hal_uart.h
	 * if (reg & MACRO): bitwise operation to get flag */
	if (sr & (USART_SR_ORE | USART_SR_RXNE)){
		uint8_t data = USART2->DR;

		/* valid data */
		if (sr & USART_SR_RXNE){
			/* FromISR version: an ISR cannot block, so a full queue drops the byte. */
			xQueueSendFromISR(rx_queue, &data, NULL);
		}

		/* When ORE is set, the byte already in DR is still valid (RM0390: "the RDR register content will not be lost").
		 * The byte that was waiting in the shift register is the one lost, overwritten by whatever arrives next.
		 * Reading DR above clears both flags, so nothing more to do here. */
	}
}
