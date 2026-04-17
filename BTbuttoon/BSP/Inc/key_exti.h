#ifndef __KEY_EXTI_H
#define __KEY_EXTI_H

#include "stm32f1xx_hal.h"

/************************ 正确引脚：PB10~PB13 ************************/
// 按键1 PB10
#define KEY1_GPIO_PORT GPIOB
#define KEY1_GPIO_PIN GPIO_PIN_10

extern uint8_t key1_flag;
extern uint32_t key1_exti_timestamp;
extern uint8_t key1_pending;

#endif