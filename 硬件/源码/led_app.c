#include "led_app.h"
#include "hc595.h"

extern TIM_HandleTypeDef htim2; /* 句柄定义在main.c */

static const uint16_t base_rows[6] = { /* 字模：bit0对应COL01 */
    0x0000u, 0x9693u, 0x91B5u, 0x9995u, 0x7F97u, 0x0000u
};
static uint8_t scan_row; /* 当前行0至5 */
static uint8_t character; /* 0=B，1=N，2=G，3=U */
static uint8_t pixel; /* 当前灯0至35 */
static uint16_t elapsed; /* 效果计时，ms */
static uint8_t pixel_rows[36]; /* 每颗灯的行号 */
static uint16_t pixel_bits[36]; /* 每颗灯的列掩码 */

void led_app_init(void) { /* 建立已装LED位置表 */
    hc595_init(); /* 先消隐清屏 */
    uint8_t count = 0u;
    for (uint8_t row = 0u; row < 6u; row++) {
        for (uint8_t col = 0u; col < 16u; col++) {
            uint16_t bit = (uint16_t)(1u << col); /* 当前列掩码 */
            if ((base_rows[row] & bit) != 0u && count < 36u) { /* 只登记已装LED */
                pixel_rows[count] = row; /* 保存行号 */
                pixel_bits[count] = bit; /* 保存列掩码 */
                count++;
            }
        }
    }
    scan_row = character = pixel = 0u; /* 6行循环 */
    elapsed = 0u;

    if (count != 36u || HAL_TIM_Base_Start_IT(&htim2) != HAL_OK) { /* 表长或启动异常则消隐 */
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
        __disable_irq();
        while (1) { }
    }
}

void led_app_tick_1ms(void) { /* 每次扫描一行 */
    uint16_t columns;
    if (LED_EFFECT == 1) { /* 逐点流水 */

        columns = (scan_row == pixel_rows[pixel]) ? pixel_bits[pixel] : 0u;
    } else {

        uint16_t character_mask = (uint16_t)(0x000Fu << (4u * character)); /* 每字符占4列 */
        columns = (uint16_t)(base_rows[scan_row] & character_mask);
    }
    hc595_scan((uint8_t)(1u << scan_row), columns); /* 输出当前行 */
    scan_row = (uint8_t)((scan_row + 1u) % 6u); /* 6行循环 */
    elapsed++;

    if (elapsed >= 250u && scan_row == 0u) { /* 整帧结束再切换，实际252ms */
        elapsed = 0u;
        character = (uint8_t)((character + 1u) % 4u); /* 切到下个字符 */
        pixel = (uint8_t)((pixel + 1u) % 36u); /* 切到下颗LED */
    }
}

void led_app_poll(void) { /* 主循环心跳 */
    static uint32_t previous;
    uint32_t now = HAL_GetTick();
    if (now - previous >= 500u) { /* 每500ms翻转 */
        previous = now;
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *timer) { /* 定时器回调 */
    if (timer->Instance == TIM2)
        led_app_tick_1ms();
}
