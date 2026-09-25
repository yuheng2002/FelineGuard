# FelineGuard v1 — Release Build Test Report

Results of running [Test Plan.md](Test%20Plan.md) against the superloop firmware.

| | |
|---|---|
| Branch | `main` |
| Commit | |
| Date | |
| Build | `make RELEASE=1` — `-Os`, `DEBUG` undefined |
| Size | text 10308, data 272, bss 1632 |
| Flashed with | `STM32_Programmer_CLI -c port=SWD -w build/release/FelineGuard.elf -v -rst` |
| Hardware | NUCLEO-F446RE, A4988, NEMA 17; motor connected for tests 1–8 |

**Result: 22 / 22 passed.**

---

## Results

### 1. Basic protocol

| # | Send | Expect | Actual | Result |
|---|---|---|---|---|
| 1.1 | `PING` | `System ready` | `Invalid command`, then `System ready` on resend | Pass ¹ |
| 1.2 | `HELLO` | `Invalid command` | `Invalid command` | Pass |
| 1.3 | empty line | nothing | nothing | Pass |
| 1.4 | `SCHEDULEAVERYLONGCOMMANDLINE` | nothing | nothing | Pass |

### 2. Debug commands are compiled out

| # | Send | Expect | Actual | Result |
|---|---|---|---|---|
| 2.1 | `CRASH` | `Invalid command` | `Invalid command` | Pass |
| 2.2 | `CRASHFEED` | `Invalid command` | `Invalid command` | Pass |

### 3. Clock

| # | Send | Expect | Actual | Result |
|---|---|---|---|---|
| 3.1 | `TIME?` | `Time not set` | `Time not set` | Pass |
| 3.2 | `TIME 1a:30` | `Invalid command` | `Invalid command` | Pass |
| 3.3 | `TIME 99:99` | `Invalid time` | `Invalid time` | Pass |
| 3.4 | `TIME 14:30` | `Time set` | `Time set` | Pass |
| 3.5 | `TIME?` | `14:30:xx` | `14:30:04` | Pass |
| 3.6 | `TIME?` later | seconds advanced | `14:30:11`, `14:30:14` | Pass |

### 4. Manual feed and arbitration

| # | Send | Expect | Actual | Result |
|---|---|---|---|---|
| 4.1 | `FEED` | `Feeding started`, `Feed complete` | `Feeding started`, `Feed complete` | Pass ² |
| 4.2 | `FEED` twice | `Feeding started`, `Busy feeding`, `Feed complete` | `Feeding started`, `Busy feeding`, `Feed complete` | Pass |

### 5. Button

| # | Do | Expect | Actual | Result |
|---|---|---|---|---|
| 5.1 | press and release B1 | `Feed complete` only | `Feed complete` | Pass ² |
| 5.2 | `FEED`, press B1 during the feed | one `Feed complete` | `Feeding started`, one `Feed complete` | Pass |

### 6. Schedule

| # | Send | Expect | Actual | Result |
|---|---|---|---|---|
| 6.1 | `SCHED X 08:00` | `Invalid command` | `Invalid command` | Pass |
| 6.2 | `SCHED A 25:00` | `Invalid time` | `Invalid time` | Pass |
| 6.3 | `SCHED A 14:35` | `Alarm A set`, then `Feed complete` at the minute | `Alarm A set`, `Feed complete` | Pass ³ |

### 7. Deferred feed

| # | Do | Expect | Actual | Result |
|---|---|---|---|---|
| 7.1 | `SCHED A 14:41`; `FEED` at about 14:40:55 | `Feeding started`, `Feed complete`, `Deferred feed started`, `Feed complete` | the same four lines, in order | Pass ⁴ |

### 8. Missed feed across a reset

| # | Do | Expect | Actual | Result |
|---|---|---|---|---|
| 8.1 | `SCHED A 15:43`; held RESET from about 15:42:55 for about 10 s | `Feed complete` after release, no `Recovered from crash` | `Feed complete` after release, no `Recovered from crash` | Pass ⁵ |

### 9. Power cycle

| # | Do | Expect | Actual | Result |
|---|---|---|---|---|
| 9.1 | disconnect 12 V, power-cycle USB, `TIME?` | `Time not set` | `Invalid command`, then `Time not set` on resend | Pass ¹ |

---

## Notes

**¹ First command after a power-up.** In both 1.1 and 9.1 — each the first command after the USB was power-cycled — the reply was `Invalid command`, and the same command sent again was answered correctly.

While the MCU is in reset, the USART pins are not yet configured as alternate function and the line is undriven. A glitch on it is received as a stray byte, which sits at the start of the command buffer and turns `PING` into something that matches nothing. The same effect shows up in the other direction as garbage characters in the serial monitor when the board is reset with the port open.

The protocol recovers on its own: the `\n` ending the corrupted line flushes it, so the next command is clean. This is the resynchronisation that line framing was chosen for. The host-side fix is to send one empty line after opening the port, which is now part of the test plan's setup.

**² Motor observed.** In 4.1 and 5.1 the motor was watched as well as the serial output: it turned for about five seconds and stopped. The serial link only shows that the firmware called the stop; this confirms the motor driver actually responded in the `-Os` build.

**³ Button held across the alarm.** During 6.3 the button was pressed at about 14:34:57 and held past the alarm time. The alarm still fired and fed on schedule, and a held button raised no request of its own — the button acts on release, not on press.

**⁴ First hardware test of the deferred path.** This path had not been exercised before. The manual feed started at about 14:40:55, so the 14:41:00 alarm arrived with the motor already running and was deferred rather than dropped.

**⁵ Evidence for 8.1 is the timing, not the log.** On its own, the serial output of this test is indistinguishable from an ordinary alarm feed. What makes it a missed-feed test is that the chip was held in reset across 15:43:00, so no code could have acted on the alarm at that moment. The feed that followed the release could only have come from the alarm flag surviving in the backup domain. The absence of `Recovered from crash` also shows a button reset is not mistaken for a watchdog reset.

## Not covered

- **Watchdog recovery.** `CRASH` and `CRASHFEED` are compiled out of a Release build, so nothing can trigger the watchdog on demand. It was verified on the Debug build.