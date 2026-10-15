#ifndef LED_APP_H
#define LED_APP_H
#include "stm32f1xx_hal.h"

#ifndef LED_EFFECT
#define LED_EFFECT 0 /* 0字符流水，1逐点流水 */
#endif

void led_app_init(void); /* 清屏并启动扫描 */

void led_app_poll(void); /* 主循环调用 */

void led_app_tick_1ms(void); /* 扫描中断调用 */
#endif
