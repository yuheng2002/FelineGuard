# FelineGuard

Firmware for a stepper-driven cat feeder, built on an STM32F446RE.

A feed can be triggered three ways — a serial command, a button press, or a daily alarm — and all three are arbitrated against a single motor. The firmware never blocks, recovers from a hang on its own, and keeps its schedule across a reset.

## What it does

- Dispenses a fixed serving by running an auger for a set time
- Accepts commands over UART as newline-terminated ASCII lines
- Takes a button press as an equivalent, one press per serving
- Keeps a real-time clock and fires up to two scheduled feeds per day
- Detects a feed that was missed while it was down, and makes up exactly one
- Resets itself if the main loop stops, and reports that on the next boot

## Hardware

| Part | Notes |
|---|---|
| NUCLEO-F446RE | STM32F446RE, board revision MB1136 C-04 (LSE crystal fitted) |
| A4988 carrier | HiLetgo StepStick clone, sense resistor 0.1 Ω |
| NEMA 17 stepper | STEPPERONLINE, 1.8°/step, 2 A per coil, 59 Ncm |
| 12 V supply | Motor only, isolated from the logic side |

The logic and motor supplies share a ground and nothing else. Neither one is routed through a breadboard power rail, so there is no node where 12 V could reach a 3.3 V pin. That rule exists because an earlier board was destroyed exactly that way — see [Decision Log](docs/Decision%20Log.md#hardware).

## Architecture

Four layers, with dependencies pointing in one direction only.

![Layered architecture](<docs/Layered Architecture.png>)

| Layer | Contents | Knows about |
|---|---|---|
| **Executive** | `main.c` | Application modules; the HAL only for system bring-up |
| **Application** | Comms, CmdProc, Feed, Button, Schedule | Drivers, and the feeder's own rules |
| **Driver** | UART, TIMER, MOTOR, IWDG, RTC, `board.h` | One peripheral each; nothing about feeding |
| **HAL / CMSIS** | ST vendor code | Registers |

Two things fall out of that split:

- **No application code touches a register or a HAL call.** The one exception is `HAL_Init()` in the Executive, which belongs to no peripheral.
- **`Feed` is the sole owner of the motor.** Every other module asks it; nothing else calls `MOTOR_Start`.

`board.h` holds the pin map and nothing else — a header of macros with no `.c` file. There is no GPIO driver, because the HAL already provides one and wrapping it would add an indirection with no content.

## How a feed happens

The main loop is a fixed sequence with no early exits. Each pass refreshes the watchdog, finishes any feed that has completed, and then gives each of the three sources a turn to raise a request.

![Main loop control flow](<docs/Control Flow.png>)

Requests all go to `Feed`, which owns the motor and applies one rule regardless of who asked:

| Source | Motor idle | Motor feeding |
|---|---|---|
| `FEED` command | accept | drop |
| Button press | accept | drop |
| Scheduled feed | accept | **defer** |

A scheduled feed is deferred rather than dropped because it is the only source with nobody present to try again.

![Feed arbitration state machine](<docs/Feed Arbitration FSM.png>)

## Reliability

**The main loop stops the motor, not the timer interrupt.** Interrupts keep firing while the main loop is hung — the CPU is still executing, just stuck. If the interrupt stopped the motor, a system that had already died would still finish its feed cleanly and look healthy. Leaving it to the main loop makes `"Feed complete"` mean that the code the watchdog supervises is still alive.

**The watchdog refresh is the evidence.** It happens in the main loop and nowhere else. Any path that fails to return to the top of the loop within about a second forces a hardware reset, and the reset cause is read from `RCC_CSR` and reported on the next boot.

**Two commands exist to prove it.** `CRASH` hangs the CPU while idle. `CRASHFEED` hangs it mid-feed, which tests something the first cannot: that a fault during motor motion still ends with the motor stopped. No firmware runs to stop it — the reset clears the timer enable bit and returns the STEP pin to its reset state. Both are compiled out of a release build.

**A missed feed needs no special code path.** The RTC alarm flag is a level, not a pulse, and it lives in the backup domain. If the device was reset across an alarm time, the flag is still set when it comes back, and the first ordinary pass of the main loop reads it like any other alarm. Because it is one bit rather than a counter, missing two alarms is indistinguishable from missing one — the "make up at most one serving" policy comes from the hardware rather than from code enforcing it.

**A power cut is treated differently from a reset.** The calendar is lost when VDD drops, so the firmware checks whether the clock has ever been set before acting on any alarm. It reports `"Time not set"` and suspends scheduled feeding rather than feeding on a clock it has no reason to trust.

## Command protocol

Commands are ASCII lines terminated by `\n`. Arguments are fixed width, so parsing reads fixed offsets with no tokenizer.

```
> TIME 14:30
Time set
> SCHED A 08:00
Alarm A set
> TIME?
14:30:52
> FEED
Feeding started
Feed complete
> FEED
Feeding started
> FEED
Busy feeding
```

| Command | Argument | Response |
|---|---|---|
| `FEED` | — | `Feeding started` → `Feed complete`, or `Busy feeding` |
| `PING` | — | `System ready` |
| `TIME` | `hh:mm` | `Time set` |
| `TIME?` | — | `hh:mm:ss`, or `Time not set` |
| `SCHED` | `A hh:mm` / `B hh:mm` | `Alarm A set` / `Alarm B set` |

Rejections come in two kinds. `Invalid command` means the line is not well formed; `Invalid time` means it is well formed but the value is not usable. The two lead the sender to do different things.

Full specification, including framing rules and every response the device can send: [Protocol.md](docs/Protocol.md).

## Building

Requires **STM32CubeIDE**. The project is a plain Eclipse managed-build project — the HAL and CMSIS trees are checked in, and no `.ioc` file or CubeMX code generation is involved.

```
File → Open Projects from File System → select the repository root
```

Then build the `Debug` configuration and flash over the on-board ST-LINK. `Debug` defines `DEBUG`, which is what compiles in the `CRASH` and `CRASHFEED` commands.

Serial settings: **115200 8N1**, line ending **LF**.

---

## Repository layout

```
Application/     Comms, CmdProc, Feed, Button, Schedule
Driver/          UART_CTRL, TIMER, MOTOR_CTRL, IWDG_CTRL, RTC_CTRL, board.h
Executive/       main.c
System/          syscalls, sysmem, system_stm32f4xx, hal_conf
HAL/  CMSIS/     ST vendor code
Startup/         startup_stm32f446retx.s
docs/            protocol, decision log, journal, diagrams
```

## Documentation

| Document | What it is |
|---|---|
| [Protocol.md](docs/Protocol.md) | The specification: commands, framing, arbitration rules, hardware constraints |
| [Decision Log.md](docs/Decision%20Log.md) | Every design decision with the alternative that was rejected and why |
| [Journal.md](docs/Journal.md) | Development log — what was built each day, what broke, and what the fix taught |

The Decision Log is the shortest and the best place to start if the question is *why* rather than *what*.
