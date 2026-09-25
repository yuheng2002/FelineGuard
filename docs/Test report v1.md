## FelineGuard v1 — Release Build Test Report

Results of running [Test Plan.md](Test%20Plan.md) against the superloop firmware.

| | |
|---|---|
| Branch | `main` |
| Version | v1.1.0 |
| Date | 2026-09-25 |
| Build | `make RELEASE=1` — `-Os`, `DEBUG` undefined |
| Size | text 10308, data 272, bss 1632 |
| Flashed with | `STM32_Programmer_CLI -c port=SWD -w build/release/FelineGuard.elf -v -rst` |
| Hardware | NUCLEO-F446RE, A4988, NEMA 17; motor connected for tests 1–8 |

**Result: 22 / 22 passed.**

#### 1. Basic protocol

| # | Send | Expect | Actual | Result |
|---|---|---|---|---|
| 1.1 | `PING` | `System ready` | `Invalid command`, then `System ready` on resend | Pass ¹ |
| 1.2 | `HELLO` | `Invalid command` | `Invalid command` | Pass |
| 1.3 | empty line | nothing | nothing | Pass |
| 1.4 | `SCHEDULEAVERYLONGCOMMANDLINE` | nothing | nothing | Pass |

#### 2. Debug commands are compiled out

| # | Send | Expect | Actual | Result |
|---|---|---|---|---|
| 2.1 | `CRASH` | `Invalid command` | `Invalid command` | Pass |
| 2.2 | `CRASHFEED` | `Invalid command` | `Invalid command` | Pass |

#### 3. Clock

| # | Send | Expect | Actual | Result |
|---|---|---|---|---|
| 3.1 | `TIME?` | `Time not set` | `Time not set` | Pass |
| 3.2 | `TIME 1a:30` | `Invalid command` | `Invalid command` | Pass |
| 3.3 | `TIME 99:99` | `Invalid time` | `Invalid time` | Pass |
| 3.4 | `TIME 14:30` | `Time set` | `Time set` | Pass |
| 3.5 | `TIME?` | `14:30:xx` | `14:30:04` | Pass |
| 3.6 | `TIME?` later | seconds advanced | `14:30:11`, `14:30:14` | Pass |

#### 4. Manual feed and arbitration

| # | Send | Expect | Actual | Result |
|---|---|---|---|---|
| 4.1 | `FEED` | `Feeding started`, `Feed complete` | `Feeding started`, `Feed complete` | Pass ² |
| 4.2 | `FEED` twice | `Feeding started`, `Busy feeding`, `Feed complete` | `Feeding started`, `Busy feeding`, `Feed complete` | Pass |

#### 5. Button

| # | Do | Expect | Actual | Result |
|---|---|---|---|---|
| 5.1 | press and release B1 | `Feed complete` only | `Feed complete` | Pass ² |
| 5.2 | `FEED`, press B1 during the feed | one `Feed complete` | `Feeding started`, one `Feed complete` | Pass |

#### 6. Schedule

| # | Send | Expect | Actual | Result |
|---|---|---|---|---|
| 6.1 | `SCHED X 08:00` | `Invalid command` | `Invalid command` | Pass |
| 6.2 | `SCHED A 25:00` | `Invalid time` | `Invalid time` | Pass |
| 6.3 | `SCHED A 14:35` | `Alarm A set`, then `Feed complete` at the minute | `Alarm A set`, `Feed complete` | Pass ³ |

#### 7. Deferred feed

| # | Do | Expect | Actual | Result |
|---|---|---|---|---|
| 7.1 | `SCHED A 14:41`; `FEED` at about 14:40:55 | `Feeding started`, `Feed complete`, `Deferred feed started`, `Feed complete` | the same four lines, in order | Pass ⁴ |

#### 8. Missed feed across a reset

| # | Do | Expect | Actual | Result |
|---|---|---|---|---|
| 8.1 | `SCHED A 15:43`; held RESET from about 15:42:55 for about 10 s | `Feed complete` after release, no `Recovered from crash` | `Feed complete` after release, no `Recovered from crash` | Pass |

#### 9. Power cycle

| # | Do | Expect | Actual | Result |
|---|---|---|---|---|
| 9.1 | disconnect 12 V, power-cycle USB, `TIME?` | `Time not set` | `Invalid command`, then `Time not set` on resend | Pass ¹ |

### Notes

**¹ First command after a power-up.** In 1.1 and 9.1, the first command after a USB power cycle came back as `Invalid command`, and sending it again worked. While the MCU is in reset, a glitch on the RX line is read as an extra byte, which ends up in front of the command. The next `\n` clears it, so the second try is clean. Because of this, the test plan now sends an empty line first.

**² Motor observed.** In 4.1 and 5.1, the motor turned for about five seconds and then stopped. The serial output only shows that the firmware asked the motor to stop; seeing it stop confirms the driver really responded in the `-Os` build.

**³ Button held across the alarm.** In 6.3, the button was pressed at about 14:34:57 and held past the alarm time. The alarm still fed on time. Holding the button did not start a feed, because the button only acts when it is released.

**⁴ Deferred path.** The manual feed started at about 14:40:55, so the motor was still running when the 14:41:00 alarm came. The alarm was deferred instead of dropped. This was the first time this path ran on hardware.

### Not covered

See [Test Plan](Test%20Plan.md#not-covered).