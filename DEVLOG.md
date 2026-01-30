# FelineGuard Engineering Log

Here is the polished version. I have kept your personal tone, your reasoning process, and your specific examples (like the cat feeder reliability). I also clarified the logic regarding *why* the ISR triggers when it does, to fix the confusion you mentioned in point #3.

---

# 2026-01-29: IWDG Implementation & Hardware Timer to replace Software Delay

### Why I did this

I wanted to ensure the system can reboot itself in case of a software failure. This is critical for safeguarding my cat. If I am gone for a few days, I trust my feeder to take care of her, but if it fails due to a power outage, I can’t blame the code. However, if it fails due to a system crash caused by bad programming, that is unacceptable.

### 1. Independent Watchdog (IWDG) Complexity

I learned that although the IWDG has fewer registers than most peripherals, its protocol is quite specific. You have to configure the **Key Register (KR)**, **Prescaler (PR)**, and **Reload Register (RLR)**.

* **The Locking Mechanism:** The KR is "locked" by default. I first need to write `0x5555` (the access code) to it. Only then can I write to the PR and RLR to configure the timing.
* **The "Feeding" Logic:** The core mechanism is essentially a down-counter. Once I set the counter, I must regularly write a reload value to the KR to "feed" the dog. If I don't feed it in time, the counter hits zero and hard-resets the entire system.
* **Independent Clock:** Unlike other peripherals, the RCC (Reset and Clock Control) does not enable the IWDG's clock. It runs on its own internal 32MHz clock (LSI). This makes perfect sense: you wouldn't want the safety system that reboots your MCU to rely on the same clocks that might have caused the crash in the first place.
* **Activation:** As long as the MCU is powered, writing the access keys activates the IWDG. The final step is to "start the engine" by writing `0xCCCC` to the KR.

**How do I know if the dog bit me?**
If the system crashes and the IWDG reboots the board, I can't exactly serial print a "System Crashing..." message *during* the crash. The solution lies in the **RCC Control & Status Register (CSR)**. Even though the system resets, the hardware flags the *source* of the reset in this register. So, inside my setup function (which runs immediately after a reboot), I check this "mailbox." If the IWDG flag is set, I know a watchdog reset occurred.

**The Math:**
`IWDG_Frequency = 32kHz / Prescaler` (assuming LSI is 32kHz, typically, not 32MHz, but the logic holds).
If I set the Prescaler to 32, I get a 1kHz frequency (1 tick/ms). To get a 1-second timeout, I set the Reload Register (RLR) to 1000. This ensures the watchdog checks the system every second.

### 2. The Problem with Software Delay

Previously, I used a `software_delay(2000000)` loop to keep the motor running for a fixed time. This created two major problems:

1. **Blocking:** It stops the CPU from doing anything else during that interval. The timing is also imprecise; it might take the CPU a different amount of time to run the loop depending on interrupts or optimizations. This might not matter for a cat feeder, but for a car or a rocket, precise timing is crucial.
2. **Starving the Watchdog:** Because the delay blocks the CPU for ~2 seconds, the code cannot return to the top of the `while(1)` loop to feed the dog. The IWDG times out (1s limit), resets the system, and this repeats infinitely, causing a crash loop.

### 3. The Solution: Hardware Timer + ISR

I decided to replace the software delay with a hardware timer. However, simply *polling* a hardware timer is still blocking logic—the CPU would still have to sit there waiting for the flag to change. I needed a non-blocking approach: **Hardware Timer + Interrupt Service Routine (ISR).**

**Choosing the Timer:**
I had been using TIM2 (General Purpose) for PWM, but I discovered simpler "Basic Timers" (TIM6 & TIM7). Their register maps are much smaller, but ST Microelectronics designed them with the same offsets for common registers (like PSC and ARR) as the advanced timers. This meant I could reuse my existing `TIM_RegDef_t` struct, though I should probably refactor this later for safety.

**The "Vector Table" & Naming Convention:**
I ran into a tricky issue with the ISR function name. I learned that in the startup assembly file (`.s`), there is a pre-defined **Vector Table** containing the addresses of all interrupt handlers. The names are strict acronyms.

* I initially tried naming my function `TIM6_IRQHandler` or random names, but the code wouldn't jump to it.
* I checked the vector table and found the specific name required is `TIM6_DAC_IRQHandler`.

**The "Wrong Timer" Bug:**
I also had a funny bug where I copy-pasted code from my attempts with TIM3. Inside the `TIM6_DAC_IRQHandler`, I wrote `CLEAR_BIT(TIM3->SR, 0)`.
Because I wasn't clearing the *correct* status register (TIM6->SR), the CPU thought the interrupt was still pending. The moment it exited the ISR, it jumped right back in, creating an infinite loop.

### 4. The Logic Flow (Event-Driven)

Transitioning to event-driven programming was logically tedious to figure out, but here is the final workflow:

1. **The Trigger:** In the main loop, when the STM32 receives the byte 'F', it turns on the Motor and LED2. Immediately after, it enables TIM6 (by setting the CEN bit in CR1).
2. **The Background Task:** We enabled the Update Interrupt (DIER) during setup. Once we set CEN (Counter Enable), TIM6 starts counting from 0 up to the ARR value (2 seconds worth of ticks). The CPU is now free to go back to the main loop to feed the watchdog and listen for other commands.
3. **The Event:** When TIM6 reaches the ARR value, the hardware sets the **Update Interrupt Flag (UIF)** in the Status Register (SR) to 1.
4. **The ISR:** The hardware sees the flag and jumps to `TIM6_DAC_IRQHandler`.
* **Logic Check:** It sees the update event occurred (so 2 seconds have passed).
* **Action:** It turns OFF the Motor and LED.
* **Cleanup:** It clears the SR flag (to prevent the loop bug mentioned above) and disables TIM6 (clears CEN). We disable the timer so it doesn't keep reloading and triggering interrupts every 2 seconds forever.



### Conclusion

Figuring out this logic was complicated, but I learned a lot about how the hardware coordinates events. I still have one minor bug: after rebooting the board, the *very first* 'F' command prints "Feed Complete" immediately (likely a flag not being cleared during initialization). But aside from that, it works perfectly. I'll save that debugging for another day.

# 2026-01-29: USART2 Interrupt Enabled & Python Script Optimization

Following the successful setup of USART2 as both transmitter and receiver for testing (I plan to connect this STM32 board to my ESP32 later for IoT features), I have now enabled the interrupt for USART2 by setting the corresponding bit in the NVIC -> ISER (Interrupt Set-Enable Register).

Although the program behaves the same externally, this is a major optimization. My previous approach was "blocking" (polling), meaning the CPU had to ask "is there new data?" millions of times per second. This wasted CPU resources. Since microcontroller CPUs are single-core and limited in processing power, they shouldn't be tied up checking for flags. By enabling NVIC, the CPU now only collects data when the hardware triggers an interrupt, leaving it free to do other tasks in the meantime.

Here are the key things I learned along the way:

**1. Hardware Architecture: GPIO vs. Internal Peripherals**
When I set up the PC13 User Button interrupt previously, I had to configure three different peripherals: SYSCFG, EXTI, and NVIC. However, for USART2, I only needed to configure the NVIC.

* **Why the difference?** GPIOs are "external" pins provided by the ST Nucleo architecture. The signal comes from outside the chip logic. That’s why they need **SYSCFG** (to route specific pins like PC13 to the correct EXTI line) and **EXTI** (the "External" Interrupt Controller) to act as the outer guard.
* **The Logic:** SYSCFG routes pins based on their number. For example, PC13 uses `SYSCFG->EXTI[4]` because 13 / 4 = index 3 (`EXTI[]` counts starting from 1, and even though the result is 3, there is a remainder, so it does not fit in the first 3 blocks), and the specific bits are determined by 13 % 4.
* **Internal Guards:** USART2 is an internal peripheral. It doesn't need the external EXTI guard; it has a direct line to the **NVIC** (Nested Vectored Interrupt Controller). The NVIC is part of the ARM Cortex-M4 core itself (documented in the PM0214 manual), which is why it handles the final "gatekeeping" for the CPU.
* **IRQ Numbers:** I found the IRQ numbers in the RM0390 Vector Table (EXTI15_10 is Position 40, USART2 is Position 38). It's important to remember: **Priority** is the logical "who goes first" sequence, while **Position** maps to the specific physical bit in the NVIC->ISER register that enables the interrupt.

**2. ISR Best Practices: Keep it Short**
The difference between polling and interrupts is just *when* the CPU notices the task. However, once inside the `USART2_IRQHandler`, the CPU is busy.

* **The Risk:** If I put too much code inside the Interrupt Service Routine (ISR)—processing that takes 1-3 seconds—the CPU cannot handle other interrupts during that time.
* **The Solution:** The ISR should only do the bare minimum (like reading the data to a variable and setting a flag). The heavy processing logic should happen in the `main` loop.
* **Real-world context:** Not all tasks have equal weight. In my Cat Feeder, "feeding" is the critical mission. I don't want a low-priority message receiver to block the high-priority feeding mechanism. In safety-critical systems (like cars), this latency could be the difference between a system reacting in time or failing.

**3. Event-Driven Programming**
I need to remember that Interrupt functions (ISRs) should never be called manually in the `main` loop. That defeats the purpose of event-driven programming.

* **Mechanism:** The ISR is defined in `main.c`, but it is triggered automatically by hardware hardware events.
* **Callback connection:** This reminds me of callback functions I learned in my ECE140A IoT class. While not exactly the same, the philosophy is identical: tell the CPU *what* to do when a specific trigger happens, without forcing the CPU to constantly check for that trigger.

**4. Debugging & Python Logic**
My debugging process taught me the most today.

* **The Infinite Loop Bug:** Initially, I forgot to reset `message = 0` after processing 'F'. This caused the motor to spin endlessly because the `while(1)` loop kept seeing 'F'.
* **The "Buffer" Realization:** I noticed that typing 'H' (Ping) didn't show the endless spam of "Feed Complete" messages that were actually happening in the background. My Python script was only reading one line at a time. This made me realize the importance of `ser.reset_input_buffer()`. Before sending a new command from Python, I now clear the buffer to ensure I'm not reading old "garbage" data or historical responses. This is the trade-off of writing my own Python script versus using a standard Serial Monitor: I have more control, but I have to manage the data flow myself.
* **Handling Disconnects:** I found that unplugging the STM32 didn't trigger my original "Timeout" warning. Instead, it crashed the script with a `PermissionError` (WriteFile failed). This is because the OS handle to the USB port was lost. I added a specific `try/except` block for `serial.SerialException` to catch this scenario, allowing the program to exit gracefully and close resources (`ser.close()`) in the `finally` block.

# 2026-01-28: USART2 Serial Debugging + Python Testing

Following the implementation of my USART2 drivers, I successfully printed simple test messages using the VSCode Serial Monitor extension. I was also able to simulate the "cat feeding" process by sending the 'F' command to the STM32 to trigger the motor.

However, I ran into several issues along the way. Solving them taught me how to optimize hardware behavior to match my expectations—something real developers do constantly. A product shouldn't just "work" once; it needs to be reliable (I plan to add a WatchDog timer later). Here is my debugging process:

### The "Garbage Data" Issue

Initially, I tried printing a simple "welcome message" before the `while(1)` loop. However, I kept receiving 4 random replacement characters every time I flashed the code. I tried adding a software delay to let the system settle and even attempted to clear "invisible" `\r\n` characters, but nothing worked.

I realized this likely isn't a code issue, but rather **hardware electrical jitter** (floating pins during startup). The physical solution would be attaching a pull-up resistor to the TX pin. However, that seems unnecessary for my current prototype. As long as the baud rates match and the PC and STM32 "speak the same language," I can handle this in software.

### Switching to Python

I decided to stop using the pre-packaged Serial Monitor. It felt too much like the Arduino library—an external, encapsulated tool. I wanted to go a level deeper and build my own tool using Python.

Even though I’m still using Python's `serial` and `time` libraries, I now have full control over the communication logic.

At first, my Python script could:

1. Print its own status messages.
2. Send commands (like 'F') to the STM32 to spin the motor.

However, I realized this was essentially **one-way communication**. The script was just "talking at" the STM32 without knowing if the board actually received the command. This wasn't the interactive two-way system I wanted.

### Implementing Two-Way Communication

To fix this, I added `response = ser.readline().decode('utf-8', errors='ignore').strip()` to let the script "listen" to the board. I also implemented connection safeguards:

1. **Startup Logic:** Since I can't easily fix the hardware jitter, I decided to work around it. I accept that garbage data exists at startup. My solution is to wait (`time.sleep()`) and then flush the buffer (`ser.reset_input_buffer()`) to clear the noise before actual communication begins.
2. **Handshake Command ('H'):** Since the Python script and the STM32 board don't always start at the exact same time, I added an 'H' command. This allows me to manually "ping" the board to check if the connection is still alive—very useful if I leave the script running but unplug the STM32 (or if I forget what I did the night before!).

### Conclusion

This process touched on concepts like **Synchronous vs. Asynchronous communication** (and device synchronization). While I'm still learning the nuances, this was a great start.

I really appreciate diving deep into Bare-Metal drivers. It allows me to understand exactly what is happening physically when I control hardware with Embedded C.

---

# 2026-01-28: USART2 Setup & Enhanced Understanding of Volatile

Today, I implemented basic USART drivers and successfully printed my first test message, "Motor System Initialized!", from my STM32 MCU to the VSCode Serial Monitor.

## From Arduino to Bare-Metal
This basic Serial Print might not look impressive on the surface. In my ECE140A IoT course, printing to the Serial Monitor with an ESP32 only takes three steps:
1. Connect the ESP32 to the desktop via cable.
2. Call `Serial.begin(115200);`.
3. Call `Serial.print("--- testing ---");`.

It seems simple, but that is only because someone else already wrote all the drivers in the background to handle the peripherals. By implementing the USART drivers on the STM32 myself, I have finally gained a full, register-level understanding of how the board actually interacts with my PC.

## USART Configuration (The 8-N-1 Protocol)
Like other peripherals, USART has many registers. For my testing purposes, I defined a configuration struct with five key settings: **Mode**, **WordLength**, **Parity**, **StopBits**, and **BaudRate**.

I used the standard **"8-N-1"** protocol:
* **WordLength = 8 bits**: Standard data size (instead of 9 & also because I will not enable Parity check).
* **Parity = None**: I know parity checks are powerful for unstable connections, but since my STM32 is connected via a solid USB cable right next to my desktop, I don't need the extra overhead.
* **StopBits = 1**: I learned that Stop Bits are like letting the receiver "catch their breath" between data chunks. 0.5 would be too fast, and 2 would be too slow/wasteful. The middle value, 1, is the standard.

For the **Mode**, I checked the datasheet and found that **PA2** supports USART2/TX and **PA3** supports USART2/RX when set to Alternate Function 7 (**AF7**), so I configured those GPIOs accordingly.

For **BaudRate**, instead of just calling the mysterious `Serial.begin(115200)` function in the Arduino library, I manually implemented a `USART_SetBaudRate` function. I set Oversampling to 16 (`OVER8=0`) and calculated the value using the standard equation, confirming via the datasheet that my CPU runs at 16MHz by default.

## The "Volatile" Keyword & Assembly Optimization
A key takeaway from today is understanding the `volatile` keyword. When I started bare-metal development a month ago, I knew `volatile` was used to "stop the compiler from breaking things," but my understanding was vague. Recently, while learning about Assembly Language and ARM instructions in my **CSE30** course, I finally connected the dots.

### Instruction Efficiency
In CSE30, I learned that code efficiency comes down to the number of instructions the CPU executes. For example, a `do-while` loop can sometimes be more efficient than a `while` loop because of how branch instructions are handled (checking the condition once vs. twice). 

In limited-resource environments (like STM32 or 8-bit MCUs), every bit of optimization matters. Fewer instructions mean fewer clock cycles, which ultimately means the MCU draws less power and runs more efficiently. However, we don't write everything in Assembly because it would make development painfully slow. We rely on the **Compiler Optimizer** to translate our C code into efficient Assembly. But sometimes, the optimizer is *too* smart for its own good.

### The Problem with Optimization in Hardware
Consider this polling loop:
```c
while ( ! (USART2->SR & TXE) ); // Wait until Transmit Empty flag is set
```
If I compile this with `-O2` optimization without `volatile`, the CPU assumes `USART2->SR` is just a regular variable in memory. It thinks, "This value doesn't change inside the loop, so I'll just load it once to save time."

In Assembly, it might look like this:
```assembly
LDR R0, [R1]     ; Load SR value into Register R0 (Only happens once!)
Loop:
    CMP R0, #0   ; Check if R0 is 0
    BEQ Loop     ; If equal, branch back to Loop
```
The result: The CPU keeps checking the cached value in `R0`. Meanwhile, the actual hardware flag in the `SR` register might have flipped to 1 because the data transfer finished, but the CPU never looks at the physical address again. The program hangs in an infinite loop.

### The Volatile Solution
By adding the `volatile` keyword to the `SR` member in my `USART_RegDef_t` struct, I force the compiler to be "less lazy." It tells the compiler: "The value at this address can change at any time (by hardware), so do not cache it."

The resulting Assembly becomes:
```Assembly
Loop:
    LDR R0, [R1] ; ALWAYS re-load the value from the physical memory address
    CMP R0, #0
    BEQ Loop
```
This ensures the CPU always checks the real state of the hardware register.

### Struct Alignment
I also reinforced my understanding of struct memory mapping. Since `uint32_t` is exactly 4 bytes, and the STM32 registers are aligned by 4 bytes, defining a struct `USART_RegDef_t` with `uint32_t` members automatically aligns my code with the physical memory offsets.

**Correction on yesterday's code**: I actually forgot to add `volatile` to my struct definition yesterday! It still worked, likely because I was compiling in **Debug Mode** (which usually defaults to `-O0` optimization). If I had switched to **Release Mode** (which enables `-O2`), my code likely would have broken.

Today solidified the link between high-level C code, Assembly instructions, and the physical behavior of the processor.

## 2026-01-27: Motor Testing & PWM Logic Deep Dive

Today, following the successful physical connections made yesterday between the STM32 board, the A4988 Motor Driver, and the NEMA 17 Stepper Motor, I was able to reuse both the GPIO driver and the TIM driver I previously implemented for the PWM breathing LED demo.

### 1. Clock Speed & Calculation
Although the STM32F446RE is capable of running at **180MHz**, I have not manually configured the PLL (Clock Tree) yet, so it is currently running at its default HSI speed of **16MHz**.

Using the standard frequency formula:
$$Frequency = \frac{16\text{MHz}}{(\text{PSC} + 1) \times (\text{ARR} + 1)}$$

### 2. Initial Test
I used **PA0** to control the **STEP** pin and **PA1** for the **DIR** pin.
* **Settings:** PSC = 15, ARR = 999.
* **Result:** $16,000,000 / (16 \times 1000) = 1000\text{Hz}$.

The motor worked perfectly, spinning constantly at a relatively high speed. Toggling PA1 between 1 and 0 successfully changed the rotation direction (Clockwise/Counter-clockwise). It is honestly a pretty cool feeling to control physical hardware just by writing code.

### 3. Key Takeaway: LED vs. Motor (The Logic of PWM)
Even though both the Breathing LED and the Stepper Motor use the same TIM2 peripheral to generate a square wave, they utilize the PWM signal for fundamentally different purposes.

* **For the LED (Brightness Control):**
    * The focus is on **Duty Cycle (CCR)**.
    * **Logic:** Channel 1 is active as long as `TIMx_CNT < TIMx_CCR1`.
    * **ARR vs. PSC:** ARR sets the "cycle length," while PSC sets the "pace" (how fast the counter counts).
    * **Resolution:** ARR acts as the **resolution** of brightness. If ARR is 999, I have 1000 distinct levels of brightness. A larger ARR means the "steps" between brightness levels are smaller (e.g., 0.01% vs 0.1%), resulting in a smoother fading effect.

* **For the Motor (Speed Control):**
    * The focus is on **Frequency (ARR)**.
    * **Duty Cycle is Irrelevant:** The motor driver only cares about the **"Rising Edge"** (the instant voltage jumps from 0 to 1). It steps once per edge. It doesn't matter if the signal stays HIGH for 1% or 99% of the cycle, as long as the edge is detected.
    * **The Role of ARR:** Here, ARR regulates the *time interval* between these rising edges. Keeping PSC value as an invariant, a larger ARR means a longer wait between steps, resulting in a slower speed.
    * **Control Precision:** Just like with the LED, a higher ARR (combined with a lower PSC) gives us higher **Control Resolution**. We can achieve the same frequency with (PSC=3, ARR=5000) as (PSC=15, ARR=999), but the former allows us to fine-tune the speed more precisely, which is critical for smooth acceleration later.

### 4. The "Physics" Limit
I encountered an interesting issue when I kept PSC at 15 but lowered ARR to **800** (increasing frequency to ~1250Hz). The motor started making a **high-pitched whining sound (stalling)** but wouldn't spin.

I learned that this is due to the physical inertia of the motor rotor. If the frequency (speed request) is too high at startup, the coils switch magnetic fields faster than the rotor can physically move, causing it to lock up. This proves that finding the right balance between PSC and ARR is crucial—not just for the math, but for the physical limits of the hardware.

## 2026-01-26: Hardware Integration & Driver Logic Setup

Today, I spent several hours figuring out how to properly set up the physical connections between my STM32F4 board, the A4988 Motor Driver, and the NEMA 17 Stepper Motor before diving into the Bare-Metal driver implementation.

Here is the logic behind the setup and my key takeaways:

### 1. The Role of the Motor Driver
Essentially, the motor driver acts as a "transfer station" between the STM32 microcontroller and the motor. The STM32 operates at **3.3V** with a current limit of roughly **125mA** (total), while the NEMA 17 requires **0.8A to over 2A** depending on the load.
Direct connection is impossible: a source suitable for the STM32 cannot drive the motor, and a source powerful enough for the motor would instantly fry the microcontroller. The A4988 driver bridges this gap.

### 2. Physical Wiring & Lessons Learned
Following Rachel De Barros's guide, I successfully set up the connections:
* **SLP (Sleep) and RST (Reset):** I connected these two pins in parallel using a short jumper wire. If left unconnected (floating), they might default to a low state, causing the driver to sleep or reset. Connecting them together effectively pulls them high to keep the driver active.
* **VMOT & GND:** I used a 12V source for the "muscle voltage" to actually drive the motor.
* **VDD & GND:** I connected these to the 5V and GND pins on the STM32 board. VDD stands for **Logic Supply Voltage**. It powers the internal logic circuits of the A4988—the "brain" that interprets signals—while VMOT powers the "muscle."

### 3. Concept Correction: Signal vs. Power
I corrected a previous assumption: even though the STM32 controls the driver via low-current signals, the driver itself holds back the 12V power like a dam holding water. The STM32 signal is merely the valve handle; it tells the driver when to release the 12V current to the motor coils. The signal itself does not drive the motor.

### 4. Theory vs. Reality (The Capacitor)
Building this circuit highlighted the difference between ideal lecture theory and engineering reality. In ideal circuit theory, connecting a capacitor in parallel with a DC voltage source is tricky because voltage across a capacitor cannot change instantaneously, theoretically causing infinite current surges.
However, in the real world, a **100µF decoupling capacitor** is essential here. It acts as a local energy reservoir to smooth out voltage spikes (inductive kickback) caused by the motor coils and protects the driver from damage.

### 5. Pin Selection & Documentation Navigation
To control the motor, I need two pins (`STEP` and `DIR`). I specifically wanted to use **TIM2** (a General Purpose Timer), as I am already familiar with it from my PWM LED demo.
I had to navigate four different documents, finally understanding the specific purpose of each:
1.  **Datasheet (STM32F446xC/E):** The "Menu." It tells me what alternate functions (AF) each pin physically supports (e.g., "PA0 supports TIM2_CH1 via AF1").
2.  **Reference Manual (RM0390):** The "Bible." It details memory addresses, register offsets, and bit definitions. I use this the most.
3.  **User Manual (UM1724):** The "Board Manual." It explains Nucleo-specific features (e.g., PA5 is connected to the onboard LED, PC13 to the User Button).
4.  **Programming Manual (PM0214):** The "Assembly Guide." Details the Cortex-M4 instruction set.

### 6. AI Assistance vs. Engineering Judgment
I must credit Gemini for helping me ramp up on STM32 fundamentals. However, when it suggested using **PA8 and PA9** for the motor, I verified this against the Datasheet.
* **The Conflict:** `PA8`/`PA9` connect to **TIM1**, an Advanced Timer. While powerful, TIM1 requires extra configuration (like Deadtime insertion and Break inputs) which complicates the code.
* **The Decision:** I prefer the **KISS principle (Keep It Simple, Stupid)**. I only need basic PWM (PSC/ARR configuration).
* **The Solution:** By cross-referencing the Alternate Function table, I selected **PA0 and PA1**. They support **TIM2**, are free from UART conflicts (`PA2`/`PA3`), and leave I2C/SPI pins open for future sensor expansion.

### Conclusion
AI is a powerful efficiency tool, but it often lacks context or prioritizes complexity over simplicity. As a Computer Engineering student aiming for a firmware career, system verification is my responsibility. I learned that verifying datasheet information is just as critical as verifying physical wiring.

<img width="1133" height="800" alt="9a54fb2dc44838447d41101ac381234b" src="https://github.com/user-attachments/assets/c540057b-c729-4c7f-af9d-a29cc8de63f1" />

<img width="822" height="618" alt="image" src="https://github.com/user-attachments/assets/cb0b26e9-6c26-4662-bee9-dcb3101a7f9c" />



## 2026-01-26: The "Pivot" to Bare-Metal Drivers
* **Status Change:** Paused mechanical fabrication to focus entirely on Firmware reliability.
* **Reasoning:** ECE 35 Professor Curt advised that the original "Collar" idea had strict weight constraints (<1% of body weight). While I shifted to a Feeder design, I realized that **the mechanical shell is secondary to the control logic**.
* **Decision:**
    * **Shelved:** 3D printing tasks (will resume after firmware verification).
    * **Active:** Building a "Hardware-in-the-loop" MVP using just the STM32, A4988, and Nema 17 on a breadboard.
    * **Goal:** Prove I can control the motor precisely with register-level C code before I worry about the plastic shell.

---
*(Previous Entries Below)*

## 2026-01-09: Mechanical Research (Archived)
* **Design Research:** Evaluated open-source designs (Thingiverse "NorthernLayers") to avoid reinventing the wheel.
* **Plan:** Selected Nema 17 for high torque requirements.
* **Note:** This phase is currently on hold while I develop the STM32 drivers.
