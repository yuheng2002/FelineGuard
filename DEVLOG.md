# FelineGuard Engineering Log


## 2026-01-26: Continued
* Today, I spent severals hours figuring out how to properly set up the physical connections between my STM32F4 board, the A4988 Stepper Motor, the NEMA 17 Motor, before I actually dive into implementing the Bare-Metal drivers for this project.
Here is the full logic of how it works, and some of my key takeaways.
The motor driver, namingly, acts as the "transfer station" between the STM32 microcontroller and the motor, the former works at 3.3V and has a current limit of 125mA, while the later ranges from 0.8A to over 2A depending on its current load. Therefore, there is no possible for us to connect these two things directly using a single voltage source. The suitable voltage source will not be able to generate big enough current to drive the motor, while a voltage source that can generate big enough current will instantly fry the STM32 Microcontroller. Thus, this is where the motor driver comes in. 

Following Rachel De Barros's youtube video "Control a NEMA 17 Stepper Motor with A4988 Driver", I was able to properly set up the physical connections. I used one of the smallest jumper wires to connect the SLP and RST pins in parallel, and if I do not do this, they could be in active state, causing the motor driver to "lock down" and just not function properly. 
On the "right side" of my motor driver, I used a 12V source to supply power to VMOT and GND pins, VMOT stands for Motor Voltage, which is like the "muscle voltage" that actually drives the motor to spin. And then I connected VDD and GND to the 5V and GND pins in the encapsulated Arduino power region on my STM32 board, which allows the STM32 to act as the brain to send signals to the motor driver, and upon receiving these signals, the motor driver will deliver voltage to the motor to let it spin. VDD stands for "Voltage Drain Drain", which baiscally acts as a logic gate, the signal.

Now, one thing I learned which is different from my previous assumption is that, even though the STM32 board controls the motor driver through signal with very small current, the motor driver actually holds that 12V source, it is like the valve of a water gate, it keeps all the water, and let it all go to the motor once it receives a "let go" signal. It does not use "signal" to control the motor like STM32 uses signal to control the motor driver.

Also, by actually building the physical connections myself, I learned from the video that, in the real world, when elements are most of the time not as ideal as I learned from lecture examples, the same principle does not always apply. In ideal envinroment, we cannot connect a capacitor in parallel with a voltage source, as a capacity's voltage cannot change instantanieouly, and if we do this, the capacitor might just have to release huge amount of energy in a very short amount of time, causing spark in the air and damage to the electornics. However, for our purpose, a decouping capacitor of 100uF connected in parallel with the 12V voltage source is actually used to protect our motor driver, which, according to Rachel, "balance out any voltage spikes that might come through that would otherwise be hitting your stepper motor and potentially damage it".

This concludes everything I learned on the Hardware side.

Now, since the motor driver has two embedded functions in it, one of which is step, and other of which is DIR, which stands for direction. I needed to find two pins on my STM32 board that supported TIM2, which is the same basic hardware timer I previously used in my PWM breathing LED demo, but also does not conflict with other properties I want to use with my board. I have at my hands four different datasheets along with pinout maps. While previously I was just basically going everything to find whatever I needed when I was working on my previous demos, this time I learned the difference between these manuals and why it is this way.

the STM32 board is manuafactured by STMicroelectronics, which is a company that only focuses on making the MCU board, it is a raw CPU with no drivers. Now, the brand Nucleo, makes the f446RE board as a development board, they add "features" to the board, such as connecting PA5 to LED2, and PC13 to the USER button. Thus, 
1. Datasheet (STM32F446xC/E) is like the "user manual" provided by Nucleo to teach clients what "functions" each specific pin can do, for example, it provides the Alternate Function table, which tells that, "PA5 can use TIM_CHI with Alternate Function AF1".
2. the RM0390 manual, this is what I read the most, because it tells me the actual physical memory addresses of different groups of registers on this board and their offsets, and more specially, how to change their behaviors by setting specific bits.
3. User Manual (UM1724), this one tells that, "PA5" is LED2, and PA2/PA3 are for USART, and PC13 is for user button, stuff like this.
4. Programming manual (PM0214), it is a manual of a bunch of assembly language instructions, which I doubt I will need for my purpose at the moment.

Now, I must give credit to AI, specifically Gemini, in terms of helping me "self-study" the fundamentals of firmware and specially STM32 registers, because it really saved me a lot of time having to just figure out basically, what is what, what it does...It suggested me use PA8 as the pin for Step, and PA9 for DIR. However, as I manually went over the Table 11. Alternate Function table in Datasheet STM32F446xC/E, I found that PA8/PA9 only support TIM1, which is an advanced timer similar to PA8. When I questioned its feedback, it actually insisted that it would be easier to just stick with the PA8/9 pins for whatever reason it listed, and that it could just give me all the driver code I needed to run the motor. However, I did not want it to give me code that I did not understand, because it hurts my learning. And also, because I only need to set up similar logic, such as PSC, ARR, to generate a PWM wave to control the speed of my motor spinning, I did not want to put extra effort in having to set up more registers just to use the PA8/9 pins. I think it makes much more sense to just keep everything simple and stupid for my purpose. The more complicated it gets, the more likely I will make mistakes. As a result, I found in the datasheet that PA2 and PA3 are reserved for UART, which I will be using for this project. and I also want to late add sensor features to my project, so I also want to avoid GPIOs that are reserved for I2C or maybe even SPI if I wanted to add a screen. By cross referencing different GPIOs and their alternate functions, I landed my eyes on PA0 and PA1, which do not conflict with other purposes and therefore are a perfect match.

This experience once again makes me realize that, AI is a very powerful tool that can boost people's working efficiency. It thinks faster and reads faster than any human, but it also tends to make mistakes a lot of the times. As a computer engineering student who wants to pursue a firmware career, the stability of system is one of my top priorities. Veficiation of not only hardware connections, but also information from datasheets are both important tasks for me to ensure a stable system.



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
