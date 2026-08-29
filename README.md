# FelineGuard

Firmware for a stepper-driven cat feeder, built on an STM32F446RE.

A feed can be triggered three ways — a serial command, a button press, or a daily alarm — and all three are arbitrated against a single motor. The firmware never blocks, recovers from a hang on its own, and keeps its schedule across a reset.

![Hardware setup: NUCLEO-F446RE, A4988 carrier on a breadboard, NEMA 17 driving the auger, 12 V supply](<docs/hardware setup.jpg>)

## What it does

- Dispenses a fixed serving by running an auger for a set time
- Accepts commands over UART as newline-terminated ASCII lines
- Takes a button press as an equivalent request, one press per serving
- Keeps a real-time clock and fires up to two scheduled feeds per day — two because the RTC has exactly two alarm registers, so each feeding time lives directly in hardware with no schedule table to maintain
- Detects a feed missed while it was down, and makes up **at most one** however many were missed
- Resets itself if the main loop stops, and reports that on the next boot

## System overview

Three sources can request a feed. They converge on the MCU, which arbitrates
between them and drives a single STEP/DIR output through to the auger.

![System signal chain](<docs/System architecture.png>)

The 12 V motor supply reaches the coils only through the A4988 — it never
touches the logic side. The watchdog relationship is two-way: the main loop
refreshes it, and it resets the MCU if that stops happening.

## Firmware Architecture

Four layers, with dependencies pointing in one direction only.

![Layered architecture](<docs/Layered Architecture.png>)

| Layer | Contents | Responsibility |
|---|---|---|
| **Executive** | `main.c` | Initialization order and the main loop; no feeding logic of its own |
| **Application** | Comms, CmdProc, Feed, Button, Schedule | The feeder's rules — framing, parsing, arbitration, scheduling |
| **Driver** | UART_CTRL, TIMER, MOTOR_CTRL, IWDG_CTRL, RTC_CTRL, `board.h` | One peripheral each; no knowledge of what a feed is |
| **HAL / CMSIS** | ST vendor code | Register access |

> No layer calls into the one above it. Within the application layer modules do call each other — CmdProc asks Feed, Feed reports through Comms — but nothing below ever calls back up, otherwise it would be a dependency inversion.

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

## Verification

Each module was tested on hardware as it was written, and the whole chain end to end once the last one was in place.

**STEP waveform.** Measured on PA0 with a logic analyzer: **250.56 Hz**, period **3.991 ms**. The 0.2% error comes from the HSI internal RC oscillator, which is specified at ±1%. At 250 Hz in full-step mode with a 200-step motor, a five-second serving is 6.25 revolutions of the auger.

![250 Hz STEP waveform captured with a logic analyzer](<docs/250Hz waveform.png>)

A representative session over the serial link:

```
> PING
System ready
> TIME 14:30
Time set
> TIME?
14:30:52
> SCHED A 14:32
Alarm A set
Feed complete
> FEED
Feeding started
Feed complete
> FEED
Feeding started
> FEED
Busy feeding
> CRASH
Recovered from crash
> CRASHFEED
Recovered from crash
```

**Scheduled feed.** The `Feed complete` after `Alarm A set` arrived on its own two minutes later, with nothing sent in between.

**Arbitration.** The second `FEED` lands inside the five-second window and is refused rather than queued.

**Crash recovery.** `CRASH` hangs the main loop while idle; the watchdog resets the MCU and the next boot reports the cause.

**Crash during motion.** `CRASHFEED` hangs mid-feed. The motor stopped about a second in instead of running the full five — the reset arrived first.

**Missed feed.** With Alarm A set one minute ahead, the reset button was held down across the alarm time. On release, a feed ran: the alarm flag had been set by hardware while the CPU was in reset, and the first pass of the main loop consumed it.

**Power cycle.** Pulling the USB cable and reconnecting brings back `Time not set`, and scheduled feeding stays suspended until `TIME` is sent again.

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

## Known limitations

- **Dispensing is open loop.** The firmware controls how long the auger turns, not how many grams come out, and it cannot detect a skipped step.
- **The calendar does not survive a power cut.** VBAT is tied to VDD on this board, so the clock and schedule are lost and scheduled feeding suspends until `TIME` is sent again.
- **A repeating reset loop repeats the make-up feed.** The alarm flag is cleared after the feed rather than before, so a reset landing between the two replays it. This is the deliberate direction: an extra serving is recoverable, a missed one is not.
- **The A4988 is briefly enabled at power-on.** Between reset and `MOTOR_Init()`, the `EN` pin floats and the driver's internal pull-down enables it. Harmless in the documented power-up order, since VMOT is not connected yet.
- **Hardware faults are not distinguished from bad input.** A HAL failure inside `RTC_SetTime` is reported as `Invalid time`, the same as an out-of-range hour. The distinction was dropped deliberately — a dead oscillator is not something the owner can act on.

## Future improvements

- [ ] **Scripted test against a Release build.** Everything under Verification was run by hand on a `Debug` build. A host-side script driving the serial link would make the test repeatable, and running it against `Release` (`-O3`) would catch anything that quietly depends on a missing `volatile` or on debug-build timing.
- [ ] **Load cell on the auger.** An HX711 would make a serving weight-based rather than time-based, and would let the firmware notice a skipped step instead of assuming none.
- [ ] **SPI status display.** Would give the button a return path. A dropped button press is currently silent.
- [ ] **Single supply.** 12V in, with the logic side derived through a regulator, removes the power-up ordering question entirely. The open problem is keeping the on-board ST-LINK usable without back-feeding it.
- [ ] **Proper motor mount.** The printed enclosure expects heat-set inserts at the motor face, which are not fitted yet.

## Building

Requires **STM32CubeIDE**. The project is a plain Eclipse managed-build project — the HAL and CMSIS trees are checked in, and no `.ioc` file or CubeMX code generation is involved.

```
File → Open Projects from File System → select the repository root
```

Build the `Debug` configuration and flash over the on-board ST-LINK. `Debug` defines `DEBUG`, which is what compiles in the `CRASH` and `CRASHFEED` commands.

Serial settings: **115200 8N1**, line ending **LF**.

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
