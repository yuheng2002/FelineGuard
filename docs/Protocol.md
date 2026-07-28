# FelineGuard — STM32F446RE Cat Feeder Protocol

## Contents
1. Scope
2. Tools and Related Items
3. References
4. Considerations
5. Available Operations

## 1. Scope
This document describes the cat feeder protocol: the operations the device
supports, the rules governing them, and the responses the host can expect.

## 2. Tools and Related Items
- MCU: STM32F446RE (NUCLEO-F446RE)
- SPI display, e.g. an OLED module (specific model TBD)
- A host-side scripting language, e.g. Python, for UART communication

## 3. References
- STMicroelectronics, *RM0390 Reference Manual: STM32F446xx*
- STMicroelectronics, *UM1724 User Manual: STM32 Nucleo-64 boards*
- Allegro MicroSystems, *A4988 Datasheet*

## 4. Considerations

### 4.1 Power Supply
The MCU is powered over USB from the host and runs at 3.3 V. The NEMA 17 motor
requires a separate 12 V supply on the A4988's VMOT input.

**The 12 V rail must never share a breadboard power rail with the 3.3 V or 5 V
logic rails.** A logic pin is rated for 3.3–5 V; applying 12 V to one destroys it
within milliseconds, powered or not. The two domains share only a common ground,
which is required for the A4988 to interpret the STEP/DIR levels correctly.

As a secondary precaution, power the MCU up before the 12 V supply and remove the
12 V supply first on shutdown.

### 4.2 The Serving
A **serving** is the unit in which all feeds are dispensed. Every feed, whatever
triggered it, dispenses exactly one serving.

A serving is currently defined as one dispensing cycle of fixed duration
(approximately 5 s). It is therefore an *open-loop* quantity: the firmware
controls how long the auger turns, not how many grams come out. Adding a load
cell would make the serving weight-based and close that loop; this is left as
future work.

### 4.3 Feed Arbitration
The motor is a single shared resource, so only one feed may run at a time. Feed
requests arrive from three independent sources (Section 5), and all three are
arbitrated by the same rule, independently of how — or whether — the source is
able to receive feedback.

| Source of feed request | Motor idle | Motor feeding |
|------------------------|------------|---------------|
| UART `F`               | accept     | drop          |
| Button press           | accept     | drop          |
| RTC schedule           | accept     | defer         |

* A dropped request is discarded outright; it is never queued. 
* A deferred request
is held and executed once the current feed completes, so that a scheduled feed is
delayed rather than lost — the schedule is the one source with no one present to
retry it.

Note that "feeding" describes the motor as a shared physical resource, not
firmware availability. The firmware never blocks: the main loop keeps running and
keeps receiving bytes for the full duration of a feed.

### 4.4 Feedback on a Dropped Request
Feedback is a property of the source, not of the arbitration rule. A dropped `F`
is answered with `"Busy feeding"`, because UART has a return path to the host. A
dropped button press is silently ignored, because the button has no return path;
adding one would require a separate output channel, such as a status display.

### 4.5 Unrecognized Commands
Any byte that is not a defined command is discarded, and the firmware responds
`"Invalid command. Try again!"`.

## 5. Available Operations
A feed can be triggered from three independent sources: a UART command, the
on-board button, and the RTC schedule. All three request the same action — one
serving — and are arbitrated by the rule in Section 4.3.

### 5.1 UART Commands

| Byte | Command | Description    | Response |
|------|---------|----------------|----------|
| 0x46 | `F`     | Start feed     | `"Feeding started"`, then `"Feed complete"` |
| 0x48 | `H`     | Handshake      | `"System ready!"` |
| 0x43 | `C`     | Crash test *(debug)*     | *(on next boot)* `"System recovered from crash!"` |
| 0x58 | `X`     | Crash mid-feed *(debug)* | *(on next boot)* `"System crashed while feeding. Recovered!"` |

`C` and `X` are test-only commands, not product features, and would be compiled out of a release build. Neither is considered a real feed request and deliberately excluded from the arbitration table in Section 4.3

#### F — Feed
Dispenses one serving.

#### H — Handshake
Verifies the link between host and device, confirming that the device is present
and responding. `H` is answered at any time, including during a feed, since it
reports liveness rather than availability.

#### C — Crash Test
Deliberately hangs the CPU to verify watchdog recovery. The firmware stops
feeding the watchdog, the MCU resets, and the reset cause is reported on the next
boot.

#### X — Crash Mid-Feed
Starts a feed and then hangs partway through it, so that the crash occurs while a
dispense is in progress. This exercises the crash-safe feed record: on the next
boot the firmware must report both the watchdog reset and the interrupted feed.

### 5.2 Button
Pressing and releasing the on-board user button (PC13, active-low) requests one
serving. The press is treated as one individual event: holding the button down does
not dispense more, and releasing it does not cut a feed short.

### 5.3 RTC Timed Feed
The RTC provides absolute timekeeping that survives a reset, allowing feeds to be
scheduled at real times of day. A scheduled feed is a third trigger for the same
action, not a UART command.

**Missed feeds after a reboot.** On startup the firmware compares the current RTC
time against the feed log in Flash. If one or more scheduled feeds were missed, it
dispenses **at most one serving**, then resumes the normal schedule.

Reasoning: the firmware is not aware of whether the owner fed the cat by hand
during the outage, so it must not replay every missed serving. Dispensing one
serving keeps the cat from waiting until the next scheduled time. Moderate over-feeding is safer than potentially starving a cat.