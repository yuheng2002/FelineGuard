### 2026-08-02 -- Blink LD2 to verify the HAL GPIO path

First code on the board: a blocking blink of the on-board LED, to check that the
project builds, flashes and runs.

**HAL does not enable the GPIO port clock.** Unlike the Nuvoton M2351 I use at
work, this chip needs `__HAL_RCC_GPIOx_CLK_ENABLE()` before a pin can be
configured. `HAL_GPIO_Init()` writes MODER, OTYPER, PUPDR and OSPEEDR directly
and never touches RCC. Without the clock the writes go nowhere and the pin stays
dead, with no error reported anywhere. ST lists it as a separate step in the "How
to use this driver" block at the top of `stm32f4xx_hal_gpio.c`.

LD2 is on PA5, connected to Arduino signal D13 through solder bridge SB21
(UM1724 Section 7.6 and Section 7.11).