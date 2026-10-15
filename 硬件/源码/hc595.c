#include "hc595.h"

static void write_byte(uint8_t value) { /* 高位先发，共8位 */
    for (uint8_t bit = 0; bit < 8u; bit++) {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET); /* 时钟拉低 */
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, /* 设置数据位 */
                         (value & 0x80u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET); /* 上升沿移入数据 */
        value = (uint8_t)(value << 1); /* 准备下一位 */
    }
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
}

static void latch_frame(uint8_t row, uint16_t columns) { /* 发送24位并锁存 */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
    write_byte(row); /* 先发U3行选择 */
    write_byte((uint8_t)(columns >> 8)); /* 再发U2高8列 */
    write_byte((uint8_t)columns); /* 最后发U1低8列 */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET); /* 上升沿锁存 */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
}

void hc595_init(void) { /* 上电清屏 */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET); /* OE高：关闭输出 */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7,
                     GPIO_PIN_RESET);
    latch_frame(0u, 0u); /* 清空行列 */
}

void hc595_scan(uint8_t row_select, uint16_t columns) { /* 扫描一行 */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET); /* OE高：关闭输出 */

    if ((row_select & 0xC0u) != 0u || /* 只允许单行或全零 */
        (row_select != 0u && (row_select & (row_select - 1u)) != 0u)) {
        latch_frame(0u, 0u); /* 清空行列 */
        return;
    }
    latch_frame(row_select, columns);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET); /* OE低：打开输出 */
}
