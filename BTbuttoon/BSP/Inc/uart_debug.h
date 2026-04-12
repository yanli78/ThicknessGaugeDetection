#ifndef __UART_DEBUG_H
#define __UART_DEBUG_H

#include "usart.h" // 包含 CubeMX 生成的 USART 头文件
#include <stdio.h>

/* 发送普通字符串函数 */
void UART_SendString(char *str);

/* 发送包含特定长度的数据 */
void UART_SendData(uint8_t *data, uint16_t len);

#endif /* __UART_DEBUG_H */
