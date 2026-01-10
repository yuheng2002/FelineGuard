# Development Log

## 2026-01-09: Mechanical Prototype Selection & Hardware Procurement

### 📝 Decision Making: Choosing the Mechanical Structure
As an ECE student specializing in embedded firmware, my primary focus for this project is the dual-core control system (STM32 + ESP32). I have limited experience with mechanical engineering and 3D modeling from scratch.

To ensure the project moves forward efficiently without getting stuck on mechanical fabrication, I decided to adapt an existing open-source design for the physical feeder mechanism.

**Criteria for Selection:**
1.  **Simplicity:** Must be printable with standard FDM printers without complex support structures.
2.  **Compatibility:** Must natively support the **Nema 17** stepper motor (a standard in robotics).
3.  **Reliability:** Needs a proven auger (screw) mechanism to prevent kibble jamming.
4.  **Aesthetics:** I wanted a design that looks clean and "elegant," rather than a makeshift assembly of PVC pipes.

**The Selection Process:**
I evaluated several popular designs on Thingiverse, including models by *NorthernLayers* and *gavinkennedy*. However, many were either designed for different purposes (bird feeders with small seeds), required complex heat-set inserts (which I wanted to avoid), or relied on expensive non-printed parts.

**Final Decision:**
I selected **[Pet feeder by szuchid](https://www.thingiverse.com/thing:3424616)**.
* **Pros:** It features a vertical gravity-assisted flow, a simple 3-part print (base, auger, T-connector), and uses a recycled soda bottle as the hopper, which is an elegant solution to reduce print time and material usage.
* **Action Plan:** I have downloaded the STL files (`base.stl`, `screw.stl`, `T-connector.stl`) and plan to print them at the UCSD ECE Makerspace next week using the Nema 17 motor I ordered today.

### 🛒 Hardware BOM (Bill of Materials) Status
Ordered the following core components from Amazon today:
* **Actuator:** Nema 17 Stepper Motor (59Ncm High Torque) + A4988 Driver
* **Mechanical:** 5mm to 8mm Flexible Coupler (as a fail-safe for the printed auger shaft)
* **Sensors:** 5kg Load Cell (w/ HX711) for weight monitoring; IR Break Beam Sensor for dispense verification.
* **Power:** 12V 2A DC Power Supply (with terminal adapter for breadboard compatibility).

**Next Steps:**
* Wait for hardware delivery.
* 3D print the mechanical parts.
* Begin Phase 1: Basic motor control testing with STM32.
