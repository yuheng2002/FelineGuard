#ifndef INC_BOARD_H_
#define INC_BOARD_H_

#include <stdint.h>
#include "stm32f4xx_hal.h"

/* User LED */
#define LD2_PORT            GPIOA
#define LD2_PIN             GPIO_PIN_5

/* A4988 signal pins */
#define MOTOR_DIR_PORT      GPIOA
#define MOTOR_DIR_PIN       GPIO_PIN_1
#define MOTOR_STEP_PORT     GPIOA
#define MOTOR_STEP_PIN      GPIO_PIN_0

/* USART2 pins */
#define USART2_RX_PORT      GPIOA
#define USART2_RX_PIN       GPIO_PIN_3
#define USART2_TX_PORT      GPIOA
#define USART2_TX_PIN       GPIO_PIN_2

/* on-board user button */
#define USER_BUTTON_PORT    GPIOC
#define USER_BUTTON_PIN     GPIO_PIN_13

#endif /* INC_BOARD_H_ */
