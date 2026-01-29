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
	uint32_t BaudRate = pUSARTHandle->USART_Config.USART_Baud;

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
	pUSARTx->BRR |= (usartdiv_fraction);
}

void USART_SendData(USART_Handle_t *pUSARTHandle, uint8_t *pTxBuffer, uint32_t Len){
    /*
     * Although C has strlen() to calculate string length using the null-terminator,
     * it is best practice to let the caller pass the explicit length of the array.
     * This avoids potential bugs with non-string data.
     *
     * Logically, the data transfer consists of two steps:
     * 1. "Load" -> writing the current byte into the DR (Data Register).
     * The DR acts as a "container" for the payload.
     * 2. "Fire" -> Once the DR is loaded, the hardware "Shift Register" takes over.
     * It physically generates the sequence of high/low voltages on the TX pin.
     *
     * To coordinate this, we need flags to know "Can I load?" and "Did it fire?".
     * That's why we have the Status Register (SR):
     * - Bit 7 TXE: Transmit Data Register Empty
     * - Bit 6 TC:  Transmission Complete
     *
     * TXE defaults to 1. When it is 1, it means:
     * "The data from the DR has been moved to the Shift Register."
     * In other words: "I have done my part (loading). I am empty now. You can load again."
     * Therefore, we MUST check if TXE is 1 before writing new data.
     *
     * TC also defaults to 1. It means "The transmission is fully complete."
     * Think of an archer shooting an arrow:
     * - TXE is like the archer's hand. If the hand is empty (TXE=1), he can grab another arrow.
     * - TC is like the arrow hitting the target.
     * Since checking TXE allows us to "reload" immediately while the previous arrow
     * is still flying (pipeline effect), we don't strictly need to wait for TC
     * between every byte. Checking TXE is faster and more efficient.
     *
     * As the driver developer, my job is simply to tell the MCU:
     * 1. Here is the data.
     * 2. Wait until it is safe to load the next byte.
     *
     * Once I write to the DR, the hardware automatically handles the electrical signals
     * and updates the flags. I don't need to manually reset TXE; writing to DR clears it automatically.
     *
     * Note: This is a "Blocking" implementation.
     * The CPU is stuck in a loop asking "Is TXE empty yet?" millions of times.
     * Later, I will optimize this using Interrupts so the CPU can do other things while waiting.
     */

    // Use a pointer to access the hardware registers directly.
    // Creating a copy (USART_RegDef_t curr = ...) would be wrong because
    // we need to modify the actual hardware address, not a local variable.
    USART_RegDef_t *pUSARTx = pUSARTHandle->pUSARTx;

    for (uint32_t i = 0; i < Len; i++){

        // Wait loop:
        // We check Bit 7 (TXE) of the Status Register (SR).
        // If it is 0 (NOT empty), we wait here.
        // If it is 1 (Empty), the loop condition becomes false, and we proceed.
        while( READ_BIT(pUSARTx->SR, 7) == 0 ); // do nothing when it is not ready to "load" again

        // Extract the value from the pointer and write to Data Register.
        // pTxBuffer is a pointer to uint8_t, meaning the compiler interprets
        // the memory block as a sequence of 1-byte (8-bit) unsigned integers.
        // The & 0xFF is a safety mask to ensure we only send 8 bits.
        pUSARTx->DR = (*pTxBuffer & 0xFF);

        // Increment the pointer to point to the next "box" (byte) in the memory array.
        pTxBuffer++;
    }
}

uint8_t USART_ReceiveData(USART_Handle_t *pUSARTHandle){
	USART_RegDef_t *USARTx = pUSARTHandle->pUSARTx;

	/*
	 * ------------------------------
	 * Step 1: Wait for Data (Blocking)
	 * ------------------------------
	 * We check the RXNE (Read Data Register Not Empty) flag in the Status Register (SR).
	 * - Bit 5 (RXNE) = 0: No data received yet.
	 * - Bit 5 (RXNE) = 1: Data has arrived and is ready to be read.
	 *
	 * Note: This is a "Blocking" implementation. The CPU will stay in this while-loop
	 * forever until a byte is physically received.
	 * Make sure to check SR (Status Register), not CR1!
	 */
	while ( READ_BIT(USARTx->SR, 5) == 0){};

	/*
	 * ------------------------------
	 * Step 2: Read from Data Register
	 * ------------------------------
	 * Logic Comparison:
	 * - In `USART_SendData`, the STM32 acts as the "Transmitter." It takes data
	 * stored in its Flash memory (our string) and pushes it out.
	 * * - In `USART_ReceiveData`, the STM32 acts as the "Receiver."
	 * When the PC sends a character (like 'F') via USB cable, the hardware
	 * automatically loads that byte into the Data Register (DR).
	 * * According to the Reference Manual, the DR functions as a dual-purpose buffer:
	 * it contains transmitted data when writing, and received data when reading.
	 * * So, we simply read the DR to get the external command.
	 * Example: If we read 'F', we trigger the logic to turn the motor (and feed the cat).
	 */
	return (uint8_t)(USARTx->DR & 0xFF); // Masking with 0xFF for safety
}

/*
 * Interrupt set-enable register (NVIC_ISER) is a contiguous sequence of eight 4-byte memory blocks
 * the rule is as follows:NVIC_ISER0 bits 0 to 31 are for interrupt 0 to 31, respectively
 * NVIC_ISER1 bits 0 to 31 are for interrupt 32 to 63, respectively
 * ....
 * NVIC_ISER6 bits 0 to 31 are for interrupt 192 to 223, respectively
 * NVIC_ISER7 bits 0 to 15 are for interrupt 224 to 239, respectively
 */
void USART_IRQInterruptConfig(uint8_t IRQNumber, uint8_t EnableOrDisable){
	/*
	 * ==============================
	 * 	  1. Locate the register
	 * ==============================
	 * Calculate which ISER register (0-7) contains the target IRQ.
	 * ------------------------------
	 * Formula:
	 * Block = IRQ / 32
	 * ------------------------------
	 * e.g. IRQ = 38 (USART2 -> position 38)
	 * so, Block = 38 / 32 = 1
	 * it is indeed in NVIC_ISER[1]
	 */
	uint8_t register_num = IRQNumber / 32;

	/*
	 * ==============================
	 * 		2. Locate the Bit
	 * ==============================
	 * Each bit in each block controls 1 specific IRQ
	 * [one bit controls one IRQ]
	 * ------------------------------
	 * e.g. IRQ = 38
	 * so, it is at 38 % 32 = 6 -> Target is Bit 6
	 */
	uint8_t target_bit = IRQNumber % 32;

	/*
	 * ==============================
	 * 	  3. Enable the register
	 * ==============================
	 * Write:
	 * 0: No effect (Refer to PM0214 4.3.2)
	 * 1: Enable interrupt
	 * We simply write a 1 to the specific bit position
	 */
	SET_BIT(NVIC_ISER->ISER[register_num], target_bit);
}
