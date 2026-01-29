/*
 * stm32f446xx_uart_driver.h
 *
 *  Created on: 2026/1/28
 *      Author: Yuheng
 */

#ifndef SOURCES_STM32F446XX_UART_DRIVER_H_
#define SOURCES_STM32F446XX_UART_DRIVER_H_

#include "stm32f446xx.h"

/*
 * ==========================================
 * 1. Clock Enable Macro
 * ==========================================
 * RCC APB1 peripheral clock enable register (RCC_APB1ENR)
 * Address offset: 0x40
 * Pre-defined in RCC_RegDef_t struct in stm32f446xx.h
 * accessible by RCC->APB1ENR
 * Bit 17 sets USART2
 * 0: USART2 clock disabled
 * 1: USART2 clock enabled
 */
#define USART2_PCLK_EN()  ( SET_BIT(RCC->APB1ENR, 17) ) // sets bit 17 to 1

/*
 * ==========================================
 * 2. Configuration Structures
 * ==========================================
 */
typedef struct{
	uint8_t USART_MODE; // TX only, RX only, or both
	uint8_t USART_WordLength; // 8 bits (standard) or 9 bits (parity/other purposes)
	uint8_t USART_ParityControl; // Odd, Even, or None
	uint8_t USART_StopBits; // 1 (standard & faster) or 2 (slow)
	uint32_t USART_Baud; // 9600, 115200, etc.
} USART_Config_t;

/*
 * USART Handle Structure
 * This structure acts as the "Object" or "Handle" for a specific Timer instance.
 * It bundles the Physical Hardware with User Configuration.
 *
 * Concept: "Job Order"
 * - "Where" to do the job: pUSART2 (Base Address of the peripheral)
 * - "How" to do the job:   USART2_Config (User parameters)
 */
typedef struct{
	USART_RegDef_t *pUSARTx;
	USART_Config_t USART_Config;
} USART_Handle_t;

/*
 * ==========================================
 * 3. Configuration Macros (USART Specific)
 * ==========================================
 * Macros to prevent user errors (magic numbers).
 */
/* @USART_MODES */
#define USART_MODE_ONLY_TX 0 // STM32 only "talk"
#define USART_MODE_ONLY_RX 1 // STM32 only "listen"
#define USART_MODE_TXRX    2 // two-way communication

/* @USART_WordLength */
#define USART_WordLength_8 0 // 8 bits (standard)
#define USART_WordLength_9 1 // 9 bits (pair with parity)

/* @USART_ParityControl */
#define USART_Parity_DISABLE 0 // None (by default)
#define USART_Parity_ODD     1 // check odd
#define USART_Parity_EVEN    2 // check even

/* @USART_StopBits */
#define USART_StopBits_1    0 // standard
#define USART_StopBits_0_5  1
#define USART_StopBits_2    2
#define USART_StopBits_1_5  3

/* @USART_BaudRate */
#define USART_Baud_9600     9600
#define USART_Baud_115200   115200

/*
 * ==========================================
 * 		4. Function Prototypes
 * ==========================================
 */
void USART_Init(USART_Handle_t *pUSARTHandle);
void USART_SetBaudRate(USART_RegDef_t *pUSARTx, uint32_t BaudRate);

void USART_SendData(USART_Handle_t *pUSARTHandle, uint8_t *pTxBuffer, uint32_t Len);

#endif /* SOURCES_STM32F446XX_UART_DRIVER_H_ */
