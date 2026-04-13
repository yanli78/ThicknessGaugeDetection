#include "key_exti.h"
#include <stdio.h>

// 定义按键按下标志位（全局变量）
uint8_t key1_flag = 0;
uint8_t key2_flag = 0;
uint8_t key3_flag = 0;
uint8_t key4_flag = 0;

/**
 * @brief  外部中断总回调函数
 * @param  GPIO_Pin: 触发中断的引脚
 * @retval 无
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
  if (GPIO_Pin == KEY1_GPIO_PIN) {
    key1_flag = 1;
  }
  // else if (GPIO_Pin == KEY2_GPIO_PIN)
  //   key2_flag = 1;
  // else if (GPIO_Pin == KEY3_GPIO_PIN)
  //   key3_flag = 1;
  // else if (GPIO_Pin == KEY4_GPIO_PIN)
  //   key4_flag = 1;
}