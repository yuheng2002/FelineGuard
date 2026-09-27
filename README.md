# FelineGuard

[![Build](https://github.com/yuheng2002/FelineGuard/actions/workflows/build.yml/badge.svg?branch=freertos)](https://github.com/yuheng2002/FelineGuard/actions/workflows/build.yml?query=branch%3Afreertos)

Firmware for a stepper-driven cat feeder, built on an STM32F446RE.

A feed can be triggered three ways — a serial command, a button press, or a daily alarm — and all three are arbitrated against a single motor. The firmware stays responsive during a feed, recovers from a hang on its own, and keeps its schedule across a reset.

> This is the FreeRTOS port. The original superloop version is on the [`main`](../../tree/main) branch — same hardware, same protocol, different execution model.

![Hardware setup: NUCLEO-F446RE, A4988 carrier on a breadboard, NEMA 17 driving the auger, 12 V supply](<docs/hardware setup.jpg>)

## What it does

- Dispenses a fixed serving by running an auger for a set time
- Accepts commands over UART as newline-terminated ASCII lines
- Takes a button press as an equivalent request, one press per serving
- Keeps a real-time clock and fires up to two scheduled feeds per day — two because the RTC has exactly two alarm registers, so each feeding time lives directly in hardware with no schedule table to maintain
- Detects a feed missed while it was down, and makes up **at most one** however many were missed
- Resets itself if a task stops giving up the CPU, and reports that on the next boot

## System overview

Three sources can request a feed. They converge on the MCU, which arbitrates
between them and drives a single STEP/DIR output through to the auger.

![System signal chain](<docs/System architecture.png>)

The 12 V motor supply reaches the coils only through the A4988 — it never
touches the logic side. The watchdog relationship is two-way: the idle task
refreshes it, and it resets the MCU if that stops happening.

## Firmware Architecture

Five layers. Dependencies point downward, apart from the two exceptions below the table.

![Layered architecture](<docs/Layered Architecture_FreeRTOS.png>)

| Layer | Contents | Responsibility |
|---|---|---|
| **Executive** | `main.c` | Initialization order and task creation; no feeding logic of its own |
| **Application** | `Comms`, `CmdProc`, `Feed`, `Button`, `Schedule` | The feeder's rules — framing, parsing, arbitration, scheduling. Each module runs as a task |
| **Middleware** | FreeRTOS kernel | Tasks, queues, mutexes and delays; used by the application and by UART_CTRL |
| **Driver** | `UART_CTRL`, `MOTOR_CTRL`, `IWDG_CTRL`, `RTC_CTRL`, `board.h` | One peripheral each; no knowledge of what a feed is |
| **HAL / CMSIS** | ST vendor code | Register access |

> Calls go downward, with two exceptions. `UART_CTRL` calls up into FreeRTOS to queue each received byte, and the kernel calls back into `main.c` through the idle and stack-overflow hooks. Within the application layer modules do call each other — CmdProc asks Feed, Feed reports through Comms.

One thing falls out of that split: **`Feed` is the sole owner of the motor.** Every other module asks it; nothing else calls `MOTOR_Start`.

`board.h` holds the pin map and interrupt priorities — a header of macros with no `.c` file. There is no GPIO driver, because the HAL already provides one and wrapping it would add an indirection with no content.

FreeRTOS takes SysTick for its tick. The HAL still needs a 1 ms tick for its timeouts, so that one moves to TIM7 (`System/Src/stm32f4xx_hal_timebase_tim.c`).

## How a feed happens

Each module runs as its own task and sleeps until it has something to do.

| Task | Priority | Blocks on | Then |
|---|---|---|---|
| Comms | 2 | a byte from the UART interrupt | adds it to the line; hands complete lines to CmdProc |
| CmdProc | 1 | a complete line | parses it and calls the matching module |
| Feed | 1 | a feed starting | waits five seconds, stops the motor |
| Button | 1 | a 20 ms delay | samples B1; requests a feed on release |
| Schedule | 1 | a 1 s delay | requests a feed if an alarm has fired |
| Idle | 0 | never blocks | refreshes the watchdog |

Comms runs one level higher because a full receive queue drops bytes, while a line waiting to be parsed just waits.

Every request goes through `Feed_Request()`, which applies one rule regardless of who asked:

| Source | Motor idle | Motor feeding |
|---|---|---|
| `FEED` command | accept | drop |
| Button press | accept | drop |
| Scheduled feed | accept | **defer** |

A scheduled feed is deferred rather than dropped because it is the only source with nobody present to try again.

![Feed arbitration state machine](<docs/Feed Arbitration FSM.png>)

On an idle-to-feeding transition, `Feed_Request()` starts the motor and signals `Feed_Task` through a binary semaphore. `Feed_Task` sleeps five seconds in `vTaskDelay()`, stops the motor, then either starts the deferred feed or goes back to waiting. This replaces v1's TIMER module and its interrupt.

Two mutexes guard what the tasks share: `state_mutex` around the feed state, and `tx_mutex` around the UART transmitter so responses from CmdProc and Feed never interleave.

## Reliability

**The idle task refreshes the watchdog.** It has the lowest priority, so it runs only when every other task is blocked. A task that holds the CPU for about a second starves it, and the watchdog resets the MCU. The reset cause is read from `RCC_CSR` and reported on the next boot.

**Two commands exist to prove it.** `CRASH` spins inside `CmdProc_Task` while idle. `CRASHFEED` starts a feed and then spins, so the fault lands during motor motion. No firmware runs to stop the motor — the reset clears the timer enable bit and returns the STEP pin to its reset state. Both are compiled out of a release build.

**A missed feed needs no special code path.** The RTC alarm flag is a level, not a pulse, and it lives in the backup domain. If the device was reset across an alarm time, the flag is still set when it comes back, and the first check by `Schedule_Task` reads it like any other alarm. Because it is one bit rather than a counter, missing two alarms is indistinguishable from missing one — the "make up at most one serving" policy comes from the hardware rather than from code enforcing it.

**A power cut is treated differently from a reset.** The calendar is lost when VDD drops, so the firmware checks whether the clock has ever been set before acting on any alarm. It reports `"Time not set"` and suspends scheduled feeding rather than feeding on a clock it has no reason to trust.

## Verification

After the port, these were rerun on hardware with the Debug build:

- **Feed and arbitration.** A feed stops after five seconds; a second `FEED` during a feed gets `Busy feeding` at once.
- **Button.** A press and release starts a feed.
- **Scheduled feed.** Alarm A fired on time.
- **Crash recovery.** `CRASH` and `CRASHFEED` both come back with `Recovered from crash`. After `CRASHFEED`, `Recovered from crash` arrives with no `Feed complete` before it — the watchdog reset came before the five-second feed could finish. In hardware a feed is just the STEP waveform from TIM2, and the reset turns TIM2 off, so the motor stopped at that moment.

Not yet rerun on this branch: the deferred feed, a missed feed across a reset, a power cycle, and the Release build against the [Test plan](<docs/Test plan.md>).

The STEP waveform was not re-measured. TIM2 and `MOTOR_CTRL` are unchanged from `main`, where it measured 250.25 Hz.

## Command protocol

Commands are ASCII lines terminated by `\n`. Arguments are fixed width, so parsing reads fixed offsets with no tokenizer.

| Command | Argument | Response |
|---|---|---|
| `FEED` | — | `Feeding started` → `Feed complete`, or `Busy feeding` |
| `PING` | — | `System ready` |
| `TIME` | `hh:mm` | `Time set` |
| `TIME?` | — | `hh:mm:ss`, or `Time not set` |
| `SCHED` | `A hh:mm` / `B hh:mm` | `Alarm A set` / `Alarm B set` |

Rejections come in two kinds. `Invalid command` means the line is not well formed; `Invalid time` means it is well formed but the value is not usable. The two lead the sender to do different things.

After opening the port, send one empty line before the first command. While the MCU is in reset its UART pins are undriven, and a glitch on the line can arrive as a stray byte in front of the first command. The empty line flushes it.

Full specification, including framing rules and every message the device can send: [Protocol.md](docs/Protocol.md).

## Hardware

| Part | Notes |
|---|---|
| NUCLEO-F446RE | STM32F446RE, board revision MB1136 C-04 (LSE crystal fitted) |
| A4988 carrier | HiLetgo StepStick clone, sense resistor 0.1 Ω |
| NEMA 17 stepper | STEPPERONLINE, 1.8°/step, 2 A per coil, 59 Ncm |
| 12 V supply | Motor only, isolated from the logic side |

The current limit is set with `V_ref = 8 × I_max × R_cs` — 0.8 V here, giving a 1.0 A vector limit and about 0.71 A per coil in full-step mode, comfortably under the motor's rating and within what the A4988 can dissipate with a heat sink and no forced air.

**Supply isolation.** The logic and motor supplies share a ground and nothing else. Neither is routed through a breadboard power rail, so there is no node where 12 V could reach a 3.3 V pin. That rule exists because an earlier board was destroyed exactly that way; the post-mortem is in the [Decision Log](docs/Decision%20Log.md).

The A4988 `EN` input is driven high while idle, so the coils are only energized during a feed rather than dissipating holding current continuously.

## Pin map

| Pin | Signal | Mode | Notes |
|---|---|---|---|
| PA0 | STEP to A4988 | AF1 (TIM2_CH1), pull-down | 250 Hz PWM, generated in hardware |
| PA1 | DIR to A4988 | Output, push-pull | Driven high at init |
| PA8 | EN to A4988 | Output, push-pull | Active low; high at idle, only low during a feed |
| PA2 | USART2 TX | AF7, no pull | 115200 8N1 |
| PA3 | USART2 RX | AF7, no pull | RXNE interrupt per byte |
| PC13 | User button B1 | Input, pull-up | Active low, RC-debounced on the board, polled every 20 ms |

### A note on probing the UART

PA2 and PA3 are routed to the on-board ST-LINK. UM1724 documents them as CN10 pin 35 and 37, but in the default hardware configuration those header pins are not electrically connected to the MCU — the solder bridges that would connect them are open, and the ones routing the signals to the ST-LINK are closed instead.

> I tried to capture the UART waveform and a logic analyzer on CN10 pin 35 showed nothing, even though the UART was working over the virtual COM port the whole time.

## Known limitations

- **The watchdog cannot see a task that blocks forever.** A task stuck waiting on a mutex or a queue gives up the CPU, so the idle task still runs and keeps refreshing. A deadlock looks healthy from the outside.
- **Dispensing is open loop.** The firmware controls how long the auger turns, not how many grams come out, and it cannot detect a skipped step.
- **The calendar does not survive a power cut.** VBAT is tied to VDD on this board, so the clock and schedule are lost and scheduled feeding suspends until `TIME` is sent again.
- **The alarm flag is cleared too early.** The protocol says to clear it after the feed; the firmware clears it right before. So a reset mid-feed cuts that serving short and it isn't made up.
- **The A4988 is briefly enabled at power-on.** Between reset and `MOTOR_Init()`, the `EN` pin floats and the driver's internal pull-down enables it. Harmless in the documented power-up order, since VMOT is not connected yet.
- **Hardware faults are not distinguished from bad input.** A HAL failure inside `RTC_SetTime` is reported as `Invalid time`, the same as an out-of-range hour. The distinction was dropped deliberately — a dead oscillator is not something the owner can act on.

## Future improvements

- [ ] **Task check-ins for the watchdog.** Refresh only once every task has reported in, which would also catch a task blocked forever. Comms, CmdProc and Feed block with `portMAX_DELAY`, so each would first need a timeout.
- [ ] **Scripted test.** A host-side script driving the serial link would make the test plan repeatable, and could write the test report itself.
- [ ] **Single supply.** 12V in, with the logic side derived through a regulator, removes the power-up ordering question entirely. The open problem is keeping the on-board ST-LINK usable without back-feeding it.
- [ ] **Proper motor mount.** The printed enclosure expects heat-set inserts at the motor face, which are not fitted yet.
- [ ] **Load cell on the auger.** An HX711 would make a serving weight-based rather than time-based, and would let the firmware notice a skipped step instead of assuming none.
- [ ] **SPI status display.** Would give the button a return path. A dropped button press is currently silent.

## Building

Build in **STM32CubeIDE**, or with `make` (below). The project is a plain Eclipse managed-build project — the HAL, CMSIS and FreeRTOS trees are checked in, and no `.ioc` file or CubeMX code generation is involved.

```
File → Open Projects from File System → select the repository root
```

Build the `Debug` configuration and flash over the on-board ST-LINK. `Debug` defines `DEBUG`, which is what compiles in the `CRASH` and `CRASHFEED` commands.

Without CubeIDE, the `Makefile` builds the same sources with `arm-none-eabi-gcc`. CI runs both builds on every push:

```
make              # Debug   → build/debug/FelineGuard.elf
make RELEASE=1    # Release → build/release/FelineGuard.elf
```

From v1.1.0 on, each release on the [Releases](../../releases) page has the Release `.elf` attached, built by CI and checked on hardware before publishing. No release has been tagged from this branch yet; `v2.*` releases will be. Flash it with STM32CubeProgrammer.

Serial settings: **115200 8N1**, line ending **LF**.

## Repository layout

```
Application/     Comms, CmdProc, Feed, Button, Schedule
Driver/          UART_CTRL, MOTOR_CTRL, IWDG_CTRL, RTC_CTRL, board.h
Executive/       main.c
FreeRTOS/        kernel, Cortex-M4F port, heap_4, FreeRTOSConfig.h
System/          syscalls, sysmem, system_stm32f4xx, hal_conf, HAL time base on TIM7
HAL/  CMSIS/     ST vendor code
Startup/         startup_stm32f446retx.s
docs/            protocol, decision log, journal, test plan, diagrams
Makefile         builds without CubeIDE
.github/         CI: build on every push, draft release on version tags
```

## Documentation

| Document | What it is |
|---|---|
| [Protocol.md](docs/Protocol.md) | The specification: commands, framing, arbitration rules, hardware constraints |
| [Decision Log.md](docs/Decision%20Log.md) | Design decisions for v1 and the alternatives rejected; not updated for this port — see the Journal for v2 |
| [Journal.md](docs/Journal.md) | Development log — what was built each day, what broke, and what the fix taught |
| [Test plan.md](<docs/Test plan.md>) | The Release build test procedure, shared by both branches |