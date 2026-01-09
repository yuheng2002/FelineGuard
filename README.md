# FelineGuard: AI-Driven Dietary & Health Station 🐱

**FelineGuard** is a modular, dual-core embedded system designed to solve multi-cat dietary management and health monitoring challenges.

Unlike traditional time-based feeders, FelineGuard employs a **split-architecture** design: using an **STM32** for robust real-time actuation and an **ESP32** for high-level sensing and logic. This ensures that critical motor functions are decoupled from sensor processing, maximizing system reliability.

---

## 🏗 System Architecture

The system is divided into two independent subsystems communicating via **UART**:

### 1. The Body (Real-Time Actuation)
* **MCU**: STM32 Nucleo (Cortex-M4)
* **Role**: Handles precise motor stepping, safety interrupts, and watchdog monitoring.
* **Tech**: Bare-metal C drivers encapsulated in C++ classes.

### 2. The Brain (Sensing & Logic)
* **MCU**: ESP32-S3
* **Role**: Manages Thermal Sensing (Health), Identity Logic, and WiFi connectivity.
* **Tech**: C++, I2C Protocols, Sensor Fusion.

---

## 🗺️ Development Roadmap

This project is executed in four distinct phases to ensure a modular and testable development process.

### 🟢 Phase 1: The Mechanical Body (STM32 Foundation)
*Focus: Bare-metal Drivers, GPIO, Interrupts, and C++ RAII.*

- [ ] **Hardware Setup**: Connect ULN2003 Driver & Stepper Motor to STM32.
- [ ] **GPIO Driver**: Implement low-level register configuration (Input/Output) without HAL.
- [ ] **Motor Abstraction**: Create a C++ `class Motor` to encapsulate stepping logic.
- [ ] **UART Receiver**: Implement Ring Buffer & Interrupt-based UART to receive commands (e.g., `'F'` to feed).
- [ ] **Safety**: Basic boundary checks to prevent motor overrun.

### 🟡 Phase 2: Thermal Perception (ESP32 Sensing)
*Focus: I2C Protocol, Data Processing, Sensor Integration.*

- [ ] **Hardware Setup**: Interface MLX90640 Thermal Camera with ESP32.
- [ ] **I2C Driver**: Implement I2C communication to read sensor registers.
- [ ] **Data Processing**: Parse the $32 \times 24$ temperature array to find `Max_Temp`.
- [ ] **Health Logic**: Trigger an alert signal if detected temperature > 39.5°C.
- [ ] **Unit Testing**: Verify temperature readings against known sources.

### 🔴 Phase 3: Neural Integration (System Fusion)
*Focus: Inter-Processor Communication (IPC), State Machines.*

- [ ] **Physical Link**: Connect STM32 (UART TX/RX) and ESP32 (UART RX/TX) with common ground.
- [ ] **Protocol Design**: Define hex command structure (e.g., `0x01`=FEED, `0xFF`=EMERGENCY_STOP).
- [ ] **State Machine (STM32)**: Handle `IDLE`, `FEEDING`, and `LOCKED` states based on UART signals.
- [ ] **Integration Test**: Thermal Sensor (ESP32) detects heat -> Sends Stop Command -> Motor (STM32) halts immediately.

### 🔵 Phase 4: User Interaction & Optimization (Advanced)
*Focus: SPI Protocol, DMA, and UI.*

- [ ] **SPI Display**: Interface an OLED/TFT screen via **SPI** to STM32.
- [ ] **Status Dashboard**: Display real-time system status ("Ready", "Feeding", "Temp: Normal").
- [ ] **DMA Upgrade**: Refactor UART reception to use **Direct Memory Access (DMA)** to offload CPU.
- [ ] **Watchdog**: Enable Independent Watchdog (IWDG) to auto-reset system on hard faults.

---

## 🛠 Tech Stack & Skills

* **Hardware**: STM32F4, ESP32-S3, Stepper Motors (28BYJ-48), Thermal Camera (MLX90640), OLED (SPI).
* **Languages**: C (Drivers), C++17 (Application Logic), Python (Data Analysis/Testing).
* **Protocols**:
    * **UART**: Board-to-board communication.
    * **I2C**: Sensor data acquisition.
    * **SPI**: Display interface.
* **Concepts**: Bare-metal programming, RAII (Resource Acquisition Is Initialization), Interrupt Service Routines (ISR), DMA, Watchdog Timer.

---

## 🚀 Getting Started

*(Instructions on how to build and flash the firmware will be added here)*

1.  **STM32 Firmware**: Located in `/firmware-stm32`. Build using CMake/Make.
2.  **ESP32 Firmware**: Located in `/firmware-esp32`. Build using PlatformIO.

---

*Project created by Yuheng for UCSD ECE 145 Portfolio & 2026 Internship Preparation.*
