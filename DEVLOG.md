# FelineGuard Engineering Log

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
* **WordLength = 8 bits**: Standard data size (instead of 9).
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
