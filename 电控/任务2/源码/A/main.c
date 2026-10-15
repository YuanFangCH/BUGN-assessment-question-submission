#include "board.h"
#include "logic.h"
#include "oled.h"
#include <stdio.h>

static uint16_t adc_raw, adc_filtered, command; /* 原始值、滤波值、指令 */
static uint32_t tx_frames, uart_errors; /* 发送数、错误数 */

static void show_parameters(uint32_t now) { /* 每次刷新一行 */
    static uint8_t page; /* 当前显示行 */
    char text[24]; /* 显示缓冲 */
    switch (page) {
    case 0: snprintf(text, sizeof text, "TASK2 A UART TX"); break;
    case 1: snprintf(text, sizeof text, "ADC:%u", adc_raw); break;
    case 2: snprintf(text, sizeof text, "FILT:%u", adc_filtered); break;
    case 3: snprintf(text, sizeof text, "CMD:%u", command); break;
    case 4: snprintf(text, sizeof text, "ANG CMD:%u DEG", (unsigned)((uint32_t)command * 180u / 1000u)); break;
    case 5: snprintf(text, sizeof text, "TX:%lu", (unsigned long)tx_frames); break;
    case 6: snprintf(text, sizeof text, "UART ERR:%lu", (unsigned long)uart_errors); break;
    default: snprintf(text, sizeof text, "UP:%lu S", (unsigned long)(now / 1000u)); break;
    }
    oled_show_line(page, text);
    page = (uint8_t)((page + 1u) % 8u); /* 循环显示8行 */
}

int main(void) {

    board_init(); /* 初始化外设 */
    oled_init(); /* 初始化屏幕 */
    uint32_t sample_time = 0, send_time = 0, display_time = 0, heartbeat = 0;
    uint8_t sequence = 0; /* 帧序号 */

    while (1) {
        uint32_t now = HAL_GetTick();
        if (now - sample_time >= 10u) { /* 每10ms采样 */
            sample_time = now;
            adc_raw = board_adc(); /* 读取PA0 */
            adc_filtered = (uint16_t)((3u * (uint32_t)adc_filtered + adc_raw) / 4u); /* 新读数占1/4 */
        }
        if (now - send_time >= 20u) { /* 每20ms发送 */
            send_time = now;

            command = adc_filtered <= 3u ? 0u : /* 吸附两端值 */
                      adc_filtered >= 4092u ? 1000u : adc_map(adc_filtered, 1000);

            uint8_t frame[8] = {0xAA, 0x55, 1, sequence++, /* 8字节控制帧 */
                                (uint8_t)command, (uint8_t)(command >> 8), 0, 0x0D}; /* 指令低字节在前 */
            frame[6] = crc8(frame, 6); /* 前6字节校验 */
            if (HAL_UART_Transmit(&huart1, frame, 8, 5) == HAL_OK)
                tx_frames++; /* 发送成功 */
            else
                uart_errors++; /* 发送失败 */
        }
        if (now - display_time >= 40u) { /* 每40ms刷新一行 */
            display_time = now;
            show_parameters(now);
        }
        if (now - heartbeat >= 500u) { /* 板载灯心跳 */
            heartbeat = now;
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        }
    }
}
