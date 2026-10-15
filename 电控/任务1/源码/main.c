#include "board.h"
#include "logic.h"
#include "oled.h"

#define SAMPLE_MS 10u /* 采样间隔 */
#define DISPLAY_MS 250u /* 刷新间隔 */

int main(void) {

    board_init(); /* 初始化外设 */
    oled_init(); /* 初始化屏幕 */
    uint16_t adc_filtered = 0; /* 滤波值 */
    uint32_t sample_time = 0, display_time = 0;

    while (1) {
        uint32_t now = HAL_GetTick(); /* 当前毫秒数 */
        if (now - sample_time >= SAMPLE_MS) {
            sample_time = now;
            uint16_t adc = board_adc(); /* 读取PA0 */
            adc_filtered = (uint16_t)((3u * (uint32_t)adc_filtered + adc) / 4u); /* 新读数占1/4 */
            board_pwm(adc_map(adc_filtered, 1000)); /* 换算PWM */
        }
        if (now - display_time >= DISPLAY_MS) {
            display_time = now;
            oled_show_values(adc_filtered, adc_map(adc_filtered, 1000)); /* 显示ADC和占空比 */
        }
    }
}
