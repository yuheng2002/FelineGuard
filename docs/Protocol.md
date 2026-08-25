# FelineGuard — STM32F446RE Cat Feeder Protocol

## Contents
1. Scope
2. Tools and Related Items
3. References
4. Available Operations
5. Considerations

## 1. Scope
This document describes the cat feeder protocol: the operations the device supports, the rules that govern them, and the responses the host can expect.

## 2. Tools and Related Items
- MCU: STM32F446RE (NUCLEO-F446RE, board version MB1136 C-04)
- Stepper driver: A4988 carrier (HiLetgo StepStick clone, $R_CS = 0.1 Ω$)
- Motor: STEPPERONLINE NEMA 17 bipolar stepper, 1.8 degrees per step, 2 A per coil, 59 Ncm
- Motor supply: 12 V, separate from the logic supply (see Section 5.1)
- A host-side scripting language, e.g. Python, for UART communication
- SPI display (deferred)

## 3. References
- STMicroelectronics, *RM0390 Reference Manual: STM32F446xx*
- STMicroelectronics, *UM1724 User Manual: STM32 Nucleo-64 boards*
- Allegro MicroSystems, *A4988 Datasheet*
- Pololu, *A4988 Stepper Motor Driver Carrier* ([product page](https://www.pololu.com/product/1182)) -- documents the $V_ref$ equation; note the sense resistor value differs from the clone used here

## 4. Available Operations

A feed can be triggered from three sources: a UART command, the on-board button, and the RTC schedule. All three request the same action, one serving (Section 5.2), and all three are arbitrated by the same rule (Section 5.3).

### 4.1 Command Format

A command is a line of ASCII text ending with `\n` (LF, 0x0A). The device stores incoming bytes until it receives `\n`, then parses the whole line as one command. Nothing is executed before the terminator arrives.

Because the line is parsed as a unit, a command is either complete or not there at all. A burst of bytes cannot be split into several commands by accident, and a line that is not recognised is rejected whole rather than partially executed.

Parsing rules:

- A trailing `\r` (CR, 0x0D) is removed before parsing. Some terminals end a line with `\r\n` and others with `\n`. Both are accepted.
- An empty line is ignored and gets no response.
- The command name ends at the first space, or at the end of the line if there is no space. Anything after that space is the argument.
- Arguments are fixed width and zero padded. `08:00` is valid, `8:0` is not. This keeps parsing to fixed offsets and removes the need for a tokenizer.
- A command name ending in `?` is a query: it reports a value and changes nothing. The convention is borrowed from SCPI. `TIME` and `TIME?` are therefore separate commands, distinguished in the text itself rather than by whether an argument is present.
- If a line is longer than the receive buffer, it is discarded. The device then discards every byte that follows until the next `\n`. Without this rule the tail of an oversized line would be parsed as a new command.

**Two kinds of rejection.** A line can fail for two different reasons, and the device says which:

- `"Invalid command"` — the line is not a well-formed command. An unknown name, a missing space, the wrong length, a non-digit where a digit belongs.
- `"Invalid time"` — the command is well-formed but the value is not usable. An hour above 23, a minute above 59.

The two lead the sender to do different things: check the syntax, or change the number.

### 4.2 UART Commands

| Command | Argument | Description | Response |
|---|---|---|---|
| `FEED` | — | Dispense one serving | `"Feeding started"`, then `"Feed complete"`; `"Busy feeding"` if refused |
| `PING` | — | Check the link | `"System ready"` |
| `TIME` | `hh:mm` | Set the current time of day | `"Time set"` or `"Invalid time"` |
| `TIME?` | — | Report the current time | `"hh:mm:ss"`, or `"Time not set"` |
| `SCHED` | `A hh:mm` or `B hh:mm` | Set one of the two feeding times | `"Alarm A set"` / `"Alarm B set"`, or `"Invalid time"` |
| `CRASH` | — | *(debug)* Hang the CPU while idle | *(on next boot)* `"Recovered from crash"` |
| `CRASHFEED` | — | *(debug)* Hang the CPU during a feed | *(on next boot)* `"Recovered from crash"` |

Any line that is not one of these gets `"Invalid command"`.

The device also sends messages that are not answers to a command:

| Message | When |
|---|---|
| `"Feed complete"` | A feed has finished, whatever started it |
| `"Deferred feed started"` | A feed that had been deferred is now running (Section 5.3) |
| `"Recovered from crash"` | At startup, when the last reset came from the watchdog |
| `"Time not set"` | At startup, when the RTC calendar has never been set |
| `"RTC clock failed to initialize"` | At startup, when the LSE crystal did not start |

`"Feed complete"` is sent for every feed, including ones started by the button or the schedule. It is a report of what the device did, not an answer to whoever asked.

#### FEED
Dispenses one serving.

#### PING
Confirms that the device is present and responding. `PING` is answered at any time, including during a feed, because it reports liveness and not availability.

#### TIME and TIME?
`TIME` sets the time of day in the RTC calendar. `TIME?` reports it back.

The two formats are deliberately not symmetric. Setting takes `hh:mm` and implies `:00`, because the owner should not have to type seconds. The query returns `hh:mm:ss`, because reading the seconds is the most direct way to confirm the calendar is actually advancing.

The date is not set by the owner. The firmware uses a fixed start date, which is enough because the schedule repeats daily and never refers to a calendar date. The year is set to a non-zero value on purpose: the RTC reports a calendar as initialized based on the year field being different from 0, and that flag is what Section 4.4 relies on.

#### SCHED
Sets one of the two daily feeding times. `A` and `B` select which one. Two feeds per day is a fixed limit, because the RTC has exactly two alarms and each feeding time is held in one of them.

Naming the slot means a feeding time can be changed on its own. With an implicit "keep the two most recent" scheme, changing one time would require re-entering both.

#### CRASH and CRASHFEED
Both are development aids, not product features, and are compiled out of a release build. Neither is considered a feed request, so neither appears in the arbitration table in Section 5.3.

`CRASH` stops the main loop while the device is idle. The watchdog then resets the MCU, and the reset cause is reported on the next boot.

`CRASHFEED` starts a feed and then stops the main loop while the motor is still running. It tests one thing that `CRASH` cannot: that a crash during motor motion still ends with the motor stopped. No firmware runs to stop it — a system reset clears the timer's enable bit and returns the STEP pin to its reset state, which removes the waveform. That is the assumption worth testing on real hardware. The result is checked by watching the motor, not by a report from the firmware.

Both commands hang the CPU on purpose, so any byte sent between the command and the reset is lost. The host should send a debug command only when the device is idle, and should wait for the boot message before sending anything else.

### 4.3 Button

Pressing and releasing the on-board user button (PC13, active low) requests one serving. The press is one discrete event. Holding the button down does not dispense more, and releasing it does not stop a feed early.

### 4.4 Scheduled Feed

Each feeding time set by `SCHED` is held in one of the RTC alarm registers, Alarm A or Alarm B. The date fields of the alarm are masked, so each alarm triggers once per day at the same time of day and the firmware does not have to re-arm it.

The alarm registers and the calendar are both in the backup domain. They keep their contents through a system reset, including a watchdog reset, and lose them when VDD is removed. The clock and the schedule therefore have the same lifetime. If one is lost, the other is lost with it.

**When the clock is not set.** The RTC reports whether its calendar has ever been initialized. If it has not, which is the normal state after a power cycle, scheduled feeding is suspended and the device reports this at startup. A `SCHED` command is still accepted and stored in this state; it takes effect as soon as `TIME` sets the clock.

This guard matters more than it looks. The calendar counts from 00:00:00 whether or not anyone has set it, so an alarm can genuinely fire on a clock that means nothing. Without the guard the device would feed at an arbitrary time.

**Order of `TIME` and `SCHED`.** The two commands can be sent in any order. Setting the clock does not trigger a feed whose time has already passed. The owner can therefore skip the next scheduled feed by setting the clock to just after it.

**Missed feed after a reset.** The alarm flag is set by hardware when an alarm triggers and is cleared only by software. It is also in the backup domain, so it survives a reset. If the flag is set when the firmware comes back up, an alarm fired while the firmware was not running, and the device dispenses one serving.

No separate startup check exists for this. The flag is a level rather than a pulse, so the first ordinary pass of the main loop reads it exactly like any other alarm.

The flag is a single bit and not a counter, so at most one serving is dispensed no matter how many alarms were missed. This matches the policy: the firmware cannot know whether the owner fed the cat by hand, so it must not replay every missed feed.

The flag is cleared after that feed completes, not before. If the device is reset between the feed and the clear, the same feed is repeated on the next boot. This is deliberate, for the same reason as everywhere else in this document: one extra serving is safer than a missed one.

> Known limitation: in a repeating reset loop, this repeats on every boot. It is accepted, because a reset loop is a fault the watchdog cannot clear by itself and needs the owner in any case.

## 5. Considerations

### 5.1 Power Supply

The MCU is powered over USB from the host and runs at 3.3 V. The NEMA 17 motor needs a separate 12 V supply on the A4988 VMOT input.

**The 12 V rail must never share a breadboard power rail with the 3.3 V or 5 V logic rails.** A logic pin is rated for 3.3 to 5 V. Applying 12 V to one destroys it within milliseconds, whether the board is powered or not. The two domains share only a common ground, which the A4988 needs in order to read the STEP and DIR levels correctly.

As a secondary precaution, power the MCU before the 12 V supply, and remove the 12 V supply first on shutdown. This is a habit, not a safeguard. The rule above is the one that prevents the failure.

The A4988 `EN` input is driven high while idle, so the coils are only energised during a feed. Holding current would otherwise be dissipated continuously, and a cat feeder is idle for almost all of its life.

### 5.2 The Serving

A **serving** is the unit in which all feeds are dispensed. Every feed dispenses exactly one serving, whatever triggered it.

A serving is one dispensing cycle of fixed duration, currently about 5 s. At 250 Hz in full-step mode with a 200-step motor, that is 6.25 revolutions of the auger.

It is therefore an open-loop quantity: the firmware controls how long the auger turns, not how many grams come out, and it has no way to detect a skipped step. A load cell would make the serving weight based and close that loop. This is left as future work.

### 5.3 Feed Arbitration

The motor is a single shared resource, so only one feed can run at a time. Feed requests arrive from three sources, and the same rule arbitrates all of them. The rule does not depend on whether the source can receive an answer.

| Source of feed request | Motor idle | Motor feeding |
|---|---|---|
| `FEED` command | accept | drop |
| Button press | accept | drop |
| Scheduled feed | accept | defer |

- A **dropped** request is discarded. It is never queued.
- A **deferred** request is held and runs when the current feed finishes. A scheduled feed is delayed instead of lost, because it is the one source with nobody present to send it again.

Only one scheduled feed can be held at a time. A second one arriving while another is already deferred is dropped. Because seconds are fixed at zero, two alarms are at least a minute apart and a feed lasts five seconds, so an alarm can never collide with another alarm — the deferred state is only ever reached by an alarm arriving during a manual feed.

![Feed arbitration state machine](Feed_Arbitration_FSM.png)

"Feeding" here describes the motor as a physical resource, not firmware availability. The firmware never blocks. The main loop keeps running and keeps receiving bytes for the whole duration of a feed.

![Main loop control flow](Control_Flow.png)

### 5.4 Feedback on a Dropped Request

Feedback belongs to the source, not to the arbitration rule. A dropped `FEED` is answered with `"Busy feeding"`, because UART has a return path to the host. A dropped button press is ignored silently, because the button has no return path. Giving it one would need a separate output channel, such as a status display.

This is why a host-side test can only exercise one of the three sources directly. The button and the schedule are observed by watching the motor.

### 5.5 Watchdog Timeout

The independent watchdog (IWDG) timeout is about 1 s. The main loop refreshes it once per pass, so any path that does not return to the top of the loop within that window forces a hardware reset.

The IWDG runs off the LSI, an internal RC oscillator specified at 17 to 47 kHz, so "one second" is really somewhere between roughly 0.7 s and 1.9 s. The margin is large enough that this does not matter: nothing in the firmware blocks for more than a few milliseconds, the longest single operation being a UART transmission of about 1.3 ms at 115200 baud. The timeout is set by how quickly the device should recover from a hang, not by any operation it has to tolerate.

Refreshing the watchdog is the main loop's job and only the main loop's. The refresh is the evidence that the supervised code is still running, which is also why the motor is stopped from the main loop rather than from the timer interrupt: interrupts keep firing while the main loop is hung, so an interrupt-driven stop would let a dead system finish a feed and look healthy.
