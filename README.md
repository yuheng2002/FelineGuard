# FelineGuard (STM32 Bare-Metal Project)

**FelineGuard** is a pet feeder project I am building from scratch to learn embedded firmware development.

While many smart feeders focus on App features, my goal here is to master the low-level control. I am writing my own drivers in **C (Bare-Metal)** for the STM32F446RE without using the HAL library. The idea is to make sure the motor control is reliable and deterministic—so the cat gets fed even if the Wi-Fi breaks.

> **Current Status:** Working on Phase 1 (Motor Drivers & Basic Logic).

---

## 🛠 Hardware Setup

I am keeping the hardware simple to focus on the code logic.

* **MCU:** STM32 Nucleo-F446RE (ARM Cortex-M4)
* **Motor:** Nema 17 Stepper Motor (controlled via **A4988 Driver**)
* **Power:** 12V 2A DC Adapter (for the motor) + 3.3V Logic
* **Tools:** Logic Analyzer (for verifying PWM waveforms) & Multimeter

---

## 💻 Tech & Registers (What I'm Learning)

I am manually configuring the microcontroller registers to understand how the hardware works under the hood.

* **GPIO Driver:**
    * Using `MODER` and `AFR` to set pin modes.
    * Using `BSRR` (Bit Set/Reset Register) for atomic pin control (Step/Dir signals), avoiding read-modify-write issues.
* **Timer (TIM2):**
    * Configuring `PSC` (Prescaler) and `ARR` (Auto-Reload) to generate precise 1kHz PWM signals for the stepper motor.
* **Safety & Debugging (In Progress):**
    * **UART:** Writing a simple command parser to control the motor from my computer.
    * **Watchdog (IWDG):** Setting up the hardware watchdog to auto-reset the system if the firmware freezes.

---

## 📝 Development Plan

### Phase 1: The Basics (Current Focus)
*Goal: Make the motor spin precisely without HAL.*
- [x] Hardware wiring (STM32 -> A4988 -> Motor).
- [x] Write GPIO driver to control the A4988 Direction pin.
- [x] Configure TIM2 to generate PWM pulses for the Step pin.
- [ ] Verification: Check the PWM frequency with a Logic Analyzer.

### Phase 2: Control & Safety (This Week)
*Goal: Add interaction and reliability.*
- [ ] Implement UART to send "Feed" commands via laptop.
- [ ] Add a User Button interrupt (PC13) as a manual trigger.
- [ ] Enable the Watchdog Timer (IWDG) to prevent crashes.

### Phase 3: Future Improvements
- [ ] Move the logic to a Finite State Machine (FSM).
- [ ] Potentially connect to an ESP32 for Wi-Fi features.

---

## 📂 Project Structure

```text
FelineGuard/
├── Drivers/
│   ├── Inc/               # My header files (stm32f446xx.h, etc.)
│   └── Src/               # My driver implementation (.c files)
├── Src/
│   └── main.c             # Main application logic
└── devlog.md              # My daily notes and debugging steps
