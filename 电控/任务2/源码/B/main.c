#include "board.h"
#include "logic.h"
#include "oled.h"
#include <stdio.h>
#include <string.h>

static uint8_t rx_byte, ring[128]; /* 接收字节、环形缓冲 */
static volatile uint16_t head, tail; /* 写位置、读位置 */
static volatile uint32_t uart_errors; /* 串口错误数 */
static uint16_t command, pulse = SERVO_MID_US, target = SERVO_MID_US; /* 指令、输出、目标 */
static uint32_t last_valid, rx_frames; /* 收帧时刻、有效帧数 */
static uint8_t received; /* 是否收到有效帧 */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart) { /* 接收中断回调 */
    if (uart->Instance != USART1)
        return;
    uint16_t next = (uint16_t)((head + 1u) % 128u); /* 计算下个写位置 */
    if (next != tail) { /* 缓冲未满 */
        ring[head] = rx_byte; /* 存入字节 */
        __DMB(); /* 先写数据再更新位置 */
        head = next;
    } else {
        uart_errors++;
    }
    HAL_UART_Receive_IT(uart, &rx_byte, 1); /* 继续接收1字节 */
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart) { /* 串口错误回调 */
    if (uart->Instance != USART1)
        return;
    uart_errors++;
    HAL_UART_AbortReceive(uart); /* 结束旧接收 */
    HAL_UART_Receive_IT(uart, &rx_byte, 1); /* 继续接收1字节 */
}

static void process_uart(uint32_t now) { /* 主循环解包 */
    static uint8_t frame[8]; /* 暂存一帧 */
    static unsigned length; /* 已收字节数 */
    while (tail != head) { /* 读取待处理字节 */
        frame[length++] = ring[tail];
        tail = (uint16_t)((tail + 1u) % 128u);
        if (length == 8u) { /* 收满后校验 */
            uint16_t value = (uint16_t)frame[4] | ((uint16_t)frame[5] << 8); /* 合并指令高低字节 */
            if (frame[0] == 0xAA && frame[1] == 0x55 && frame[2] == 1 && /* 检查帧头、类型 */
                frame[7] == 0x0D && crc8(frame, 6) == frame[6] && value <= 1000u) { /* 检查帧尾、CRC和范围 */
                command = value;
                target = (uint16_t)(SERVO_MIN_US + /* 映射到500至2500us */
                         (uint32_t)value * (SERVO_MAX_US - SERVO_MIN_US) / 1000u);
                last_valid = now; /* 记录有效帧时刻 */
                received = 1;
                rx_frames++;
                length = 0; /* 准备下一帧 */
            } else {
                memmove(frame, frame + 1, 7); /* 错帧左移1字节 */
                length = 7; /* 保留后7字节 */
                uart_errors++;
            }
        }
    }
}

static void servo_update(uint32_t now) { /* 更新舵机输出 */
    if (now - last_valid > UART_TIMEOUT_MS) /* 超时保持 */
        target = pulse;
    if (pulse < target) /* 每次最多增加20us */
        pulse = (uint16_t)(target - pulse > 20u ? pulse + 20u : target);
    else if (pulse > target) /* 每次最多减少20us */
        pulse = (uint16_t)(pulse - target > 20u ? pulse - 20u : target);
    board_pwm(pulse); /* 写入PWM比较值 */
}

static void show_parameters(uint32_t now) { /* 每次刷新一行 */
    static uint8_t page;
    char text[24];
    const char *link = !received ? "WAIT" : /* 等待、保持或正常 */
                       now - last_valid > UART_TIMEOUT_MS ? "HOLD" : "OK";
    switch (page) {
    case 0: snprintf(text, sizeof text, "TASK2 B UART %s", link); break;
    case 1: snprintf(text, sizeof text, "CMD:%u", command); break;
    case 2: snprintf(text, sizeof text, "TARGET:%u US", target); break;
    case 3: snprintf(text, sizeof text, "PULSE:%u US", pulse); break;
    case 4: snprintf(text, sizeof text, "ANG CMD:%u DEG", (unsigned)((uint32_t)(pulse - SERVO_MIN_US) * 180u / (SERVO_MAX_US - SERVO_MIN_US))); break;
    case 5: snprintf(text, sizeof text, "RX:%lu", (unsigned long)rx_frames); break;
    case 6: snprintf(text, sizeof text, "UART ERR:%lu", (unsigned long)uart_errors); break;
    default: snprintf(text, sizeof text, "UP:%lu S", (unsigned long)(now / 1000u)); break;
    }
    oled_show_line(page, text);
    page = (uint8_t)((page + 1u) % 8u); /* 循环显示8行 */
}

int main(void) {
    board_init(); /* 初始化外设 */
    oled_init(); /* 初始化屏幕 */
    if (HAL_UART_Receive_IT(&huart1, &rx_byte, 1) != HAL_OK) /* 开启接收中断 */
        fail_stop();
    uint32_t step_time = 0, display_time = 0, heartbeat = 0;
    while (1) {
        uint32_t now = HAL_GetTick();
        process_uart(now); /* 处理接收数据 */
        if (now - step_time >= 20u) { /* 每20ms更新输出 */
            step_time = now;
            servo_update(now);
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
