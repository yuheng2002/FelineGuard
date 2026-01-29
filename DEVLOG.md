# FelineGuard Engineering Log

## 2026-01-28: USART2 Setup & Enhanced Understanding of Volatile
Today, I implemented basic usart drivers and printed from my STM32 MCU to the VSCode Serial Monitor my first testing message "Motor System Initialized!".

Now, this basic Serial Print itself is not difficult or impressive at all. I recognize it is very basic. However, just recently, in my ESP32 IoT experience in this ECE140A course I am taking, I only had to do three things to print to the Serial Monitor.
1. I physically connect my ESP32 with my desktop using the ESP32 cable.
2. I call "Serial.begin(115200);"
3. I call Serial.print("--- testing ---")
Seems pretty simple, but it is because someone already wrote all the drivers to set up the peripherals in my ESP32 board in the background and keep those functions in the standard Arduino library, but now by implementing the usart drivers on my STM32 board myself, I am able to fully understand at the register level the interaction between my STM32 board and my PC.

Just like other peripherals, USART also has many registers, and within each register, different sets of bits serve different purposes. For my testing purpose, I only had to define a struct that has 5 configurations. Mode, WordLength, Parity(odd, even, or none), stopbits(0.5, 1, 1.5, 2), and baudrate. I used the standard "8-N-1" protocol, which means wordlength is 8 bits, not 9, and None parity checksum (as far as I learned, parity check is a powerful tool, but it is more used when the connection between devices is not stable, but in my case, my STM32 is right next to my desktop, conneted by a solid USB cable, so I do not need to use it just to save myself some trouble). and lastly, '1' refers to StopBits, as far as I have learned, its like someone needs to "catch his breathe" while taking, 0.5 would be too fast, while 2 would be too slow, so I just use the middle value 1, which seems to be pretty standard. As for Mode, PA2 support USART2/TX when set to AF7, and PA3 supports USART2/RX when set to AF7, so I just configured those two GPIOs. BaudRate is set to 115200, but instead of just calling that mysterious function Serial.begin(115200) in C++ using Arduino library, I had to manually implement a USART_SetBaudRate function where I set Oversampling to 16 (Over8=0) and used the corresponding equation. I also looked into the datasheet to confirm that, at default, my CPU runs at 16MHz, so everything checks out.

Now, another key takeaway is that, since I started learning the basics of Bare-Metal STM32 driver development about a month ago, I learned that it is important to use the volatile keyword to"avoid compiler optimizing code logic and just ignoring register values", but this is merely a very vague understanding. Just recently, I learned in my current course CSE30 about assembly language and ARM instructions, I finally put everything together to secure a solid logic chain. 

I learned in CSE30 that, what makes a do while loop better than a regular while loop is that, a do while loop checks the condition once at first, and then it immediately jumps into the first line of code in the while loop, and then only checks again the same condition to see if it should repeat the same thing at the end of the while loop, which means that, if the loop is supposed to run N times, it executes N + 1 "brach instructions", whereas the normal while loop executes 2N branch instructions. Now, if only with my previous experience and understanding of CS algorithm, this all equates to O(n), which shouldnt matter at all. However, in limited-resouce development envinroment, such as using a STM32 board, or maybe even another 8-bit, 16-bit Microcontroller, every bit of optimization matters. It all comes down to the number of instructions the CPU of my board has to execute, because eventually these arm instructions will be translated to machine language of a bunch of 1s and 0s, but in order to execute these, the peripherals has to "behave accordingly", right? I am not an expert in terms of hardware, but I understand that the board seems to do this by "strike" or whatever even electrons? I don't know, but the thing is, optimized code will help the MCU live longer, draws less power, otherwise what is the point of optimization? Anyways, this is just a quick example I thought of.

Let's get back to the volatile concept.

With what I just mentioned above, I wanted to point out that, just because I need to optimize performance, it does not mean that I need to manually translate every C code I write in assembly language, which would just make it 100 times harder than it already is and slow down the whole process. That's where the compilation optimizer comes in (hopefully thats what its called), it knows how to simply our "stupid and redundant C code" to effiecient assembly code that does the same thing. For example, in this simple while loop (while ! (USART2->SR & XE)), with -O2 optimization, the CPU does not know if the value of SR actually maps to an actual physical memory address on a STM32 MCU, it could just be some virtual garbage value I made up, right? so it will load the value, lets say R1, to R0, so LDR R0, [R1], and then goes to the loop, Loop:CMP R0, #0, BEQ Loop. Now, this is where it will cause problems. the CPU is now reading the value of USART->SR as what it previously stored in its cached register r0, but this register value could be changed due to physical facotrs, such as what if something goes wrong with the physical circuit? Or whatever, I cannot think of cases as I never encounted any. But, if I add the volatile keyword in the USART_RegDef_t struct member variable SR, the CPU cannot be "lazy" and will always execute LDR R0, [R1]
CMP R0, #0
BEQ Loop
It ALWAYS checks that value before executing any of the code inside the while loop, which is exactly what we want it to do.

Thus, since I define a struct called USART_RegDef_t which has all the registers defined in the USART register map, and also the memory address offsets are aligned using the technique that, the different between two registers is 4 bytes, and uint32_t is 4 bytes, so upon me pre-defining a USART_RegDef_t pointer USART2, when I do USART2->SR, it WILL access the actual SR register of USART2, so this is our last defence, I must put the keyword volatile before this register member variable.

Now, I actually forgot to do this when I was writing my code yesterday, so I didnt have either for my USART register definition struct, but it still worked, I am assuming that either I am lucky, or it was because I was in debug mode by default, instead of being in release mode, which will turn on -O2 optimization automatically.

So, I learned a lot from today's experience, especially in terms of why exactly we really need volatile, from assembly language level, and how basic USART protocol works.

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
