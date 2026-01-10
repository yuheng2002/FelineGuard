# FelineGuard Development Log

This log documents my engineering decisions, technical hurdles, and progress as I build a dual-core automated pet feeder.

---

## 2026-01-09: Architecture, Mechanical Selection & "Makerspace Readiness"

### 💡 Project Philosophy: Why I'm not "Reinventing the Wheel"
As an ECE student, my core objective is the embedded firmware for the STM32 and ESP32. Early on, I realized that getting bogged down in complex mechanical CAD modeling would stall the project's momentum. 

**Decision:** I have opted for a "Verified Prototyping" approach—using open-source mechanical designs to allow me to focus 100% on the control logic and sensor fusion.

### ⚙️ Mechanical Selection: The "szuchid" Design
After evaluating multiple models on Thingiverse (e.g., NorthernLayers, gavinkennedy), I encountered several "deal-breakers": complex heat-set inserts, reliance on PVC pipes, or designs suited only for small bird seeds.

**Final Choice:** [Pet feeder by szuchid](https://www.thingiverse.com/thing:3424616)
- **Why?** It's elegant in its simplicity. It uses a vertical gravity-assisted flow and a clever soda-bottle thread for the hopper.
- **Components to print:** `bottom.stl` (Motor base), `screw.stl` (The auger), and `top.stl` (Hopper connector).
- **Prototyping Note:** I will use the **5mm to 8mm Flexible Coupler** I ordered as a mechanical bridge between the motor shaft and the printed auger. This provides a "safety margin" for any minor 3D printing alignment issues.

### 🛒 Hardware Procurement (BOM Status)
The following "Phase 1" components are currently in transit:
- **Actuator:** NEMA 17 Stepper Motor (59Ncm High Torque) + A4988 Driver.
- **Sensors:** 5kg Load Cell (HX711) for weight tracking; IR Break Beam Sensor for meal verification.
- **Power:** 12V 2A DC Adapter (Essential: includes a screw terminal adapter to avoid cutting cables).

### 🖨️ 3D Printing Strategy (Bambu Studio / Makerspace)
Through the UCSD ECE Makerspace tutorials, I've clarified the workflow from `.stl` to `.gcode`. 



**Key Technical Adjustments for the Auger (`screw.stl`):**
Standard print settings (15% infill) are too brittle for mechanical torque. To ensure the feeder can handle the resistance of dry kibble without snapping, I will manually override the slice parameters:
- **Infill Density:** 40% (Gyroid pattern for multi-directional strength).
- **Wall Loops:** Increased to 3-4 layers.
- **Supports:** Enabled "Tree (Auto)" to handle the overhangs on the T-connector.

### 🛠️ Makerspace Onboarding & Safety
I have successfully signed the DocuSign waiver and passed the **General Lab Access Quiz** with 100% accuracy. 

**Next Week's Mission:**
1. **Physical Sign-off:** Demonstrate 3D printer and soldering safety to a Tutor at Jacobs Hall B538.
2. **Fabrication:** Initiate the 4-6 hour print job for the mechanical chassis.
3. **Soldering:** Tackle the first hardware hurdle—soldering headers onto the HX711 amplifier. (The plan is to use the 3D printing "wait time" to gain hands-on soldering experience).

---
