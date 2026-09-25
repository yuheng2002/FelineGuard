# FelineGuard — Release Build Test Plan

A manual test procedure against `Protocol.md`, run once per firmware version: v1 (superloop, `main` branch) and v2 (FreeRTOS, `freertos` branch). Both must pass the same list — the protocol is the contract, not the implementation.

This checklist is also the specification for a future automated test script.


## Setup

### Build
git checkout main          # or: git checkout freertos
make clean
make RELEASE=1
```

Output: `build/release/FelineGuard.elf`. This is the same binary CI builds, so what is tested here is what CI produces.

### Flash

Close any CubeIDE debug session first — only one program can hold the ST-LINK. Then, in PowerShell:

```powershell
$PLUGINS = "F:\STM32CubeIDE_2.1.1\STM32CubeIDE\plugins"
$prog = (Resolve-Path "$PLUGINS\*cubeprogrammer*\tools\bin" | Select-Object -Last 1).Path
$env:PATH = "$prog;$env:PATH"
STM32_Programmer_CLI -c port=SWD -w build/release/FelineGuard.elf -v -rst
```

`-c port=SWD` connects through the ST-LINK, `-w` writes the file, `-v` verifies it, `-rst` resets the chip afterwards.

### Serial

115200 8N1, line ending **LF**.

After opening the port, send one empty line before the first command. During reset the USART pins are not yet configured, and the glitch on the line can be received as garbage byte; the empty line flushes it. Without this, the first command after a power-up (e.g. a `PING`) comes back `Invalid command`.

### Start from a known state

Disconnect the 12 V motor supply first, then unplug the USB cable and plug it back in. This drops the backup domain, so the clock and both alarms are cleared. Reconnect 12 V afterwards if the motor is to be observed. Every run starts here.

Only the start of the run and test 9 need a power cycle, so the motor can stay connected for everything in between.

## Tests

Mark each line Pass or Fail for both versions.

### 1. Basic protocol

| # | Send | Expect | v1 | v2 |
|---|---|---|---|---|
| 1.1 | `PING` | `System ready` | | |
| 1.2 | `HELLO` | `Invalid command` | | |
| 1.3 | empty line | nothing | | |
| 1.4 | `SCHEDULEAVERYLONGCOMMANDLINE` | nothing — an over-long line is discarded silently | | |

### 2. Debug commands are compiled out

| # | Send | Expect | v1 | v2 |
|---|---|---|---|---|
| 2.1 | `CRASH` | `Invalid command` | | |
| 2.2 | `CRASHFEED` | `Invalid command` | | |

Release only. In a Debug build these two reset the chip.

### 3. Clock

| # | Send | Expect | v1 | v2 |
|---|---|---|---|---|
| 3.1 | `TIME?` | `Time not set` | | |
| 3.2 | `TIME 1a:30` | `Invalid command` | | |
| 3.3 | `TIME 99:99` | `Invalid time` | | |
| 3.4 | `TIME 14:30` | `Time set` | | |
| 3.5 | `TIME?` | `14:30:xx` | | |
| 3.6 | `TIME?` again about 10 s later | seconds have advanced | | |

### 4. Manual feed and arbitration

| # | Send | Expect | v1 | v2 |
|---|---|---|---|---|
| 4.1 | `FEED` | `Feeding started`, then `Feed complete` about 5s later | | |
| 4.2 | `FEED`, then `FEED` again immediately | `Feeding started`, `Busy feeding` right away (not after 5s), then one `Feed complete` | | |

`Feed complete` arriving at all is the key Release check here. If optimisation had broken the timeout path, the feed would never end and this line would never come.

### 5. Button

Needs a hand on the board.

| # | Do | Expect | v1 | v2 |
|---|---|---|---|---|
| 5.1 | press and release B1 | nothing at first, then `Feed complete` about 5s later | | |
| 5.2 | send `FEED`, then press and release B1 during the feed | `Feeding started`, then a single `Feed complete` — the press is dropped silently | | |

### 6. Schedule

| # | Send | Expect | v1 | v2 |
|---|---|---|---|---|
| 6.1 | `SCHED X 08:00` | `Invalid command` | | |
| 6.2 | `SCHED A 25:00` | `Invalid time` | | |
| 6.3 | `TIME?` to read the time, then `SCHED A` set to the next whole minute | `Alarm A set`, then `Feed complete` about 5s after the minute turns | | |

A scheduled feed prints no `Feeding started` — the schedule has no return path.

### 7. Deferred feed

The first time this path gets tested. The alarm has to fire while a manual feed is running.

1. `TIME?` to read the time, then `SCHED A` set to the next whole minute → `Alarm A set`
2. Keep sending `TIME?` until the seconds read about `:56`
3. Send `FEED`

Expect, in this order:

```
Feeding started
Feed complete
Deferred feed started
Feed complete
```

| # | Result | v1 | v2 |
|---|---|---|---|
| 7.1 | all four lines, in order | | |

If only one `Feed complete` appears, the timing missed: `FEED` went in too early and ended before the alarm. If `FEED` returns `Busy feeding`, it went in too late and the alarm started first. Retry on a new minute.

### 8. Missed feed across a reset

1. `TIME?`, then `SCHED A` set to the next whole minute
2. About 5s before the minute, press and **hold** the black RESET button
3. Keep holding until about 5s after the minute, then release

| # | Expect | v1 | v2 |
|---|---|---|---|
| 8.1 | `Feed complete` within about 5s of release, and no `Recovered from crash` | | |

The alarm fired while the chip was held in reset; the flag survived in the backup domain and the first pass after boot picked it up. A reset from the button is not a watchdog reset, so there is no recovery message.

### 9. Power cycle

| # | Do | Expect | v1 | v2 |
|---|---|---|---|---|
| 9.1 | disconnect 12 V, unplug USB, plug back in, reopen the serial port, send an empty line, then `TIME?` | `Time not set` | | |

---

## Not covered

- **Watchdog recovery.** A Release build has no way to trigger the watchdog on demand, by design: `CRASH` and `CRASHFEED` are compiled out because a command that hangs the device does not belong in production firmware. The watchdog was verified on the Debug build, where the IWDG code is the same. 
- **The motor physically stopping**, unless the motor is connected. `Feed complete` confirms the firmware stopped it, not that the shaft stopped turning.