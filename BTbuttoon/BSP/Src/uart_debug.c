#include "uart_debug.h"

/* * GCC 编译器的 printf 重定向
 * VSCode 使用 arm-none-eabi-gcc，必须重写 _write 函数，而不是 fputc
 */
int _write(int file, char *ptr, int len) {
  // 使用 HAL 库的阻塞发送函数将数据发往 USART1
  HAL_UART_Transmit(&huart1, (uint8_t *)ptr, len, HAL_MAX_DELAY);
  return len;
}

/* 简单的字符串发送函数 (作为不使用 printf 时的备用) */
void UART_SendString(char *str) {
  while (*str != '\0') {
    HAL_UART_Transmit(&huart1, (uint8_t *)str, 1, HAL_MAX_DELAY);
    str++;
  }
}

/* 发送指定长度的字节数据 */
void UART_SendData(uint8_t *data, uint16_t len) {
  HAL_UART_Transmit(&huart1, data, len, HAL_MAX_DELAY);
}