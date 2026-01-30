/*
 * Created on: 2026/1/2
 * Author: Yuheng
 *
 * Description:
 * Header file for TIM (Hardware Timer) Driver.
 *
 * Why Hardware Timer?
 * 1. Efficiency: Software delay (busy-wait loops) wastes CPU cycles, blocking other tasks.
 * Hardware timers operate independently of the CPU core.
 * 2. Precision: Hardware timers provide precise timing determined by the system clock,
 * whereas software delay duration varies with compiler optimization and code execution.
 * 3. PWM Generation: This driver handles PWM signal generation for the breathing LED project.
 */

#ifndef SOURCES_STM32F446XX_TIMER_DRIVER_H_
#define SOURCES_STM32F446XX_TIMER_DRIVER_H_

/*
 * ==========================================
 * 1. Peripheral Clock Setup
 * ==========================================
 * Must be called FIRST before using any Timer.
 * 'EnableOrDisable' should be ENABLE or DISABLE macros.
 * TIM2 is connected to APB1 Bus (offset 0x00) according to memory map in RM0390 manual
 * thus, we need to access RCC_APB1ENR (APB1 peripheral clock enable register)
 * Bit 0 sets TIM2
 * 0: TIM2 clock disabled
 * 1: TIM2 clock enabled
 */
#define TIM2_PCLK_EN()  (SET_BIT(RCC->APB1ENR, 0)) // Bitwise Macro defined in stm32f446xx.h

#define TIM3_PCLK_EN()  (SET_BIT(RCC->APB1ENR, 1)) // Bit 1 TIM3EN: TIM3 clock enable

#define TIM6_PCLK_EN()  (SET_BIT(RCC->APB1ENR, 4)) // Bit 4 TIM6EN: TIM6 clock enable


/*
 * ==========================================
 * 2. Configuration Structures
 * ==========================================
 */

/*
 * TIM Configuration Structure
 * This structure acts as the "Menu" for user-configurable settings.
 *
 * [Design Decision Note]
 * For the "Breathing LED" project, this structure is intentionally simplified.
 * It abstracts away the complexity of specific register bits that don't need
 * to change for this specific application.
 *
 * Exposed Settings:
 * 1. Prescaler (PSC): Controls the "Speed" of the timer tick.
 * 2. Period (ARR): Controls the "Frequency" of the PWM cycle.
 *
 * Hidden Settings (Hardcoded in Driver):
 * - PWM Mode (CCMR1): Fixed to PWM Mode 1.
 * - Output Enable (CCER): Always enabled for Channel 1.
 * - Counter Enable (CR1): Always turned on after init.
 *
 * Why not CCR1?
 * The Capture/Compare Register (CCR1) controls duty cycle (brightness).
 * Since brightness changes dynamically at runtime (in the while loop),
 * it is not part of the static initialization configuration.
 */
typedef struct{
    uint32_t Prescaler;  // Clock Prescaler (PSC): SystemCoreClock / (Prescaler + 1)
    uint32_t Period;     // Auto-Reload Value (ARR): Determines PWM frequency
} TIM_Config_t;

/*
 * TIM Handle Structure
 * This structure acts as the "Object" or "Handle" for a specific Timer instance.
 * It bundles the Physical Hardware with User Configuration.
 *
 * Concept: "Job Order"
 * - "Where" to do the job: pTIMx (Base Address of the peripheral)
 * - "How" to do the job:   TIM_Config (User parameters)
 */
typedef struct{
    TIM_RegDef_t *pTIMx;       // Pointer to the Timer's Register Base Address
    TIM_Config_t TIM_Config;   // Configuration settings
} TIM_Handle_t;

/* Function Prototypes */
void TIM_PWM_Init(TIM_Handle_t *pTIMHandle);
void TIM_SetCompare1(TIM_RegDef_t *pTIMx, uint32_t CaptureValue);

void TIM_Basic_Init(TIM_Handle_t *pTIMHandle); // Basic Timer (not targeted at PWM)
void TIM_IRQInterruptConfig(uint8_t IRQNumber, uint8_t EnableOrDisable);
#endif /* SOURCES_STM32F446XX_TIMER_DRIVER_H_ */
