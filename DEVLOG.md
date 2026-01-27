# FelineGuard Engineering Log


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
