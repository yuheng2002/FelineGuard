### 2026-08-02 -- Blink LD2 to verify the HAL GPIO path

First code on the board: a blocking blink of the on-board LED, to check that the project builds, flashes and runs.

**HAL does not enable the GPIO port clock.** Unlike the Nuvoton M2351 I use at work, this chip needs `__HAL_RCC_GPIOx_CLK_ENABLE()` before a pin can be configured. `HAL_GPIO_Init()` writes MODER, OTYPER, PUPDR and OSPEEDR directly and never touches RCC. Without the clock the writes go nowhere and the pin stays dead, with no error reported anywhere. ST lists it as a separate step in the "How to use this driver" block at the top of `stm32f4xx_hal_gpio.c`.

LD2 is on PA5, connected to Arduino signal D13 through solder bridge SB21 (UM1724 Section 7.6 and Section 7.11).

### 2026-08-07 -- UART_CTRL: ring buffer, clocks, NVIC priority, RXNE and ORE

Today I implemented and tested the UART_CTRL module.

#### Why a ring buffer

I used a ring buffer to store the raw bytes received on the USART2_RX pin. Unlike I2C, UART has no frame boundaries such as START/STOP conditions to mark one complete transaction, so UART_CTRL just puts raw bytes into `receive_buf`, and the comms module collects each byte into a command buffer until it sees a `\n`, which is what the protocol uses to end a command.

The problem is that UART is interrupt-driven while comms is polled in the main loop. There is guaranteed to be a mismatch between the rate at which the producer and the consumer handle data. That is where a ring buffer comes in: it gives the consumer more time to catch up without slowing the producer down.

#### Why `k+1` instead of a `size` variable

Either way, the ring buffer variables are shared between the consumer in the main loop and the producer in the ISR. With `front` and `rear`, the consumer only writes `front` and the producer only writes `rear`. This is fine, because each variable has exactly one writer.

A `size` variable breaks that. The consumer has to decrement it after taking a byte, and the producer has to increment it after writing one, so both sides write the same variable and a race condition appears. `volatile` alone does not fix it: it only tells the compiler not to keep the variable in a register and to re-read it from memory every time. It does nothing about the fact that `size--` and `size++` are read-modify-write sequences that can interleave.

For example, say `size` is 5:

1. the main loop reads 5, computes 4, but has not written it back yet
2. the ISR fires, reads 5, computes 6, writes 6
3. the main loop resumes and writes 4

The final value is 4. But since one byte was consumed and one was produced, they cancel each other out, so the correct value is 5 -- the ISR's increment is lost.

The `k+1` approach allocates `k+1` physical elements while only allowing `k` of them to hold data, which gives `(rear + 1) % (capacity + 1) == front` as the Full condition. Since `rear` always points to the next available index, if `rear + 1` wraps back to `front`, it means `rear` is sitting on that extra slot, so the buffer is full.

#### Every peripheral needs its own clock

I remembered to enable the clock for GPIOA with `__HAL_RCC_GPIOA_CLK_ENABLE()`, but I forgot `__HAL_RCC_USART2_CLK_ENABLE()`. Without that line the whole USART2 peripheral is dead -- same lesson as the GPIO one from 08-02, just one level up: the pins were configured but the peripheral driving them was not clocked.

#### Set the ISR priority before enabling the IRQ

```c
HAL_NVIC_SetPriority(USART2_IRQn, 5, 0);
HAL_NVIC_EnableIRQ(USART2_IRQn);
```

and not the other way around, otherwise there is a window where the interrupt is live but still at its default priority.

The prototype is `void HAL_NVIC_SetPriority(IRQn_Type IRQn, uint32_t PreemptPriority, uint32_t SubPriority)`. PreemptPriority decides whether one IRQ can interrupt another; SubPriority only decides which one runs first when two are already pending at the same preempt level, and never causes a preemption. Note that `HAL_Init()` selects `NVIC_PRIORITYGROUP_4`, which gives all four implemented bits to preemption and none to subpriority, so in this project SubPriority is always 0 and has no effect.

UART has the tightest deadline of any peripheral interrupt here, so it gets the highest priority among them (SysTick sits at 0, above everything). I picked 5 to leave room in both directions: 0-4 for anything more urgent later, 6-15 for TIM, the button and the RTC alarm. Priorities are relative, not absolute.

#### RXNE, RXNEIE and ORE

To know exactly when a byte arrives, the ISR reads USART2's status and data registers directly. That is normal for the driver layer -- the rule in this project is that the application layer never touches registers, not that nothing does.

`RXNE` is set by hardware whenever the data register is loaded from the shift register. I had assumed that without `RXNEIE` set, `RXNE` could not be set. That is wrong: flags are set by hardware regardless of the interrupt enable bit. `RXNEIE` only decides whether setting the flag also raises a USART2 interrupt.

`ORE` (bit 3 in USART_SR) is the overrun error. It is set when `RXNE` is still 1 -- there is a byte in the data register that nobody has read -- and the next byte has finished arriving in the shift register with nowhere to go. RM0390 states that "the RDR register content will not be lost but the shift register will be overwritten", so the byte already in the data register stays valid; the one waiting in the shift register is the one lost, overwritten by whatever arrives next.

`ORE` is cleared by "a read to the USART_SR register followed by a read to the USART_DR register". My original implementation was:

```c
void USART2_IRQHandler(void){
	/* check RXNE: it is set when DR receives data */
	if (__HAL_UART_GET_FLAG(&USART2_Handle, UART_FLAG_RXNE)){
		uint8_t data = USART2->DR;
	}
	// ... other logic
}
```

`__HAL_UART_GET_FLAG()` does read SR, and this is fine in most cases, but it misses an edge case. Suppose `ORE` is set and something reads the data register without reading SR first -- for example the SFR (Special Function Register) view in the debugger. That read clears `RXNE` but leaves `ORE` set, because the clearing sequence needs both steps.

Now the ISR is stuck: `RXNE` is low, so the condition is false, so `DR` is never read, so `ORE` is never cleared -- and since `RXNEIE` raises an interrupt on either flag, the ISR is re-entered immediately, forever. The main loop never gets to run. This is an interrupt storm, not something the hardware can get out of on its own.

The fix is to read `DR` when either `RXNE` or `ORE` is high, since the action is the same for both: the former means there is data, the latter needs clearing. After that, if `RXNE` was the one set, the byte is valid and goes into the receive buffer; otherwise it is dropped.