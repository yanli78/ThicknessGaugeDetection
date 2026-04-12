#ifndef __KEY_EXTI_H
#define __KEY_EXTI_H

#include "stm32f1xx_hal.h"

/************************ 正确引脚：PB10~PB13 ************************/
// 按键1 PB10
#define KEY1_GPIO_PORT GPIOB
#define KEY1_GPIO_PIN GPIO_PIN_10
// // 按键2 PB11
// #define KEY2_GPIO_PORT GPIOB
// #define KEY2_GPIO_PIN GPIO_PIN_11
// // 按键3 PB12
// #define KEY3_GPIO_PORT GPIOB
// #define KEY3_GPIO_PIN GPIO_PIN_12
// // 按键4 PB13
// #define KEY4_GPIO_PORT GPIOB
// #define KEY4_GPIO_PIN GPIO_PIN_13

/************************ 按键参数 ************************/
#define KEY_PRESSED 0       // 低电平有效（接GND）
#define DEBOUNCE_TIME_MS 20 // 消抖时间


// 定义按键按下标志位（全局变量）
extern uint8_t key1_flag;
extern uint8_t key2_flag;
extern uint8_t key3_flag;
extern uint8_t key4_flag;

#endif