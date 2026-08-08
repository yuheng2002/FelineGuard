# FelineGuard — Decision Log

The design decisions behind the protocol, including the options that were
considered and dropped. Written during design, before implementation.


## The Feed Model

### One serving is the unit for every feed
Every feed dispenses the same fixed amount, whatever triggered it. The
alternative was to let each source define its own amount. Keeping one unit means
there is one action to arbitrate instead of three behaviours to reconcile.

### Button: one press dispenses one serving
Two options for the button:

- Hold to run. The motor turns while the button is held.
- One press, one serving, regardless of how long the button is held.

Hold to run makes the button different in kind from the other two sources, which
are one-shot requests. It also needs guard conditions to stop the manual path from
interfering with a feed already running. The commercial feeder I own uses one
press, one portion. That choice makes all three sources uniform and the guards
unnecessary.

### Three sources, one arbitration rule
The motor is a single physical resource, so a feed can only start when no feed is
running. The same rule covers all three sources, and it does not depend on whether
the source can be answered. `FEED` and the button are dropped when busy. A
scheduled feed is deferred instead, because it is the only source with nobody
present to send it again.

### Feedback belongs to the source, not to the rule
A dropped `FEED` gets `"Busy feeding"`, because UART has a return path to the
host. A dropped button press is ignored silently, because the button has none.
Giving the button feedback would need a separate output channel, such as the
status display. That is a new feature, not a change to the rule.

## Command Protocol

### Line framing
Single-byte commands work only while no command needs an argument, and setting the
clock needs one. Two ways to carry an argument:

- The command byte says how many argument bytes follow. If a byte is lost, the
  parser waits forever, so this needs an inter-byte timeout. There is no clean
  value for that timeout: it must be longer than the gap inside one command and
  shorter than the gap between two commands, and those two conflict once commands
  are typed by hand.
- A line ending in `\n`. Framing does not depend on timing at all. A partial line
  is dropped at the next terminator, so the protocol recovers by itself.

Line framing was chosen. It also gives unknown input a sensible meaning: `FF` is
one line and one unknown command, answered with `"Invalid command"`. Under a byte
protocol the same input would be two commands, the first accepted and the second
dropped as busy, which is not how a command should behave.

### Fixed-width, zero-padded arguments
`08:00` is valid, `8:0` is not. Fixed width means the argument is read at fixed
offsets, so no tokenizer and no `sscanf`. The constraint costs the host nothing.

### Oversized lines are dropped to the end of the line
If a line does not fit the receive buffer, it is dropped, and every byte after it
is dropped until the next `\n`. Resuming right after the overflow would let the
tail of an oversized line be parsed as a new command.

### `TIME` and `SCHED` are separate commands
One command doing both would do two unrelated things. They are separate, and the
order does not matter. Setting the clock does not trigger a feed whose time has
already passed, so the owner can skip the next scheduled feed by setting the clock
to just after it. That is useful, not a limitation.

### Two feeding times, held in the RTC alarm registers
The RTC has exactly two alarms. More feeds would mean keeping a table somewhere,
arming only the next feed, and re-arming after each one. Fixing the limit at two
lets each feeding time live directly in `ALRMAR` or `ALRMBR`. With the date fields
masked, each alarm repeats daily and never needs re-arming. Two feeds a day is also
what my own cat gets.

### `Q` is not part of the protocol
`Q` quits the host script. It never reaches the device and the firmware has no
concept of it, so it belongs to the host script documentation.

### Debug commands do not change the protocol
`CRASH` and `CRASHFEED` exist so watchdog recovery can be shown on demand, and
they would be compiled out of a release build. Because they are meant to be
removable, they must not change how anything else behaves: they are not feed
requests, they are not in the arbitration table, and no field or rule exists for
their sake. Where they need cooperation, which is not being sent during a feed,
that is a host-side expectation rather than something the firmware enforces.

### `CRASHFEED` is checked by watching the motor
The other option was for the firmware to report on the next boot that a feed had
been interrupted. That needs a separate in-progress marker in persistent storage,
which is complexity added for a test command. The command tests one physical
outcome, that the motor is stopped after a crash during motion, so watching the
motor is enough.

## Reliability

### The main loop stops the motor, not the timer interrupt
The motor driver only starts and stops the PWM and has no concept of duration. A
timer bounds the feed. When it expires, the interrupt sets a flag and the main loop
stops the motor.

The reason is not interrupt length. Interrupts keep running while the main loop is
hung. If the interrupt stopped the motor, a system that had already stopped running
its main loop would still finish the feed cleanly, and the feed would look
successful. Leaving it to the main loop makes "feed complete" mean that the code
the watchdog supervises is still alive.

### Missed feeds are detected from the alarm flag, not a stored timestamp
The other option was to store the time of the last feed and compare it with the
current time at startup. That fails in the case it was meant for: the RTC calendar
is lost when VDD is removed, so after a power cut the device does not know the
current time and cannot make the comparison.

The alarm flag is set by hardware and cleared only by software, and it lives in the
backup domain, so it survives a reset. If the flag is set at startup, an alarm fired
while the firmware was not running. It is one bit and not a counter, so at most one
serving is dispensed however many alarms were missed. This needs no persistent
storage at all.

After a power cut the calendar is uninitialised, so scheduled feeding is suspended
until `TIME` arrives and no catch-up feed happens. The commercial feeder behaves the
same way: the clock has to be set again after a power cut.

### The alarm flag is cleared after the feed, not before
Clearing it first loses the feed if the device crashes during it. Clearing it after
repeats the feed if the device resets between the feed and the clear. The second is
preferred: an extra serving can be tolerated, a missed one cannot. In a repeating
reset loop this repeats on every boot, which is accepted, because a reset loop needs
the owner anyway.

### Over-feeding is the direction to fail in
The rule behind several decisions above. A cat fed twice is slightly overfed. A cat
not fed has nothing.

### At most one missed feed is made up
The firmware cannot know whether the owner fed the cat by hand during an outage, so
replaying every missed feeding time could empty the hopper into the bowl. One
serving keeps the cat from waiting until the next scheduled time, and the overshoot
is bounded.

### Watchdog timeout is about 1 second
It was set to 2s at one point to cover a Flash sector erase. Flash was then
dropped, and nothing else in the firmware blocks for more than a few milliseconds,
so the timeout is now set by how fast the device should recover from a hang rather
than by any operation it has to tolerate.

## Storage

### No Flash persistence
Three uses were considered and dropped:

- **A feed log.** It would record scheduled feeds only, so it could not answer
  "when was the cat last fed". A log that is incomplete in a way its reader cannot
  see is worse than no log.
- **Missed-feed detection.** Replaced by the alarm flag, which is simpler and works
  in the case a stored timestamp could not.
- **Configuration constants.** The linker already places compile-time constants in
  Flash. Reserving fixed Flash pages is what you do when two separately built
  binaries have to agree on where parameters live. This is a single application and
  has no such constraint.

The schedule is not persisted either. It lives in the alarm registers, which have
the same lifetime as the clock: both survive a reset and are lost on a power cut.
The owner has to set the clock again after a power cut anyway, and is already
connected in order to send that command, so sending the schedule again costs almost
nothing.

Dropping Flash also removed the longest blocking operation in the system, which is
why the watchdog timeout could come down.

## Hardware

### The 12V rail is isolated by wiring discipline
An early board was destroyed when the 12V motor supply was connected while the
logic side was unpowered. The proximate cause is overvoltage on a pin rated for
3.3 to 5V, not the order the supplies were applied in; an unpowered chip is not a
safe chip. The post-mortem cleared the A4988 of a path from VMOT to its logic side,
which leaves the shared breadboard rails as the likely route.

The rule is that the 12V rail never shares a breadboard power rail with 3.3V or
5V. The two domains share only a ground, which the A4988 needs in order to read
the STEP and DIR levels. Powering up in a set order is kept as a habit, but it is
not the safeguard.

A single supply, 12V in with logic derived through a regulator, would remove the
ordering question entirely. It was not adopted because development runs on USB
power and uses the ST-LINK for flashing, which brings back a second supply and
needs board jumper changes to avoid back-feeding.

## Implementation

### No GPIO driver
The plan was a GPIO_CTRL module wrapping init, read and write. The HAL already
provides all of that, so wrapping it would be an indirection with nothing in it.

What is left is board knowledge: which pin each signal is on. That lives in
`board.h`, a header of macros with no `.c` file. Each driver configures its own
pins from those macros, so the pin map has one home and every module still
carries the setup it needs.

### Executive may call the HAL directly for system bring-up
The Executive calls the HAL directly, but only for system bring-up: 
anything that drives the feeder goes through the application layer.

### Use a ring buffer between UART_CTRL and Comms
Unlike I2C, which has START/STOP conditions to delimit one complete transaction, UART delivers a byte stream with no frame boundaries, so the boundary has to be defined by the protocol -- this one uses `\n` to end a command. UART_CTRL takes in raw bytes, and Comms polls them and assembles them into a command line.

The two run at different rates: UART_CTRL is interrupt-driven, Comms is polled in the main loop. If the host sends two commands back to back, say a `FEED` followed by a `PING`, the bytes of the second one arrive while the first is still being processed and have nowhere to go.

Requiring the host to wait between commands would work, but it puts correctness in the hands of the other end. A ring buffer decouples the two rates instead and depends on nothing outside the device. It is also a chance to implement one properly.