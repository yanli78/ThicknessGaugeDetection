#include "key_exti.h"

// 定义按键相关的全局变量
uint8_t key1_flag = 0;
uint32_t key1_exti_timestamp = 0;
uint8_t key1_pending = 0;

/**
 * @brief  外部中断总回调函数
 * @param  GPIO_Pin: 触发中断的引脚
 * @retval 无
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
  if (GPIO_Pin == KEY1_GPIO_PIN) {
    // 仅在未处于消抖状态时才处理，避免重复触发
    if (key1_pending == 0) {
      key1_pending = 1;                    // 标记进入消抖等待
      key1_exti_timestamp = HAL_GetTick(); // 记录当前系统时间
    }
  }
}