#ifndef HC595_H
#define HC595_H
#include "stm32f1xx_hal.h"

void hc595_init(void); /* GPIO就绪后清屏 */

void hc595_scan(uint8_t row_select, uint16_t columns); /* 行：bit0至5；列：bit0至15 */
#endif
