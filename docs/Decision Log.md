# FelineGuard — Decision Log

Why the system is built the way it is. Each entry states the decision, the alternative that was considered, and the reason for choosing between them.

This is the shortest of the three documents and the one to read first. `Protocol.md` specifies the rules; `Journal.md` records how they were arrived at, day by day.

---

## Scope

**Firmware first.** The goal of this rewrite is the firmware: layered, robust, and maintainable. The mechanical build already exists from an earlier version and is reused as it is. Enclosure work, a vision-based feeding mode and an IoT interface are all possible later; none of them would have made the firmware better.

**HAL instead of register-level drivers.** An earlier version of this project used register-level GPIO, timer, UART and watchdog drivers written against RM0390. The focus this time is layered architecture rather than peripheral bring-up, so this version calls the ST HAL — with the condition that for any HAL function used, I should be able to say which registers it writes. That mattered in practice: several entries below came from reading the HAL source and finding behavior the API did not describe.

**CubeIDE for scaffolding, no CubeMX configuration.** The IDE provides the startup file, linker script, CMSIS headers and HAL sources. Peripheral initialization is written by hand. Assembling the scaffolding manually would take time without teaching much, while generated initialization code would be code I had not written.

**SPI display deferred.** The original plan included an SPI status display. It adds no core functionality and has no coupling to anything else, so it can be added later without revisiting any decision here.

## The feed model

### One serving is the unit for every feed

Every feed dispenses the same fixed amount, whatever triggered it.

The alternative was to let each source define its own amount. One unit means there is a single action to arbitrate, instead of three behaviors to reconcile.

### One button press dispenses one serving

The alternative was hold-to-run: the motor turns while the button is held.

Hold-to-run makes the button different in kind from the other two sources, which are one-shot requests, and it needs guard conditions to keep the manual path from interfering with a feed already running. The commercial feeder I own uses one press, one portion. Adopting that makes all three sources uniform and removes the need for the guards entirely.

### Three sources, one arbitration rule

The motor is a single physical resource, so a feed can only start when no feed is running. The same rule covers all three sources.

`FEED` and the button are dropped when busy. A scheduled feed is deferred instead, because it is the only source with nobody present to send it again.

### Feedback belongs to the source, not to the rule

A dropped `FEED` is answered with `"Busy feeding"`; a dropped button press is silently ignored.

That is not an inconsistency in the rule. The rule is identical for all three sources — what differs is whether the source has a return path. UART does, the button does not. Giving the button feedback would mean adding an output channel, which is a new feature rather than a change to arbitration.

## Command protocol

### Line framing rather than a length prefix

Single-byte commands work only while no command needs an argument, and setting the clock needs one. Two ways to carry an argument:

- **The command byte says how many bytes follow.** If a byte is lost, the parser waits forever, so this needs an inter-byte timeout. There is no clean value for that timeout: it has to be longer than the gap inside one command and shorter than the gap between two, and those constraints conflict as soon as commands are typed by hand.
- **A line ending in `\n`.** Framing does not depend on timing at all, and a partial line is discarded at the next terminator, so the protocol resynchronises on its own.

Line framing also gives unknown input a sensible meaning. `FF` is one line and one unrecognized command, answered with `"Invalid command"`. Under a byte protocol the same input would be two commands — the first accepted, the second dropped as busy — which is not how a command is supposed to work.

### Fixed-width, zero-padded arguments

`08:00` is valid, `8:0` is not.

Fixed width means arguments are read at fixed offsets: no tokenizer, no `sscanf`, and no variable-length parsing to get wrong. The constraint costs the host nothing.

### Oversized lines are discarded through to the end of the line

Not just the overflowing bytes. Resuming immediately after the overflow would let the tail of an over-long line be parsed as a new command — a garbage line could produce a valid one.

### `TIME` and `SCHED` are separate commands, in any order

One command doing both would do two unrelated things. Keeping them separate also means a `SCHED` sent before the clock is set is simply stored, and takes effect once `TIME` arrives.

### Two feeding times, held directly in the RTC alarm registers

The RTC has exactly two alarms. Supporting more would mean keeping a schedule table somewhere, arming only the next feed, and re-arming after each one. Fixing the limit at two lets each feeding time live in `ALRMAR` or `ALRMBR`, and with the date fields masked each alarm repeats daily with no re-arming at all. Two feeds a day is also what my own cat gets.

The host names the slot — `SCHED A 08:00` — rather than the firmware keeping the two most recent times. With an implicit scheme, a new time replaces whichever slot was written longest ago, **which is not necessarily the one the owner meant to change; editing one feeding time would mean re-entering both.** It would also need state to track which slot is next in line, and that state would have the same persistence problem as the alarms themselves.

### Two levels of rejection

Originally every failure returned `"Invalid command"`. It became unclear once commands took arguments : `TIME 1a:30` and `TIME 99:99` fail for different reasons and need different corrections. The first is a syntax problem, the second a value problem, so the second gets `"Invalid time"`.

The test for whether a distinction is worth making is whether it changes what the reader does next. These two do. Splitting further — a bad length versus a bad character — would not, since both mean "check the format".

There is a third failure the host is not told about: the driver also returns false when the underlying HAL call fails, and that is folded into `"Invalid time"`. Separating it would mean returning a status enum instead of a bool. I chose not to, because there is nothing the user could do with the distinction. Once the LSE is running, a HAL timeout on the RTC most likely means the peripheral or the crystal is broken — a warranty call, not a retry. A production device would want a distinct fault indication somewhere (e.g. flashing LED...), but not in the message the owner reads.

### `TIME?` as a query

Testing the set commands exposed a gap: I could confirm that `TIME 14:30` returned `"Time set"`, but not that the calendar actually held 14:30. Without a read-back there is nothing to assert on except the acknowledgement. I shouldn't have to use a debugger and check the register values all the time, not to mention the customers.

The trailing `?` for a query comes from SCPI, which I use between host and MCU on a project at work. It keeps the two commands distinct in the text itself rather than relying on whether an argument is present, and it extends if `SCHED A?` is ever wanted.

Setting takes `hh:mm` and implies `:00`; the query returns `hh:mm:ss`. The asymmetry is deliberate — the owner should not have to type seconds, but reading them is the most direct way to see the calendar advancing.

### `Q` is not part of the protocol

`Q` quits the host script. It never reaches the device and the firmware has no concept of it, so it belongs to the host script's documentation, not here.

### Debug commands do not change the protocol

`CRASH` and `CRASHFEED` exist so that watchdog recovery can be demonstrated on demand, and they are compiled out of a release build.

Because they are meant to be removable, they must not change how anything else behaves: they are not feed requests, they do not appear in the arbitration table, and no field or rule exists for their sake. Where they need cooperation — not being sent during a feed — that is stated as a host-side expectation rather than enforced in firmware.

### `CRASHFEED` is verified by watching the motor

The alternative was for the firmware to report, on the next boot, that a feed had been interrupted. That requires a separate in-progress marker in persistent storage — real complexity added for the sake of a test command.

The command tests one physical outcome: that the motor is stopped after a crash during motion. Watching the motor answers that directly.

## Reliability

### The main loop stops the motor, not the timer interrupt

A timer bounds each feed. When it expires, the interrupt sets a flag and the main loop stops the motor. The motor driver itself only starts and stops the PWM and has no concept of duration.

The reason is not that the interrupt would be too slow or too long. It is that **interrupts keep firing while the main loop is hung** — the CPU is still executing, just stuck. If the interrupt stopped the motor, a system that had already stopped running its main loop would still finish the feed cleanly and look healthy from the outside.

Leaving it to the main loop makes `"Feed complete"` mean something stronger: the code the watchdog supervises is still alive. The same reasoning puts the watchdog refresh in the main loop and nowhere else — the refresh *is* the evidence.

### Missed feeds are detected from the alarm flag, not a stored timestamp

The alternative was to record the time of the last feed and compare it against the current time at startup. That fails in exactly the case it exists for: the RTC calendar is lost when VDD is removed, so after a power cut the device does not know the current time and cannot make the comparison.

The alarm flag is set by hardware, cleared only by software, and lives in the backup domain.

Two properties fall out of the hardware rather than being enforced in code:

- **No dedicated startup path is needed.** The flag is a level, not a pulse, so the first ordinary pass of the main loop reads it like any other alarm.
- **At most one serving is made up.** The flag is one bit, not a counter, so missing two alarms is indistinguishable from missing one.

After a power cycle, the calendar is uninitialised, so scheduled feeding suspends itself until `TIME` arrives and no catch-up feed happens at all. The commercial feeder behaves the same way.

### The alarm flag is cleared after the feed, not before

Clearing it first opens a window between the clear and the feed actually starting; a reset landing there loses the feed entirely, with nothing left to record that it was due. Clearing it after means a reset between the feed and the clear repeats it.

Neither ordering is free. The choice follows the rule below.

In a repeating reset loop this repeats on every boot, which is accepted: a reset loop is a fault the watchdog cannot clear by itself and needs the owner in any case.

### Over-feeding is the direction to fail in

The rule behind several decisions above.

An extra serving is recoverable — the owner sees a full bowl and knows. A missed one is not, and nobody finds out. Wherever a failure mode had to land on one side or the other, it lands on the recoverable one.

### At most one missed feed is made up

The firmware cannot know whether the owner fed the cat by hand during an outage, so replaying every missed feeding time could empty the hopper into the bowl.

One serving keeps the cat from waiting until the next scheduled time, and the overshoot stays bounded at a single serving.

### Watchdog timeout is about one second

It was set to two seconds at one point to cover a Flash sector erase. Flash was subsequently dropped from the design, and nothing else in the firmware blocks for more than a few milliseconds.

**The timeout is therefore set by how quickly the device should recover from a hang, not by any operation it has to tolerate.**

## Storage

### No Flash persistence

Three uses were considered and each was dropped:

- **A feed log.** Writing one is not the hard part — every feed passes through the same completion point, so all three sources could be recorded there. The problem is the timestamps. A power cut resets the calendar to zero, so entries written after it **are on a different time base** than entries written before, and comparing the two means nothing. Half the log would be unreadable in a way its reader could not detect, which is worse than having no log at all.
- **Missed-feed detection.** Replaced by the alarm flag, which is simpler and works in the one case a stored timestamp could not.
- **Configuration constants.** The linker already places compile-time constants in Flash. Reserving fixed Flash pages is what you do when two separately built binaries have to agree on where parameters live — the situation in the bootloader project at work, and not the situation here.

**The schedule is not persisted either.** It lives in the alarm registers, which have exactly the same lifetime as the clock: both survive a reset, both are lost on a power cut. Since the owner has to set the clock again after a power cut and is already connected to send that command, sending the schedule again costs almost nothing.

**Dropping Flash also removed the longest blocking operation in the system, which is why the watchdog timeout could come down.**

## Hardware

### The 12 V rail is isolated by wiring, not by procedure

An early STM32F446RE board was destroyed when the 12 V motor supply was connected while the logic side was unpowered.

The proximate cause is overvoltage on a pin rated for 3.3 to 5 V — not the order the supplies were applied in. An unpowered chip is not a safe chip: with VDD at 0 V there is no supply to absorb injected current, so the same fault does more damage. The autopsy cleared the A4988 of any path from VMOT to its logic side, which leaves the shared breadboard rails as the likely cause.

The rule is therefore that the 12 V rail never shares a breadboard rail with 3.3 V or 5 V. In the current wiring neither supply goes through a rail at all — both connect directly to their pins, so there is no node on the board where the two could meet. The two domains share only a ground, which the A4988 needs in order to read the STEP and DIR levels.

Powering up in a set order is kept as a habit, but it is not the safeguard. A safeguard that depends on a human remembering a sequence is not one.

A single supply — 12 V in, logic derived through a regulator — would remove the ordering question entirely. It was not adopted because development runs on USB power and uses the ST-LINK for flashing, which reintroduces a second supply and needs board jumper changes to avoid back-feeding.

### The A4988 is disabled between feeds

The `EN` input is driven high while idle, so the coils are de-energised.

Left enabled, a stepper holds position by keeping both coils energised. That dissipates power continuously and makes an audible whine, and **a cat feeder is idle for well over 99% of its life** — the holding torque buys nothing while the heat is constant.

> Known limitation: between power-on and `MOTOR_Init()` the pin floats and the
> A4988's internal pull-down enables the driver. In the documented power-up order, this is harmless — VMOT is not connected yet, so nothing can be energised. It would only matter if the 12 V supply were already present when the MCU comes up, which the wiring rule above already rules out. An external pull-up would close the window properly, but that means a 3.3 V rail on the breadboard.

## Implementation

### No GPIO driver

The plan was a `GPIO_CTRL` module wrapping init, read and write. The HAL already provides all three, so the module would have been an indirection with no actual logic inside it.

What is genuinely board-specific is which pin each signal is on. That lives in `board.h`, a header of macros with no `.c` file. Each driver configures its own pins from those macros, so the pin map has one home and every module still carries the setup it needs.

The test this came from — does the module own any state, or make any decision? — is the same one that kept `IWDG_CTRL` (it owns the reset-cause query) and `TIMER` (it owns the tick accounting).

### Executive may call the HAL directly for system bring-up

`HAL_Init()` configures the flash prefetch, the SysTick tick and the HAL MSP. It belongs to no peripheral, so no driver can own it, and a single init function that knew the startup order of every module would throw a stronger dependency.

The Executive therefore calls the HAL directly, but only for system bring-up. Anything that drives the feeder goes through the application layer.

### A ring buffer between UART_CTRL and Comms

Unlike I2C, which has START and STOP conditions to delimit a transaction, UART delivers a byte stream with no frame boundaries — the boundary has to be defined by the protocol. `UART_CTRL` takes in raw bytes; `Comms` assembles them into a command line.

The two run at different rates. `UART_CTRL` is interrupt-driven, `Comms` is polled in the main loop, so if the host sends two commands back to back the bytes of the second arrive while the first is still being processed and have nowhere to go.

**Requiring the host to wait between commands would work, but it puts correctness in the hands of the other end, where the firmware can neither verify nor enforce it.** A ring buffer decouples the two rates and depends on nothing outside the device.

**Indices instead of a size counter.** The buffer is sized one element larger (K + 1) than its capacity (K) so that full and empty can be told apart from head and tail alone. The alternative — a `size` field — would be **written by both the ISR and the main loop,** and **`size++` is a read-modify-write that can interleave. `volatile` does not fix that; only a critical section or an atomic would.** Designing the shared write away is cheaper than either, and it is checkable by inspection: **each index has exactly one writer.**
