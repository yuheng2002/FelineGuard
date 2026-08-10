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

#### What happens if the consumer or producer reads an outdated value

This is the standard way to implement a ring buffer, and it is also the one that feels most natural. But it is worth writing down a property that falls out of it.

Removing the `size` variable removes the race condition that comes from one variable being written by both the ISR and the main loop. That race would otherwise need a critical section (disabling interrupts) or an atomic read-modify-write; designing it away is the cheaper fix.

Timing effects do not disappear entirely, though. Either side can still read a value the other side is about to change:

**Consumer reads a stale `rear`.** Say the buffer is empty, so `front == rear`.
The main loop reads both, sees them equal and takes nothing. An ISR then fires and stores a byte. The main loop picks it up on the next pass a few microseconds later. Nothing is lost.

**Producer reads a stale `front`.** Say the buffer is full. The main loop reads the byte at `front` but is interrupted before it updates `front`. The ISR sees the old value, concludes the buffer is still full, and drops the incoming byte. One byte is lost, but nothing already stored is overwritten.

In both cases the stale read makes that side *underestimate* what it can do: the consumer thinks there is less data than there is, the producer thinks there is less room than there is. The worst outcome is doing one less thing, never doing the wrong thing. That is a consequence of each index having exactly one writer, not something the code checks for.

The second case is also unlikely in practice. The buffer can hold more bytes than the longest supported command, and filling it would require the main loop to stall for far longer than it takes a byte to arrive at 115200 baud.

And if a byte were dropped, the layering absorbs it: Comms still assembles a line at the next `\n`, but the line is incomplete, so CmdProc matches it against no command and answers `"Invalid command"`. The stream resynchronises at the following `\n`. The firmware protects itself rather than relying on the host discipline.

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

### 2026-08-07 -- TIMER

#### Renaming TIM_CTRL to TIMER

I originally wanted TIM_CTRL to be one universal driver for the TIM peripheral on top of the HAL. That does not hold up, because "TIM" is not one thing. This chip has advanced-control timers (TIM1, TIM8), general-purpose timers (TIM2 to TIM5) and basic timers (TIM6, TIM7), and this project uses two of them for completely different jobs.

One generates the PWM that drives the motor STEP pin. That output goes out on a GPIO, so it needs a pin configured, and the waveform is really part of the A4988 interface -- so TIM2 belongs to `MOTOR_CTRL`.

The other just counts a requested amount of time and never leaves the chip. Basic timers have no output channels and no pins at all, which is exactly what that job needs. So this module keeps TIM6 and is named for what it provides rather than for the peripheral it happens to use.

#### The general shape of enabling an interrupt

Yesterday's UART work already had all the pieces; writing TIM6 made the pattern obvious.

There are three separate things involved:

- **The flag** -- set by hardware when the event happens. `RXNE` for UART goes high when a byte moves from the shift register into the data register. It is the hardware saying "a byte just arrived", and it is set no matter if anyone is listening.
- **The interrupt enable** -- `RXNEIE` for UART, `UIE` for TIM. This decides whether setting the flag also raises an interrupt request. Without it the flag still works; nobody knows about it.
- **The NVIC** -- the gate between the peripheral's interrupt request and the CPU. Without enabling it the request never reaches the core.

So enabling an interrupt on any peripheral is the same three steps:

```c
HAL_UART_Init(&USART2_Handle);
__HAL_UART_ENABLE_IT(&USART2_Handle, UART_IT_RXNE);   /* RXNEIE */
HAL_NVIC_SetPriority(USART2_IRQn, 5, 0);
HAL_NVIC_EnableIRQ(USART2_IRQn);
```

```c
HAL_TIM_Base_Init(&TIM6_Handle);
__HAL_TIM_ENABLE_IT(&TIM6_Handle, TIM_IT_UPDATE);     /* UIE */
HAL_NVIC_SetPriority(TIM6_DAC_IRQn, 6, 0);
HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
```

The enable bit and the NVIC are set once at init. The flag is the part that has to be dealt with at run time, on every interrupt.

**How the flag gets cleared differs between peripherals.** For UART, reading the data register clears `RXNE` as a side effect, so handling the byte and clearing the flag are the same action. TIM has no such side effect: `UIF` must be cleared explicitly at the top of `TIM6_DAC_IRQHandler`. Forgetting it means the flag is still set when the handler returns, the interrupt fires again immediately, and the main loop never runs again -- the same failure mode as leaving `ORE` set.

##### Timer equation

    update frequency = f_tim / ((PSC + 1) * (ARR + 1))

`PSC + 1` because dividing a clock by zero makes no sense, so the register value N means "divide by N + 1" and 0 means "no division". `ARR + 1` because the counter is zero-based: it counts 0, 1, ... ARR, which is ARR + 1 ticks.

TIM6 has a 16-bit ARR, so it holds at most 65535. A higher ARR gives better resolution, but resolution only matters for PWM duty cycle, and 1% scale (from 1 to 100%) is usually enough. For a plain counter it buys nothing.

So the approach is to find a factor pair that divides evenly and reads well. I picked PSC = 15 and ARR = 999:

    16 MHz / (16 * 1000) = 1000 Hz  ->  1 ms per tick

1 ms is the unit this module exposes, so the application can ask for any duration it wants: a 5-second feed is just `TIMER_StartTimeout(5000)`.


### 2026-08-09 -- MOTOR_CTRL and the A4988

I implemented the MOTOR_CTRL module. The PWM that drives the motor's STEP input is generated on TIM2. Using the same equation as TIM6, I set the prescaler to 63 and ARR to 999:

    16,000,000 / ((63 + 1) * (999 + 1)) = 250 Hz

`OCPolarity` is HIGH, which means the output is high while `CNT < CCR`. CCR is 500, so the duty cycle is 50%.

For driving the A4988 the duty cycle does not actually matter, because the driver steps on the rising edge and ignores everything else. Any CCR between 1 and ARR produces the same motion; only CCR = 0 breaks it, since that removes the rising edge entirely. This is the opposite of PWM for LED brightness, where the duty cycle *is* the output and a larger ARR buys finer resolution.

I verified with a logic analyzer that the waveform is 250.56 Hz with a period of 3.991 ms. The 0.2% error comes from the HSI internal RC oscillator, which is specified at +/-1%.

![250 Hz STEP waveform measured on PA0](250Hz_waveform.png)

#### Notes from the A4988 documentation

The Pololu page warns that "the STEP and DIR pins are not pulled to any particular voltage internally, so you should not leave either of these pins floating in your application." I enabled the internal pull-down on PA0 (STEP) and PA1 (DIR). The pull-down on STEP matters after `MOTOR_Stop()`, when the timer no longer drives the pin.

"Please note that the RST pin is floating; if you are not using the pin, you can connect it to the adjacent SLP pin on the PCB to bring it high and enable the board." RST and SLP are shorted with a jumper.

"Connecting or disconnecting a stepper motor while the driver is powered can destroy the driver." The reason is that motor coils are inductors and their current cannot change instantaneously: breaking the circuit while current is flowing produces a large `V = L * di/dt` spike that can punch through the driver's output stage. So: wire everything first, then power up; power down before touching any wire.