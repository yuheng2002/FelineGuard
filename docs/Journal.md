<details>
<summary><strong>Superloop version (v1)</strong> — 2026-08-02 to 2026-08-24, click to expand</summary>

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

The enable bit and the NVIC are set once at init. The flag is the part that has to be dealt with at runtime, on every interrupt.

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

![250 Hz STEP waveform measured on PA0](<250Hz waveform.png>)

#### Notes from the A4988 documentation

The Pololu page warns that "the STEP and DIR pins are not pulled to any particular voltage internally, so you should not leave either of these pins floating in your application." I enabled the internal pull-down on PA0 (STEP) and PA1 (DIR). The pull-down on STEP matters after `MOTOR_Stop()`, when the timer no longer drives the pin.

"Please note that the RST pin is floating; if you are not using the pin, you can connect it to the adjacent SLP pin on the PCB to bring it high and enable the board." RST and SLP are shorted with a jumper.

"Connecting or disconnecting a stepper motor while the driver is powered can destroy the driver." The reason is that motor coils are inductors and their current cannot change instantaneously: breaking the circuit while current is flowing produces a large `V = L * di/dt` spike that can punch through the driver's output stage. So: wire everything first, then power up; power down before touching any wire.

### 2026-08-10 -- Motor bring-up

Continuing from yesterday: the PWM was verified at 250 Hz, so today was about actually driving the motor.

#### The multimeter only measures resistance when the circuit is off

I keep forgetting this. Before connecting the 12 V supply I wanted to confirm that VMOT was not shorted to VDD, and the continuity buzzer went off, which got me worried for a moment. But continuity and resistance modes work by pushing a small current through the circuit and measuring the drop -- with the MCU powered, that measurement is meaningless. With everything unpowered the same two points read over 10 kΩ, which is the internal path through the chip and perfectly normal.

This rule goes at the top of the wiring checklist: **unplug everything before any resistance or continuity measurement.**

#### Setting V_ref

The A4988 does not put the motor supply straight across the coils. The motor is rated 2 A per coil at a few volts, so 12 V applied directly would push several amps and destroy both the motor and the driver. Instead the driver chops: it watches the current through a sense resistor and switches off once the limit is reached. `V_ref` is the knob that sets that limit.

    I_max = V_ref / (8 * R_cs)

`R_cs` on my board is marked **R100**, i.e. 0.1 Ω. Pololu states that all units they have made since 2017 use 0.068 Ω sense resistors (0.050 Ω before that), so neither value matches -- mine is a HiLetgo StepStick clone, and the marking on the board is what counts.

![A4988 top view (screenshot taken from Amazon purchase history)](<A4988 top view.jpg>)

The motor is rated 2 A per coil (STEPPERONLINE NEMA 17, 59 Ncm). The A4988 can reach 2 A, but only with a heat sink *and* forced air; I only have the heat sink, so I set the limit conservatively:

    V_ref = 8 * 1.0 A * 0.1 Ω = 0.8 V

It measured 0.41 V out of the box, which works out to 0.36 A per coil, so this is double what the default build was running at.

Note that running below the motor's rating costs torque: if the auger meets more resistance than the motor can overcome, it skips steps and the firmware has no way to notice. The definition of one serving -- 5 seconds at 250 Hz in full-step mode, so 6.25 revolutions -- assumes no steps are lost.

#### Rewiring the supplies

Previously the 12 V supply went to a rail on the breadboard and from there to VMOT and GND, while VDD went straight from the MCU's 3.3 V pin. This time nothing goes through the rails: the 12 V leads plug directly into VMOT and the adjacent GND, and 3.3 V plugs directly into VDD. With no 12 V node anywhere on the rails, shorting it to the logic rail is not possible in this case.

A continuity test (unpowered) confirmed that the two GND pins on the A4988 are connected internally. So there were two equivalent ways to establish a common ground: bring both the MCU ground and the 12 V negative to the same `-` rail and jumper across, or connect the 12 V negative to the GND next to VMOT and the MCU ground to the GND next to VDD. I went with the latter. Either way the two domains share a reference, which they must -- the A4988 has to interpret the MCU's 3.3 V STEP and DIR levels against the same 0 V.

The rule of thumb is: **Grounds connected, supplies never connected**

Per the Pololu warning about voltage spikes on carriers with low-ESR ceramic capacitors, there is a 100 µF capacitor across VMOT and GND, placed close to the board rather than out at the supply end. With the capacitor at the far end, the lead wires are still part of the loop and the capacitor does not do its job.

#### On the board I burned

I still cannot reconstruct exactly what happened. What I remember is that the 12 V went in before the MCU was powered, but the wiring was disturbed afterwards, so the state I found might not have been the state it failed in. It may have been the order, or a 12 V lead touching the logic side. What argues against the order alone being the cause is that the same setup had worked repeatedly before.

The general rule: power the MCU first, then the 12 V; on shutdown remove the 12 V first. But it is only a habit, not a safeguard. An unpowered chip is not a safe chip -- the protection diodes on the pins exist to shunt brief static discharges, not to survive sustained overvoltage, and with VDD at 0 V there is no supply to absorb the injected current. The actual safeguard is that 12 V now has no path to the logic side at all.


### 2026-08-15

#### `strcmp` returns 0 on a match

In `CmdProc_Process` I compare the assembled command line against each supported command:

```c
int strcmp(const char *str1, const char *str2);
```

The counter-intuitive part is that it returns **0** when the two strings are identical, not 1. The name is short for "compare", and the return value is really a difference: negative, zero or positive depending on ordering. So `if (strcmp(a, b))` reads as "if they differ".

I wrote `if (strcmp(command, "FEED"))` at first, which inverts every branch. Worse, it still compiles and still looks correct.

#### `static` does not apply to a type definition

I tried to write `static typedef enum {...}` in `Feed.c`, thinking the enum should be hidden from other files.

That does not work, and it is not needed. `static` at file scope gives *internal linkage* to a variable definition or a function -- it hides a symbol from the linker. A type definition does not create a symbol at all; it only tells the compiler how to lay out memory for variables declared with that type later. There is nothing to hide.

Scope already does the job. The two enums in this module are split on purpose:

- `Feed_Source` lives in `Feed.h`, because callers need it: `Feed_Request(FEED_CMD)`.
- `Feed_State` lives in `Feed.c`, because nothing outside needs to know how the module tracks itself. Being defined in the `.c` file is enough -- no other translation unit can see it.

The variable holding the state *is* a definition, so that one does get `static`:

```c
static Feed_State curr_state = FEED_STATE_IDLE;
```

#### Why Feed needs two functions

Per the arbitration diagram there are three states. When idle, a request from any source is accepted; while feeding, only a scheduled request is accepted and it moves the state to Pending.

`Feed_Request()` handles one event: a request arrives, it is accepted or dropped, and on acceptance the motor and the timer start. But that is a one-way path -- idle to feeding to pending. Nothing in that function can bring the state back, because the thing that ends a feed is not a request; it is the timer running out, several seconds later.

So the module needs a second entry point. `Feed_Poll()` is called every pass of the main loop and asks `TIMER_InProg()` whether the current feed is still running. When it is not, that pass is the one where the feed just completed: stop the motor, then either start the pending feed or return to idle.

This split is also why stopping the motor lives in the main loop rather than in the timer ISR.

There are two different ways this firmware can fail, and they behave differently.

**The main loop hangs, but the CPU is fine.** Say it gets stuck in a `while(1)` -- which is exactly what the `CRASH` command does. The CPU keeps fetching and executing, and interrupts keep firing: the timer counts down, NVIC raises the request, the CPU jumps into the ISR, runs it, and returns straight back into the loop it is stuck in. So if the ISR stopped the motor, the feed would end cleanly on schedule and everything would look normal from the outside, while the main loop had in fact been dead for seconds.

Requiring the main loop to stop the motor makes "feed complete" mean something stronger: the code the watchdog supervises is still alive.

**Something worse happens and the CPU ends up in the HardFault handler.** That exception has priority -1, above every peripheral interrupt, and the default handler is itself a `while(1)`. The CPU stays inside a higher-priority exception context, so the timer ISR never gets serviced at all -- the motor would keep running.

This is where the watchdog comes in. It does not care which of the two happened, or what caused it. It only cares that the main loop stopped refreshing it, and resets the system either way.

Which is also why refreshing the watchdog has to be the main loop's job, and only the main loop's: the whole point is that the refresh is evidence the supervised code is still running.

### 2026-08-18 -- First end-to-end run

I wired the two application polls into the main loop:

    while (1) {
        Feed_Poll();
        CmdProc_Process();
    }

and sent `FEED\n` from the host (VS Code's Serial Monitor extension) to check the whole chain: UART interrupt, ring buffer, line assembly, command match, arbitration, response. Below is the log:
```text
---- Opened the serial port COM4 ----
---- Sent utf8 encoded message: "FEED\n" ----
Invalid command
---- Sent utf8 encoded message: "FEED\n" ----
Feeding started
---- Sent utf8 encoded message: "FEEDFEED\n" ----
Invalid command
---- Sent utf8 encoded message: "FEED\n" ----
Feeding started
---- Sent utf8 encoded message: "FEED\n" ----
Busy feeding
```

#### Getting the terminator sent at all

The Serial Monitor extension does not let me type `\n` directly -- typing a backslash sends a literal backslash. Without a terminator, Comms never completes a line, so nothing downstream ever runs. The fix was the **Line ending** dropdown in the extension, set to LF.

#### Why bytes went missing under a breakpoint

Before I found that, I put a breakpoint on the `default` case in `Comms_PollCommand()` to check whether bytes were arriving at all:

    default:
        if (command_line_idx < COMMAND_LINE_MAX){
            command_line[command_line_idx] = (char)byte;
            command_line_idx++;
        } else {
            discarding = true;
        }
        break;

It did hit, which confirmed `UART_CTRL_ReadByte()` was delivering bytes. But the behaviour was consistent across two runs: after sending `FEED`, the `F` and the first `E` made it into `command_line`, and everything after that was **lost**.

The cause is the breakpoint itself. Halting the CPU stops the ISR from running, but it does not stop the USART peripheral -- that keeps receiving in hardware regardless of what the CPU is doing.

`RXNE` is set when a complete byte moves from the shift register into the data register. So: `F` lands in DR, the CPU reads it and then halts. `E` lands in DR and sets `RXNE`, but with the CPU halted nobody reads it, so DR stays occupied. When the next byte finishes arriving in the shift register there is nowhere to put it, and `ORE` is set (RM0390 p.784). The byte already in DR is preserved; the one waiting in the shift register is the one lost, overwritten by whatever arrives next.

So `Step Over (F6)` let me read the `E` sitting in DR, but the remaining `E` and `D` were gone.

Worth noting this was self-inflicted -- I was debugging a problem that only existed because I had not sent a terminator. But the takeaway stands: **do not put breakpoints on the receive path.** Halting the CPU for even a few hundred microseconds is enough to overrun the UART at 115200 baud, where a byte arrives every 87 us. It also confirms the ORE branch in `USART2_IRQHandler` is doing its job -- without it the flag would have stayed set and RXNE would never have fired again.

#### Reading the log

The first `Invalid command` was leftover state: the breakpoint sessions had left bytes in `command_line` with a non-zero index, and I had not reset the chip. The next `FEED\n` was appended to that garbage rather than starting a fresh line.

`FEEDFEED\n` correctly returned `Invalid command`. That is Protocol section 4.5 working as intended: every line is interpreted as one command, so an eight-character line simply matches nothing. There is no such thing as a partially valid line.

The last pair is the one worth having: two `FEED\n` sent a moment apart. The first started a feed, and the second arrived inside the 5-second window and was dropped with `Busy feeding`. That is the centre cell of the arbitration table -- a request from UART or the button is dropped while the motor is busy, and only a scheduled feed is deferred. It also exercises the ring buffer, since the second command's bytes arrive while the first response is still being transmitted.

### 2026-08-19 -- IWDG and the crash commands

With IWDG_CTRL module implemented, I added `IWDG_Refresh()` into the main loop, as the first thing each iteration, per the control flow diagram.

#### Timing

The IWDG runs off the LSI. With the prescaler set to 32, the counter clock is 32 kHz / 32 = 1 kHz, so one count is 1 ms. RLR is set to 999, which gives a timeout of about 1 second.

That number is an upper bound on how long the CPU may go without refreshing, not how often it actually refreshes. In practice the loop comes around far faster than that; the timeout only has to be longer than the slowest legitimate pass.

One thing worth writing down: the LSI is an internal RC oscillator, and the datasheet specifies it at 17-47 kHz (datasheet p.106), not exactly 32. So "1 second" is really somewhere between roughly 0.7 s and 1.9 s. That is fine here, because nothing in the firmware blocks for more than a few milliseconds and the margin is forgiving, but it means the timeout should never be treated as a precise value.

#### Reporting the reset cause

`RCC_CSR` records why the last reset happened -- `PINRSTF` for the reset button, `SFTRSTF` for a software reset, `IWDGRSTF` for the watchdog, and so on. These flags are not cleared by a reset, which is exactly what makes them useful; they have to be cleared explicitly by writing RMVF.

I added `IWDG_WasResetByWatchdog()`, which reads `IWDGRSTF` and returns a bool. For now that is all I need, but a switch/case over the other flags could report the exact cause later.

It is called once in `main`, after init and before the loop:

```c
if (IWDG_WasResetByWatchdog())
{
    Comms_SendResponse("Recovered from crash");
}
```

This felt counter-intuitive at first -- my instinct was that the main loop should poll it, since anything before `while(1)` only runs once and by then the crash has already happened. But that is backwards. The crash does not return to this current session; the watchdog *resets the chip*, so `main` starts over from the top. The flag survives the reset, the check runs on the way back up, and the message goes out. Startup is the only place this check makes sense.

#### CRASH

```c
else if (strcmp(command, "CRASH") == 0){
    /* hangs the CPU deliberately */
    while (1){}
}
```

The loop stops the main loop from refreshing the watchdog. About a second later the chip resets, and the next boot reports it.

#### CRASHFEED

```c
else if (strcmp(command, "CRASHFEED") == 0){
    Feed_Request(FEED_CMD);
    while (1){}
}
```

This one starts a feed and *then* hangs, so the crash happens while the motor is running. It tests something `CRASH` cannot: that a fault during motor motion **still ends with the motor stopped**.

Nothing in my code stops it. `Feed_Poll()` normally does that from the main loop once the timer expires, but the main loop is dead. What stops the motor is the reset itself: TIM2's `CEN` is cleared, the output compare enable goes with it, and PA0 reverts to a floating input, so the STEP waveform disappears. All of that is hardware, at a point where the firmware is no longer executing anything.

The observable result is that the motor runs for roughly a second and stops, instead of the full five. The watchdog timeout is shorter than a feed, so the reset arrives first.

```text
---- Sent utf8 encoded message: "FEED\n" ----
Feeding started
---- Sent utf8 encoded message: "CRASH\n" ----
Recovered from crash
---- Sent utf8 encoded message: "CRASHFEED\n" ----
Recovered from crash
```

#### Noise and heat at idle, and the EN pin

Even following the power-up order -- MCU first, then the 12 V -- the motor hummed continuously while idle and the A4988 felt warm.

The hum is normal for a stepper: both coils stay energised at standstill to hold position, and the driver maintains that current by chopping, which the coils turn into audible noise. But holding current is dissipated the whole time, and a cat feeder is idle for well over 99% of its life.

I had been ignoring the `EN` pin on the A4988, which is active low: driving it high disables the coil drivers entirely. I assigned it to PA8 and added it to MOTOR_CTRL module. In `MOTOR_Start()` I pull EN low *before* starting the PWM, so no STEP edge lands on a disabled driver. In `MOTOR_Stop()` the order is reversed -- stop the PWM first, then disable -- though that direction matters less, since the motion is ending either way.

Known limitation: between power-on and `MOTOR_Init()`, PA8 is a floating input and the A4988's internal pull-down holds EN low, so the coils are briefly energised. An external pull-up would close that window, but it would mean running a 3.3 V rail on the breadboard, which conflicts with the wiring rule that no logic voltage goes on a rail at all. A few milliseconds of holding current is harmless, so I left it.

#### On the heat

With a finger on the A4988's heat sink it felt slightly warm, but I could hold it there almost forever. That is not a measurement, and I do not think my subjective impression is worth much in this case. Adding `EN` should remove the question entirely, since the coils are now only energised during the five seconds of an actual feed.

The open question is the sense resistor. Both the physical board and the Amazon listing photo show `R100`, i.e. 0.1 Ω, and I set `V_ref` on that basis. But Pololu's own documentation states 0.068 Ω for their carriers, so mine being a clone, I cannot be completely sure which value applies. If the real value is 0.05 Ω, the actual current is double what I calculated.

I wanted to measure the coil current directly, but my multimeter came with probe tips rather than alligator clips, and the measurement requires breaking one motor lead to put the meter in series. Pololu is explicit that "connecting or disconnecting a stepper motor while the driver is powered can destroy the driver", and having the meter fall off mid-measurement is exactly that scenario. Left for another day with proper clips: the coil current only flows during a feed now, but I would still rather know the number than assume it.

### 2026-08-20 -- Button

Added the `Button` module. The behaviour copies the commercial feeder I own: the user can hold the button as long as they like, but the request is raised when they let go, not when they press. One press and release is exactly one feed request.

PC13 is active low -- the pin reads 0 while the button is held and 1 once it is released -- so the event to detect is the transition from pressed back to idle.

#### Polled, not interrupt-driven

The button does not use EXTI. Main calls `Button_Poll()` once per pass, in the order given by the control flow diagram.

This keeps the module coherent with the ones already there. CmdProc raises a feed request when command matches `FEED`; Button raises one when it sees a release. Main walks through each source in order but never decides anything itself -- each module applies its own logic and calls `Feed_Request()` on its own behalf.

Polling is also enough on its own terms: a human press lasts at least milliseconds while the loop comes around in microseconds. An interrupt would buy nothing here and would cost an extra interrupt source.

```c
if (was_pressed && !pressed){
    Feed_Request(FEED_BUTTON);
}
```

#### No software debounce

The Nucleo's B1 button is filtered in hardware by an RC network on the board, so debouncing here would be redundant. If this ever moves to a board without that filter, the fix is confined to this module -- nothing else in the system knows or cares how a button press is detected. That containment is the practical payoff of using the layered architecture.

#### The module was written but never initialised

The first time I tested it, nothing happened at all. The code was fine; `Button_Init()` was simply missing from `init_all()`, so GPIOC's clock was never enabled and PC13 was left floating.

Worth noting because of how it presents: a module that is never initialised looks exactly like a module that is broken. Nothing in the build catches it, and reading `Button.c` over and over would never have found it. The check is to confirm the module is actually wired into startup before questioning its logic.

#### Verification

Holding the button for various lengths of time and releasing produced exactly one feed each, starting on release.

I also pressed and released the button while a feed started by the `FEED` command was already running. The motor still ran for about five seconds, not longer -- the button request was dropped, as the arbitration table specifies. Nothing is reported in that case, since the button has no return path to tell anyone.

### 2026-08-22 -- RTC_CTRL

The RTC is the most involved peripheral I have used in this project so far.

#### Two things have to happen before the RTC is even reachable

Its initialization takes more steps than any other peripheral here, because it lives in the backup domain -- a region of the chip that survives a system reset and is write-protected by default, so that stray code cannot corrupt the clock.

Unlocking it means setting the `DBP` bit (Disable backup domain write protection) in the PWR power control register, `PWR_CR`. That bit lives in the PWR peripheral, so the PWR clock has to be enabled first. PWR itself is not protected -- it is the gate that unprotects the backup domain.

The second thing is the clock source. Other peripherals hang off a bus clock, but the RTC has to keep counting while the CPU is asleep, so it needs its own low-power oscillator. This board uses the LSE, an external crystal running at exactly 32.768 kHz.

That is a different trade-off from the IWDG, which runs off the LSI. The LSI is an internal RC oscillator specified at 17-47 kHz, so "one second" is really somewhere between 0.7 and 1.9 seconds. That is fine for a watchdog: the main loop comes around in milliseconds, so the margin is enormous either way. It is not fine for a calendar. At worst the LSI is off by roughly 45%, which over a single day is around eleven hours of drift. Not every Nucleo board has an LSE crystal fitted, but the MB1136 C-04 does.

#### Why RTC_Init returns a bool

Every other Init in this project ignores the `HAL_StatusTypeDef` return value, because those functions only enable a clock and write configuration registers -- they hardly fail.

The RTC is different: `HAL_RCC_OscConfig()` waits on a physical crystal to start oscillating, and that genuinely can time out. So `RTC_Init()` returns `bool` and gives up early if the LSE does not come up.

Without that check, `HAL_RTC_Init()` would still run and everything would look fine, but the RTC would have no clock and the calendar would never advance. The failure would be silent: I would only find out by noticing a scheduled feed never happened, and then I would not know whether the problem was in CmdProc setting the time wrong or in the RTC not running at all.
```c
if (!RTC_Init())
{
    Comms_SendResponse("RTC clock failed to initialize");
}
```
Reporting it over UART is enough for now. A real product would need something better, since the host is not always connected.

#### `RTC_IsTimeSet` and the year field

`bool RTC_IsTimeSet(void)` reads the `INITS` flag in `RTC_ISR`. What is worth knowing is how the hardware decides: RM0390 says the flag is set when the calendar **year field is different from 0** -- 0 being the backup domain reset value.

So even though this project has no use for the year -- it feeds the cat at fixed times of day and never cares what day it is -- the year still has to be set to something non-zero. Leaving it at 0 would make `RTC_IsTimeSet()` return false forever, and the whole "has the clock been set" branch in the protocol would never work.

#### `RTC_SetTime` and what actually had garbage in it

`bool RTC_SetTime(uint8_t hour, uint8_t minute)` guards the input first (`hour > 23` or `minute > 59` is rejected), then fills an `RTC_DateTypeDef` and an `RTC_TimeTypeDef` and hands them to `HAL_RTC_SetDate()` and `HAL_RTC_SetTime()`. Both return values are checked; either one failing fails the whole call.
```c
RTC_DateTypeDef date = {
    /* WeekDay, Month and Date are irrelevant here; any valid value will do */
    .WeekDay = RTC_WEEKDAY_MONDAY,
    .Month   = RTC_MONTH_JANUARY,
    .Date    = 1,
    .Year    = RTC_DEFAULT_YEAR   /* must be non-zero: INITS keys off the year field */
};

// ......

RTC_TimeTypeDef time = {
    .Hours   = hour,
    .Minutes = minute,
    .Seconds = 0,
};
```

When I later inspected the structs in the debugger, some fields held garbage, and I initially blamed the local structs in `RTC_SetTime()`. That was wrong from two perspectives.

First, **a partially written initializer list is not the same as no initializer at all**. If an object has any initializer, C zero-initializes every member that is not listed -- automatic storage duration included. So `date` and `time` above have no garbage; `.SubSeconds`, `.DayLightSaving` and the rest are all 0.

The garbage was in the read-back structs I put in `init_all()` for testing, which had no initializer at all:
```c
    RTC_TimeTypeDef t;
    RTC_DateTypeDef d;
```
Those really are whatever was left on the stack. And `DayLightSaving` and `StoreOperation` stayed garbage even after the call, because `HAL_RTC_GetTime()` never writes them -- reading the HAL source, it only fills SubSeconds, SecondFraction, Hours, Minutes, Seconds and TimeFormat. A HAL "Get" function does not necessarily populate the whole struct.

I did briefly consider adding `static` to the locals in `RTC_SetTime()` to get the zeroing. That does not work, for a reason I had already written down in the note on `static` from a week ago: **a static-storage-duration object needs a compile-time constant initializer**, and `.Hours = hour` is a function parameter. It would not compile. And if it somehow did, `static` would mean the initializer runs only once, so every call after the first would silently reuse the original time.

#### Testing
```c
if (!RTC_IsTimeSet())
{
    Comms_SendResponse("Time not set");
}

RTC_SetTime(14, 30);
```

The check comes before the set, so on a genuinely fresh backup domain the host should see the message exactly once. It did:

```text
---- Opened the serial port COM4 ----
Time not set
```

Then I used "reset the chip and restart debug session" in CubeIDE, and the reset button on the board. Neither produced the message again, which is the point: the calendar survives a system reset. That is the foundation the whole scheduling design rests on.

Worth being precise about what this does *not* prove. A reset is not a power cycle. VBAT is tied to VDD on this board, so pulling the USB cable does drop the backup domain, and "Time not set" should come back. Those are the two branches the protocol distinguishes, and I have only verified one of them so far.

#### Reading the calendar back
```c
RTC_TimeTypeDef t;
RTC_DateTypeDef d;
HAL_RTC_GetTime(&RTC_Handle, &t, RTC_FORMAT_BIN);
HAL_RTC_GetDate(&RTC_Handle, &d, RTC_FORMAT_BIN);
```

The HAL documents that the second call is not optional:

```text
You must call HAL_RTC_GetDate() after HAL_RTC_GetTime() to unlock the values in the higher-order calendar shadow registers... 
Reading RTC current time locks the values in calendar shadow registers until current date is read.
```

So even with no interest in the date, it has to be read, or the shadow registers stay latched and the time stops appearing to change.

![Expressions view in STM32CubeIDE: Hours 14, Minutes 30, all date fields 1](<RTC time set.png>)

#### Two things I ran into while testing

**Locals do not survive the function.** The four lines above went into `init_all()`, which runs once. I could only see `t` and `d` by putting a breakpoint inside `init_all()` after those lines and before it returns. Putting a breakpoint anywhere in the `while(1)` loop gave "No symbol d in current context", because its stack frame is destroyed once retured from this function.

**The watchdog was starting too early.** The first time I put a breakpoint on the `HAL_RTC_GetDate()` line, CubeIDE reported `Breakpoint installation failed: Connection is shut down`, which I had never seen before.

I do have "Suspend watchdog counters while halted" enabled in the debug configuration, so the immediate cause is still not certain, but looking at `init_all()` I found a real ordering problem regardless. I had called `IWDG_Init()` first, mirroring the control flow diagram -- but that diagram describes the order of the *main loop*, where refreshing the watchdog goes first. **Initialization is not the same as main loop.**

`HAL_IWDG_Init()` starts the watchdog immediately, and nothing refreshes it until the main loop begins. **Everything between those two points has to finish within the timeout**, and `RTC_Init()` sits right in the middle of it, blocking while it waits for the crystal.

**So the watchdog should be initialized *last*: it guards the system, and the system should be ready before it is guarded.** I moved `IWDG_Init()` to the last line of `init_all()`.

### 2026-08-23 -- RTC_CTRL continued

Today I implemented `RTC_SetAlarm` and `RTC_TakeAlarm`.

#### Why the caller picks the slot

`RTC_SetAlarm` takes `RTC_AlarmSlot slot, uint8_t hour, uint8_t minute`, so whoever calls it has to say which of the two alarms it is setting. Seconds are not exposed; they are hard-coded to 0.

The alternative would be to hide the idea of a slot entirely and keep only the two most recent times: set 08:00, 13:00, 18:00 and the firmware silently drops 08:00. That is doable, but it has a problem I did not see at first. Suppose the two times are 08:00 and 18:00 and I want to change 18:00 to 19:00. I send 19:00, and the firmware replaces whichever slot was written longest ago -- which is 08:00. I end up with 18:00 and 19:00. There is no way to edit one time without re-entering both.

I stayed with what the protocol already specifies. The host also arguably *should* know that there are exactly two.

#### How it works

I fill an `RTC_AlarmTypeDef` -- hour, minute, second, the mask settings, and **which alarm it is** -- and hand it to `HAL_RTC_SetAlarm(&RTC_Handle, &alarm, RTC_FORMAT_BIN)`. The HAL takes care of the sequence around it: unlocking write protection, clearing the old flag, waiting on `ALRAWF`, writing `ALRMAR`, re-enabling.

The mask is the part that matters. `RTC_ALARMMASK_DATEWEEKDAY` sets MSK4, which makes the date field "don't care" while hours, minutes and seconds still have to match. That is what turns a one-shot alarm into one that repeats at the same time every day, with nothing to re-arm.

After that the CPU is out of the loop entirely. Hardware compares the calendar against the alarm register every second, and sets `ALRAF` or `ALRBF` in `RTC_ISR` when "the time/date registers (RTC_TR and RTC_DR) match the Alarm A/B register" (RM0390 p.665).

Worth noting that the flags are set by hardware regardless of whether the alarm interrupt is enabled -- the same relationship as `RXNE` and `RXNEIE` on the UART where hardware raises the flag, but whether or not the CPU can hear it and handle it in ISR depends on NVIC. That is why polling works here and I never configured the NVIC for the RTC at all.

#### Why clearing both flags at once is intentional

`RTC_TakeAlarm` checks `ALRAF` and `ALRBF`, and if **either** is set it clears **both** and returns true. In most other peripherals, clearing several flags in one go would be a sign that something is wrong. Here it is the point.

The question to ask is not how many flags were cleared, but whether each flag needs its own separate response. On the UART, every `RXNE` is a different byte, so merging them loses data and is always a bug. Here both flags mean the same thing -- "it is time to feed" -- and the action is identical. Merging them is deduplication, not data loss.

Two cases make that solid.

**Running normally.** Since seconds are fixed at 0, any two alarms the user sets are at least a minute apart, and a feed only takes 5 seconds, so one can never hide the other. Even if the user deliberately sets both alarms to the same time, both flags come up together and the firmware treats it as one feed -- which is correct. A serving is the unit for changing how much comes out; feeding twice at once is not. If the cat needs more, the answer is a longer dispense, not a duplicate alarm.

One side effect of the one-minute floor: an alarm can never collide with another alarm. So the `FEEDING + pending` state in the Feed FSM can only ever be reached by an alarm arriving during a *manual* feed. Setting two alarms close together will not exercise that path -- testing it needs a `FEED` command or a button press first.

**At startup.** This is where the merge really pays off. Per the protocol, however many scheduled feeds were missed while the firmware was not running, exactly one gets made up. In this project the maximum missed is two, since there are only two alarms. A single `bool` with both flags cleared expresses that policy directly, without any counting logic.

#### Calling RTC_TakeAlarm() at the end of RTC_SetTime()

I added a `RTC_TakeAlarm()` call between the last status check and `return true`. The reason took a while to see.

The calendar starts counting as soon as the clock source is running and `RTCEN` is set, well before anything sets the time. `RTC_TR` and `RTC_DR` hold whatever they hold -- all zeros on a fresh backup domain -- and the hardware increments them once per second:

    32768 Hz / ((127 + 1) * (255 + 1)) = 1 Hz

The hardware doesn't know or care whether those numbers mean anything. It just counts, and compares against the alarm register. "Setting the time" is nothing more than writing `RTC_TR` and `RTC_DR`; whether the value is real is purely a software-level idea, and `INITS` is the hint the hardware gives us to decide (it is set when the year field is non-zero).

So what if: the board is powered on, the user sets an alarm for 08:00, but never sets the time. The calendar walks from 00:00:00, and eight hours later it reaches 08:00:00 and `ALRAF` goes up.

No feed happens -- `Schedule` checks `IsTimeSet()` before raising `Feed_Request(FEED_RTC)`, and that guard holds. But the flag is now set and nothing has cleared it.

The problem is the moment the user *does* set the time. `INITS` becomes 1, the guard lifts, and on the next pass `RTC_TakeAlarm()` reads that stale flag and feeds -- executing an 08:00 feed at whatever time it happens to be.

Clearing the flags inside `RTC_SetTime()` fixes it, and the general rule is better than the specific case: **setting the clock rewrites the timeline, so every comparison result from the old one is void.**

This stays at the driver level. The stale flag is not the application's problem to know about; it is the to RTC's own knowledge that a time change invalidates previous matches.

### 2026-08-24 -- TIME and SCHED

Building on `RTC_CTRL`, today was about supporting the `TIME` and `SCHED` commands in `CmdProc`.

#### String parsing

Parsing strings in C is the part I find most tedious. Everything is a pointer, and the bounds are mine to check -- nothing stops me from reading past the end of the buffer.

What makes it manageable here is that the protocol fixes the format. `TIME hh:mm` and `SCHED A hh:mm` have every character at a known offset, so parsing is a matter of checking a prefix, checking the delimiters, and reading digits at fixed indices. No tokenising, no variable-length fields.

#### strcmp for whole commands, strncmp for prefixes

Commands with no parameters -- `FEED`, `PING` -- are the whole line, so `strcmp` works: it compares until a null terminator and returns **0** on a match. The zero is the part I got wrong the first time. I had assumed a match would give a non-zero "true", and wrote `if (strcmp(command, "FEED"))`, **which inverts every branch and still compiles cleanly.**

Commands with parameters cannot be compared whole, since the tail varies. `strncmp(a, b, n)` compares only the first `n` characters, which is what prefix matching needs. Including the trailing space in the pattern matters: `strncmp(command, "TIME ", 5)` will not match `TIMEX`, whereas comparing only four characters would.

#### Three kinds of failure, and which ones the host gets told about

Implementing these two commands forced changes to the protocol I wrote at the start of the project. I take that as a good sign rather than a bad one -- requirements shift as a system gets built, and a document that never changes usually means nobody is reading it.

Until now every command either returned its own success message or fell through to a single `"Invalid command"`. With parameters there are three distinct failures:

**1. The format is wrong.** 
`TIME 1a:30`, a missing space, the wrong length. The parser in CmdProc catches this, and `"Invalid command"` is the right answer -- the line genuinely is not a well-formed command.

**2. The format is fine but the values are not.** 
An hour above 23, a minute above 59. `RTC_CTRL` rejects these. Calling that "Invalid command" would be misleading, because the command *was* valid -- so I added `"Invalid time"`.

That distinction is the point. The two messages lead the user to do different things: one means go check the syntax, the other means the syntax is fine, change the number. This is the same reasoning behind the error codes in PMBus and SMBus, which I have been working with on the DC tester at work. That spec defines a long list of error codes precisely so that a failure tells you *what* failed rather than just *that* something did. It matters more there than here, since that tool is operated by technicians who are not necessarily the people who wrote its firmware.

**3. Everything is valid but the HAL call fails.** 
`RTC_SetTime` and `RTC_SetAlarm` also return false when the underlying HAL call returns `HAL_ERROR`, `HAL_BUSY` or `HAL_TIMEOUT`. This one gets folded into "Invalid time".

I could distinguish it -- the drivers would return a status enum instead of a bool, and CmdProc would map it to a third message. I chose not to, because there is nothing the user could do with it. Once the LSE is running, a HAL timeout on the RTC means the peripheral or the crystal is broken. A user can fix a malformed command or an out-of-range hour; they cannot fix an oscillator. The honest response would be "the device is faulty", and that is a warranty call, not a retry.

A real product would still want a distinct fault indication somewhere -- a status LED, a log, something the field technician sees. But collapsing it into the user-facing message loses nothing the user could act on.

#### TIME?

Testing the set commands exposed a gap: I had no way to read the clock back. I could confirm that `TIME 14:30` returned `"Time set"`, but not that the calendar actually held 14:30 -- and that is exactly the assertion a host-side test script would need to make.

So I added `TIME?`. **The idea of using a trailing question mark for a query comes from SCPI**, which is the protocol I will be using between host and MCU on the DC tester. Borrowing just that convention keeps the two commands distinct in the text itself, rather than relying on the presence or absence of an argument, and it extends naturally if I ever want `SCHED A?`.

#### Testing
```text
---- Opened the serial port COM4 ----
---- Sent utf8 encoded message: "CRASH\n" ----
Recovered from crash
---- Sent utf8 encoded message: "TIME 14:30\n" ----
Time set
---- Sent utf8 encoded message: "TIME?\n" ----
14:30
---- Sent utf8 encoded message: "SCHED A 14:32\n" ----
Alarm A set
Feed complete
```
`CRASH` was a regression check. I had moved `IWDG_Init()` from the start of `init_all()` to the end, and while that should not affect the reset-cause reporting, it was cheap to confirm.

#### Making up a missed feed

I set Alarm A to 14:36, then held the reset button down from 14:35 until past 14:36, timing it on my phone. On release, a feed ran.

What I want to be precise about is *why* it ran, because I initially described this to myself as `Schedule_Poll()` "checking whether a feed was missed" -- and there is no such check anywhere in the code. `Schedule_Poll()` does exactly the same two things on every pass: is the clock set, and has an alarm fired.

It works because `ALRAF` is a level, not a pulse. The hardware set it while the CPU was held in reset, nothing cleared it, and the first pass of the main loop after release read it like any other. The make-up feed needs no dedicated startup path at all -- it falls out of the flag being sticky and living in the backup domain.

It also only works for a reset. A power cycle drops the backup domain, `INITS` goes to zero, and the guard in `Schedule_Poll()` correctly refuses to feed on a clock it no longer trusts.

#### On seconds

I had decided early on that seconds do not matter for this project, and that is still true for *setting* the time -- the user should not have to type them, and 14:30 meaning 14:30:00 is the right behaviour.

Reading them back turned out to be a different question. I was timing a reset against my phone to test the missed-feed path, which is both awkward and imprecise, and the reason was that `TIME?` only reported hours and minutes. So I extended it to return `hh:mm:ss`.

#### Power cycle

Pulling the USB cable and reconnecting brought back `Time not set` on the next `TIME?`, as expected. VBAT is tied to VDD on this board, so dropping VDD drops the backup domain: the calendar resets, the year field goes back to 0, and `INITS` reads 0 again.

That completes both branches of the protocol's Section 5.3. A reset preserves the backup domain, so the clock survives and a missed alarm is made up. A power cycle does not, so scheduled feeding suspends itself until `TIME` is sent again rather than acting on a clock it has no reason to trust.

</details>

---

### 2026-09-16 -- FreeRTOS environment setup

FreeRTOS needs SysTick, so `HAL_InitTick` is overridden with ST's TIM template `stm32f4xx_hal_timebase_tim_template.c`, located in `STM32Cube_FW_F4_V1.28.3/Drivers/STM32F4xx_HAL_Driver/Src`. The template uses TIM6, which is already used elsewhere in this project, so I changed it to TIM7.

### 2026-09-17 -- FreeRTOS environment setup continued

The point of porting this project to FreeRTOS is not that it needs one. It is that I want to learn FreeRTOS, and doing it on firmware I already know means I can compare the two versions directly instead of starting from a blank project.

#### What carries over

All the init calls from v1 are reused unchanged. They only configure peripherals on this micro, and the peripherals do not care how I drive them afterwards.

#### Why an RTOS would be needed

My rough understanding is that an RTOS starts to pay off when a project has far more going on than this one does. All v1 has to do is feed the watchdog, arbitrate feed requests from three sources, and turn the motor on and off. But if there were a hundred things happening at once, a single main loop would get messy fast — everything would have to be written so it never blocks, and every one of those hundred would have to keep that promise.

#### How main changes

In the superloop version the main loop runs over and over, calling each module in sequence. Interrupts can still be used; it just means I have to handle race conditions carefully where an ISR and the main loop touch the same data (or in the ring buffer case in v1 -> get rid of the `size` variable that both the ISR and the main loop can write to).

With FreeRTOS, main's job is different: create the tasks, then call `vTaskStartScheduler()` and **hand over the CPU**. The scheduler runs whichever ready task has the highest priority, and **main never gets control back.**

#### Task states

A task is in one of three states: `Running`, `Ready`, or `Blocked`.

`vTaskDelay(500)` in `blink_task` puts that task into `Blocked` for 500 ticks. Unlike `HAL_Delay(500)`, it does not tie up the CPU. "Blocked" here means the task is stalled, and while it is, the scheduler runs whichever other task is ready with the highest priority.

#### Priority numbers run the other way

Unlike NVIC priorities, where a smaller number means higher priority (`HardFault` sits at -1, above every peripheral interrupt), FreeRTOS task priorities go the opposite direction: a larger number is higher, which feels more natural and intuitive actually. Priority 0 is the lowest, which is where the idle task runs. I guessed this the wrong way round at first, so it is worth writing down — both conventions exist in the same project now.

#### Stacks

Another difference is that the superloop has no concept of tasks at all. It is a regular program of the kind I have been writing since my first programming class: **everything shares one stack, and the flow is whatever the control flow says it is.**

**Under FreeRTOS each task gets its own stack.** The blink task gets 128 words, which is 128 × 4 = 512 bytes, because that is what I passed to `xTaskCreate` — the same number `configMINIMAL_STACK_SIZE` uses for the idle and timer tasks.

That raises two problems I can already imagine.

**One: running out of memory as tasks are added.** The RAM is the same as before, and every new task takes another stack out of it. Eventually the stacks will not fit, unless each one is trimmed to what it actually needs — and even on a micro with more RAM, enough tasks would still exhaust it.

**Two: a task that needs more than its stack.** If I assume 512 bytes and the task actually uses more, it will run off the end and into whatever is next to it in memory. FreeRTOS provides `vApplicationStackOverflowHook` for this: the kernel detects the overflow and calls it, leaving what to do up to the application. All I do here is disable interrupts so the scheduler cannot switch away, and **trap the CPU** in a `while (1)` — **preserve the scene** so I can look at `pcTaskName` and see which task it was.

```c
/* Callback FreeRTOS calls when it detects a stack overflow */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    /* Break here and read pcTaskName to see which task overflowed. */
    taskDISABLE_INTERRUPTS();
    while (1) { }
}
```

#### A separate failure: the task never gets created

The hook above catches problem two. Problem one shows up somewhere else entirely, and much earlier.

`xTaskCreate` allocates the stack and the task control block out of the FreeRTOS heap. If there is not enough left, it returns `errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY` instead of `pdPASS`, and **the task simply does not exist**. Nothing crashes; that task just never runs. So the return value is worth checking, otherwise the symptom is "a task that mysteriously does nothing".

```c
if (xTaskCreate(blink_task, "blink", 128, NULL, 1, NULL) != pdPASS)
{
    while (1) { }
}
```

Two different problems, then: the stack overflow hook is about a task exceeding the stack it was given, and this check is about the stack never being handed out in the first place.

### 2026-09-19 -- Port the UART path to FreeRTOS tasks and queues

Today I ported the communication modules to FreeRTOS.

#### The ring buffer becomes a queue

The first change is that `rx_queue` (the ring buffer in v1) is now a FreeRTOS queue, written by `xQueueSendFromISR` in the UART ISR and read by `xQueueReceive` inside `Comms_Task`.

Both of those are just functions — one called from an interrupt, one called from a task. The task is `Comms_Task`, and it is the thing that never returns: it is always Running, Ready or Blocked.

The third argument to `xQueueReceive` is `portMAX_DELAY`, meaning there is no timeout. If the queue is empty the calling task waits indefinitely.

#### Does this actually improve anything

Yes, though not enough to matter in this project, since it only has a handful of tasks. But the idea stands.

In v1, every pass of the main loop calls `Comms_PollCommand()`, which drains the ring buffer and returns `NULL` when there is nothing there. Those cycles are wasted. It does not affect this project, where there is far more CPU resources than it needs, but the waste is real.

Under FreeRTOS, a task blocked on `xQueueReceive` is taken off the ready list entirely. The scheduler does not consider it at all, so the CPU goes to whoever else is runnable instead of constantly coming back to check.

#### How does the ISR know which task to wake

This is the part I had to think through. It is not that `Comms` talks to the ISR directly — that would defeat the layering.

In v1 I had a `ring_buffer` struct with `front`, `rear` and a 33-element array. A FreeRTOS queue has all of that plus one more thing: **a list of who is waiting on it**. That list belongs to the queue, not to either function.

So the sequence is:

1. `Comms_Task` calls `xQueueReceive` and the queue is empty
2. Inside that call, FreeRTOS puts `Comms_Task` on the queue's waiting list and takes it off the ready list
3. `Comms_Task` stops there, with its stack frame and locals untouched
4. A byte arrives; the ISR calls `xQueueSendFromISR`
5. Inside *that* call, FreeRTOS stores the byte, **checks the queue's waiting list, finds `Comms_Task` on it, and moves it back to the ready list**
6. The scheduler picks it up, and `xQueueReceive` returns from where it stopped

The notifying is **done by whoever makes the condition true**. The queue is the middle ground where the two sides register their interest — **neither side needs to know the other exists.**

---

### Two ways a task can run out of stack

As mentioned in notes from `2026-09-17`, I had already pictured two failure cases: the RAM running out as more tasks are added, and an individual task being given less stack than it actually needs at runtime.

I assumed that checking the return value of `xTaskCreate` would cover the second one. It only covers the first.

`xTaskCreate` does one thing: it tries to carve the requested number of words out of the FreeRTOS heap. Either there is room or there is not. It has no way of knowing how much stack the task will actually use once it starts running — that depends on the call depth and the locals, none of which exist yet.

So I could create a task with 128 words while there is plenty of heap left, and still have that task need 1280 words at runtime. It would run off the end of its own stack and into whatever is next to it in memory.

#### Measuring instead of guessing

`uxTaskGetStackHighWaterMark(NULL)` returns how many words are left in the calling task's stack, measured at its deepest point so far.

I added one for each task and tried different (both valid and invalid) commands: `PING`, `HELLO`, `TIME?`, `TIME 14:30`, `TIME 1a:30`, `TIME 99:99`, `SCHED A 08:00`, `SCHED B 18:00`, `SCHED X 08:00`, `SCHEDULEAVERYLONGCOMMANDLINE`, and `FEED` twice. Then I suspended the debugger and read the two values.

Both tasks were created with 256 words:

- `Comms_Task` had **205** left, so it peaked at **51 words**
- `CmdProc_Task` had **201** left, so it peaked at **55 words**

So 128 words is safe for both, with more than double the headroom. Worth noting this was measured on the Debug build at `-O0`; `-Os` should use less, so this is the conservative side.

I most likely do not need to save RAM in this project. But I can picture one with enough tasks that knowing the real number, rather than guessing generously, is what makes everything fit.

### 2026-09-20 -- Port Feed to FreeRTOS

#### What changes when the sources become tasks

The recurring difference between v1 and v2 is that a sequence becomes a set of tasks.

In the superloop, ordering is guaranteed by the control flow of the main loop. Without interrupts, everything happens exactly in the order I wrote it. With interrupts, the races that appear can be handled either by designing the shared write away -- which is what dropping the `size` variable from the ring buffer did -- or by masking interrupts around a critical section.

Now each feed source is its own task. **All three can request a feed at any time, and arbitrating between them is Feed's job**, which is one of the things this project is built around. **But what if a source requests a feed, and between Feed starting the motor and updating its state, another request arrives?** The scheduler runs whichever ready task has the highest priority at that moment, and it can switch between any two instructions.

#### Masking interrupts is not the same as a mutex

I confused these at first.

`taskENTER_CRITICAL()` masks all interrupts, like `__disable_irq()`. Nothing at all can interfere -- no task, no ISR.

A mutex is narrower. **It only blocks tasks that want the same mutex**. Interrupts keep running, and unrelated tasks keep running. The analogy I had was three of us wanting to go through a door only one person fits through: any of us can go first, but the others have to wait until whoever went is completely through.

The cost of that narrowness is that a mutex can only be used between tasks. An ISR cannot take one, because taking might block and an ISR has no task context to suspend.

#### What the mutex actually covers

This is the part I had wrong. The mutex does not stay held for the duration of a feed. **It covers only the few lines that read and write `curr_state` -- microseconds**. During the five seconds the motor is actually running, nobody holds it.

That matters for the behaviour: if it were held the whole time, a `FEED` sent during a feed would sit blocked in `xSemaphoreTake` for five seconds instead of coming straight back with `Busy feeding`. The arbitration would stop working.

The rule is to **hold a mutex for as little as possible -- just long enough that the read, the decision and the write cannot be split.**

#### The semaphore is a separate thing

`Feed` still needs to know when a feed has started, so it can time it. That is a signal, not a lock, and it is the same problem as the ISR telling `Comms_Task` a byte has arrived, from yesterday's notes.

- `state_mutex` protects `curr_state`. Taken and given by the same task, around a short block.
- `feed_started` tells `Feed_Task` to start counting. Given by whichever source accepted the request, taken by `Feed_Task`.

The way to tell them apart in the code is who calls what: if the same task takes and gives, it is a lock; if one task gives and another takes, it is a signal.

#### The UART needs a mutex too

`Feed` and `CmdProc` are both tasks, and both send over UART. Feed sends `"Feed complete"` and `"Deferred feed started"`; CmdProc sends `"Busy feeding"`, `"Feeding started"` and so on.

Two tasks, one shared resource -- `HAL_UART_Transmit` writes the data register one byte at a time, so without a mutex the output can interleave:

Feed wants to send `"Feed complete"` while CmdProc wants `"System ready"`, and what comes out could be `"Feed cSystomplem readyte"`.

This is not something v1 could run into, because only the main loop ever sent anything. It is a cost that arrives with the second task that talks, not with Feed specifically.

#### SysTick, the HAL tick, and which timer ended up where

**`SysTick` is a timer in the ARM core itself.** Every Cortex-M has one, regardless of who made the chip. **`TIM6` and `TIM7` are ST peripherals**; another vendor's Cortex-M would not have them.

In v1, `HAL_Init()` configures SysTick for a 1 ms interrupt, and the `SysTick_Handler` in `main.c` calls `HAL_IncTick()`. That counter is what every HAL timeout is measured against -- `HAL_TIMEOUT` comes from comparing the current tick against the tick when the operation started. Code has no way of knowing how much real time has passed without a hardware timer behind it.

In v2, FreeRTOS wants SysTick for its own scheduler tick. So the HAL tick has to move, and ST foresaw this scenario: `HAL_InitTick()` is declared `__weak`, so overriding it is enough to put the HAL tick on TIM7 instead.

Counting what each version actually uses:

| | v1 | v2 |
|---|---|---|
| SysTick | HAL tick | FreeRTOS tick |
| TIM7 | — | HAL tick |
| TIM6 | timing a feed | — |
| TIM2 | motor PWM | motor PWM |

The same three timers either way. Deleting the TIMER module did not free a peripheral; it paid back what FreeRTOS took. What it did buy is that timing a feed is no longer a module at all -- three files, an ISR and two volatile variables became one `vTaskDelay`.

### 2026-09-20 -- Port Button and Schedule to tasks

Porting Button and Schedule to FreeRTOS. Nothing dramatic: define `Button_Task` and `Schedule_Task` in their own files, and call `xTaskCreate` for each in main, and scheduler takes care of the rest.

#### RAM, stack, and the FreeRTOS heap

I changed `configTOTAL_HEAP_SIZE` from 4096 to 8192 in `FreeRTOSConfig.h`, and this is a good place to break down three terms I kept mixing up.

**RAM** is the MCU's memory, the counterpart to Flash. On this STM32F446RE it is 128 KB.

**The stack** is one region inside RAM. `main()` and every ISR use it, whether or not FreeRTOS is used.

**The FreeRTOS heap** is another region inside RAM — a single array that FreeRTOS carves task stacks, queues and mutexes out of. `configTOTAL_HEAP_SIZE` is its size.

The reason this matters: FreeRTOS does not get all of RAM. The heap sits alongside the main stack and global data, all of it in the same 128 KB. I am making the heap bigger because there are more tasks to create now, and there is plenty of RAM to spare — the two 128-word tasks plus the three 256-word ones still leave most of it free.

#### How often should a task wake

The question that comes with every polling task is **how often it should run.** A task never returns; it is only ever `Running`, `Ready` or `Blocked`. So the pattern is: **do a little work, then sleep, which hands the CPU back.**

For the button: a person pressing and releasing takes tens to hundreds of milliseconds, never microseconds. So sampling every 20 ms is fast enough that no press slips between two samples. `Button_Task` checks the pin once, then sleeps with `vTaskDelay(pdMS_TO_TICKS(BUTTON_POLL_MS))`, where `BUTTON_POLL_MS` is 20.

For the schedule: the calendar only increments once a second, so checking once a second finds everything there is to find. The check itself is a few instructions — microseconds — and then the task sleeps for a second.

The key point about that sleep: `vTaskDelay` puts the task into the `Blocked` state, but it does not block the CPU. While this task sleeps, the scheduler runs whoever else is ready. **The work takes microseconds; the sleep gives the remaining time to everyone else.**

### 2026-09-23 
Verified all five tasks on hardware: FEED stops after five seconds, a second FEED during a feed returns Busy feeding immediately, the button starts a feed, and Alarm A fired on time. The defer path is still untested. Below is Serial Monitor output:

```text 
---- Opened the serial port COM4 ----
---- Sent utf8 encoded message: "PING\n" ----
System ready
---- Sent utf8 encoded message: "FEED\n" ----
Feeding started
Feed complete
---- Sent utf8 encoded message: "FEED\n" ----
Feeding started
---- Sent utf8 encoded message: "FEED\n" ----
Busy feeding
Feed complete
Feed complete
---- Sent utf8 encoded message: "TIME 16:05\n" ----
Time set
---- Sent utf8 encoded message: "SCHED A 16:06\n" ----
Alarm A set
---- Sent utf8 encoded message: "TIME?\n" ----
16:05:14
---- Sent utf8 encoded message: "TIME?\n" ----
16:05:41
---- Sent utf8 encoded message: "TIME?\n" ----
16:05:50
---- Sent utf8 encoded message: "TIME?\n" ----
16:05:59
---- Sent utf8 encoded message: "FEED\n" ----
Busy feeding
Feed complete
```

### 2026-09-24 -- Port the watchdog to FreeRTOS

In the superloop version, the main loop kicks the dog once per iteration. If a second goes by without a kick, the CPU must be stuck in some module — each takes microseconds, and all of them together only milliseconds at max.

In the FreeRTOS version those modules are separate tasks, and I want the watchdog to behave the same from the outside: same black box, different implementation.

#### Letting every task kick the dog does not work

**There is exactly one IWDG, so a kick from one task hides a missing kick from another.** Task A kicks, task B is stuck and does not, task C kicks — the dog is kicked and B's failure goes unnoticed. And I no longer control the order tasks run in; **the scheduler does.**

#### The standard approach, and why it does not fit

The textbook fix is for each task to report to a watchdog task, **which only kicks the dog once every task has reported.** A task that stops reporting means no kick, and the chip resets.

The catch is that this assumes every task wakes regularly. Two of mine do: `Button` every 20 ms, `Schedule` every second. But `Comms`, `CmdProc` and `Feed` block with `portMAX_DELAY` — they sleep until something happens, possibly for hours. **Making them report would mean adding a timeout to every one of those blocking calls, just so they wake up to check in.**

#### Taking the complement

**Instead of asking every task to report, check whether the CPU is ever free.**

This project has a handful of tasks, each doing microseconds of work at a time, so well over 99% of the time none of them is running — the **idle** task is. So I kick the dog from idle. It gets kicked far more often than needed, but it does exactly the job: **if idle never gets to run, some task is stuck spinning, and the watchdog resets the chip.**

> **NOTE:** If a task blocks forever: say it waits on a mutex that never gets given back, it stays asleep. It is not spinning in a while loop, so it does not trap the CPU: **blocked tasks give up the CPU so other tasks can run.** Idle still runs, and the dog still gets kicked. **The system looks healthy, but that task is practically dead.**

#### The idle hook

The idle task is created by FreeRTOS itself. Inside its `while (1)` it calls `vApplicationIdleHook()` when `configUSE_IDLE_HOOK` is 1 in `FreeRTOSConfig.h`. I set it and implemented the hook in `main.c`:

```c
void vApplicationIdleHook(void)
{
    IWDG_Refresh();
}
```

#### Verification

A breakpoint on `IWDG_Refresh()` hit almost immediately after the scheduler started. Then:

```text
---- Sent utf8 encoded message: "CRASH\n" ----
Recovered from crash
---- Sent utf8 encoded message: "CRASHFEED\n" ----
Recovered from crash
```

For `CRASHFEED`, there is no `Feed complete` before the recovery message, so the reset landed before the five-second feed finished. Both commands work unchanged from v1.

### 2026-09-24 -- Adding CI

Today was about setting up Continuous Integration with GitHub Actions.

#### Connecting it back to CSE 29

When I took CSE 29 at UCSD last fall, I learned C in a Linux environment. For each file I ran something like `gcc -c main.c -o main.o`, and sometimes without `-o` and let it pick a default name. That is the **compiler turning a C source file into an object file.**

As the class went on, assignments had several source files. Did I really want to compile each one by hand? Of course not. The answer is a makefile: **a recipe that compiles every source file into its object file, and only recompiles what changed.** That is also why I always had to add header directories under `Includes` and source directories under `Source Location` in CubeIDE's `Paths and Symbols` — those settings are what end up in the makefile CubeIDE generates.

An object file is machine code (binary), but it is not runnable yet; references between files are still unresolved. The **linker** joins all of them into one image and produces the `.elf` (Executable and Linkable Format) and a `.map` describing where everything landed.

After that class I moved on to microcontrollers, using STM32CubeIDE, NuEclipse, and VS Code. I never connected it back until now. Whatever the tool, underneath it is the same thing: **a compiler, a makefile telling it what to build, and a linker producing the `.elf` that gets flashed.** The only real difference is that for a microcontroller the compiler is a cross compiler. `arm-none-eabi-gcc` runs on my PC but produces code for the ARM chip, so the `.elf` cannot run on the PC at all.

#### Why CI needs its own Makefile

CubeIDE keeps the project settings in `.cproject` and generates a makefile from it on every build. That generated makefile lives in `Debug/`, which is not committed, and **has this machine's paths filled in.** That is fine for anyone else using CubeIDE, because their CubeIDE regenerates it from `.cproject` with their own paths.

The problem is that **only CubeIDE can translate `.cproject` into a makefile. A GitHub runner has no CubeIDE, so it needs a makefile that stands on its own.**

#### What CI does

Continuous Integration here means: **on every push, GitHub spins up a fresh Linux virtual machine, installs the tools, and tries to build the project from nothing but what is in the repository.** If a file was never committed or a path only exists on my machine, it fails there.

Beyond compiling, CI can also run test scripts, which I have not done yet. With hardware involved that part is harder. But the idea stands: **if the code does not even compile, it will not pass anything else.**

#### The files

I had Claude write the `Makefile` and the workflow file. The `.yml` is pretty straightforward and easy to read: it tells the runner to use Ubuntu, install the ARM toolchain (the `gcc-arm-none-eabi` package), then run `make` for the Debug build and `make RELEASE=1` for the Release build.

#### Checking the Makefile against CubeIDE

Before pushing, I built locally with the Makefile and compared the result with CubeIDE's build on the same branch:

| | text | data | bss |
|---|---|---|---|
| CubeIDE | 17784 | 272 | 1640 |
| Makefile | 17784 | 272 | 1640 |

Identical, so the Makefile reproduces CubeIDE's build exactly. The Release build came out at 10308 bytes of code, about 42% smaller with `-Os`. The first run on GitHub passed in about a minute and a half.