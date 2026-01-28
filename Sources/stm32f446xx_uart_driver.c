/*
 * stm32f446xx_uart_driver.c
 *
 *  Created on: 2026/1/28
 *      Author: Yuheng
 */
#include "stm32f446xx.h"
#include "stm32f446xx_uart_driver.h"
#include <stdint.h>

void USART_Init(USART_Handle_t *pUSARTHandle){
	// unpacking handle
	USART_RegDef_t *USARTx = pUSARTHandle->pUSARTx;
	uint8_t Mode = pUSARTHandle->USART_Config.USART_MODE;
	uint8_t WordLen = pUSARTHandle->USART_Config.USART_WordLength;
	uint8_t Parity = pUSARTHandle->USART_Config.USART_ParityControl;
	uint8_t StopBits = pUSARTHandle->USART_Config.USART_StopBits;
	uint32_t BuadRate = pUSARTHandle->USART_Config.USART_Baud;

	/*
	 * ==========================================
	 * 				1. Set Mode
	 * ==========================================
	 * Control Register 1:
	 * Bit 3 -> set to 1 -> Enable Transmitter
	 * Bit 2 -> set to 1 -> Enable Receiver
	 */
	if (Mode == USART_MODE_ONLY_TX){
		SET_BIT(USARTx->CR1, 3);
	}
	else if (Mode == USART_MODE_ONLY_RX){
		SET_BIT(USARTx->CR1, 2);
	}
	else if (Mode == USART_MODE_TXRX){
		SET_BIT(USARTx->CR1, 3);
		SET_BIT(USARTx->CR1, 2);
	}

	/*
	 * ==========================================
	 * 			2. Set Word Length
	 * ==========================================
	 * Control Register 1:
	 * Bit 12 M: Word length
	 * 0: 1 Start bit, 8 Data bits, n Stop bit
	 * 1: 1 Start bit, 9 Data bits, n Stop bit
	 */
	if (WordLen == USART_WordLength_8){
		CLEAR_BIT(USARTx->CR1, 12);
	}
	else if (WordLen == USART_WordLength_9){
		SET_BIT(USARTx->CR1, 12);
	}

	/*
	 * ==========================================
	 * 			3. Set Parity
	 * ==========================================
	 * Control Register 1:
	 * Bit 10 PCE: Parity control enable
	 * 0: Parity control disabled
	 * 1: Parity control enabled
	 *
	 * if Enabled:
	 * Bit 9 PS: Parity selection
	 * 0: Even parity
	 * 1: Odd parity
	 */
	if (Parity == USART_Parity_DISABLE){
		CLEAR_BIT(USARTx->CR1, 10);
	}
	else if (Parity == USART_Parity_ODD){
		SET_BIT(USARTx->CR1, 10);
		SET_BIT(USARTx->CR1, 9);
	}
	else if (Parity == USART_Parity_EVEN){
		SET_BIT(USARTx->CR1, 10);
		CLEAR_BIT(USARTx->CR1, 9);
	}

	/*
	 * ==========================================
	 * 			4. Set StopBits
	 * ==========================================
	 * Control Register 2:
	 * Bits 13:12
	 * 00: 1 Stop bit
	 * 01: 0.5 Stop bit
	 * 10: 2 Stop bits
	 * 11: 1.5 Stop bit
	 */
	if (StopBits == USART_StopBits_1){
		CLEAR_BIT(USARTx->CR2, 13);
		CLEAR_BIT(USARTx->CR2, 12);
	}
	else if (StopBits == USART_StopBits_0_5){
		CLEAR_BIT(USARTx->CR2, 13);
		SET_BIT(USARTx->CR2, 12);
	}
	else if (StopBits == USART_StopBits_2){
		SET_BIT(USARTx->CR2, 13);
		CLEAR_BIT(USARTx->CR2, 12);
	}
	else if (StopBits == USART_StopBits_1_5){
		SET_BIT(USARTx->CR2, 13);
		SET_BIT(USARTx->CR2, 12);
	}
	/*
	 * ==========================================
	 * 			5. Set Baud Rate
	 * ==========================================
	 * calling Helper Function USART_SetBaudRate
	 */
	USART_SetBaudRate(USARTx, BaudRate);

	/*
	 * ==========================================
	 * 			6. USART Enable
	 * ==========================================
	 * Control Register 1:
	 * Bit 13 UE: USART enable
	 * [
	 * 0: USART prescaler and outputs disabled
	 * 1: USART enabled
	 * ]
	 */
	SET_BIT(USARTx->CR1, 13);
}

void USART_SetBaudRate(USART_RegDef_t *pUSARTx, uint32_t BaudRate){
	/*
	 * ==========================================
	 * 		Oversampling by 16 (default)
	 * ==========================================
	 * Benefits:
	 * 1. Noise Immunity
	 * 2. Clock Tolerance
	 * NOTE:
	 * I chose Oversampling by 16 (default) over 8 because:
	 * a. 16 is more "reliable", because it checks the received data 16 times
	 * b. It requires higher clock frequency, so maximum baud rate is limited,
	 * but I only need 115200, which is standard baud rate for Serial Printing
	 * c. Oversampling by 8 sacrifices reliability to gain higher baud rate,
	 * but again, I only need 115200, so I do not need it
	 *
	 * Control Register 1:
	 * Bit 15 OVER8: Oversampling mode
	 * 0: oversampling by 16
	 * 1: oversampling by 8
	 */
	CLEAR_BIT(pUSARTx->CR1, 15);

	/*
	 * ==========================================
	 * 		Standard Equation 1 (OVER8 = 0)
	 * ==========================================
	 * Tx/Rx baud = f_ck / (8 x (2 - OVER8) x USARTDIV
	 *
	 * thus,
	 * usartdiv = f_ck / (8 * 2 * baud)
	 */

	// 1. set correct Peripheral Clock Frequency
	// according to datasheet (page 105), 6.3.10 Internal clock source characteristics
	// Table 41. HSI oscillator characteristics
	// frequency -> f_HSI = 16MHz
	uint32_t PCLK = 16000000;

	// 2. Baud rate register (BRR)
	// Bits 15:4 DIV_Mantissa[11:0]: mantissa of USARTDIV
	// These 12 bits define the mantissa of the USART Divider (USARTDIV)

	// Bits 3:0 DIV_Fraction[3:0]: fraction of USARTDIV
	// It uses a fixed-point algorithm
	uint32_t usartdiv_mantissa = 0;
	uint32_t usartdiv_fraction = 0;

	// 3. Calculation
	// usartdiv = f_ck / (8 * 2 * baud)
	// NOTE:
	// when using MCUs such as this STM32F4 board,
	// it is best to avoid using floating point numbers,
	// thus, I will multiply DIV by
	uint32_t divider = 8 * 2 * BaudRate;
	usartdiv_mantissa = PCLK / divider; // integer part

	uint32_t remainder = PCLK % divider; // source of fractional part

	// to get (A / B) and round
	// do:
	// (A + B/2) / B
	usartdiv_fraction = (remainder * 16 + (divider / 2)) / divider;

	// 4. Overwrite Bits
	uint32_t mask = (1u << 12) - 1; // creates a mask of 12 1s
	uint8_t shift_amount = 4; // Mantissa starts at bit 4
	pUSARTx->BRR &= (~mask << shift_amount); // clear bits first
	pUSARTx->BRR |= (usartdiv_mantissa << shift_amount);

	pUSARTx->BRR &= (~0xF); // clear Bits 3:0
	pUSARTx->BRR |= (usart_fraction);
}
