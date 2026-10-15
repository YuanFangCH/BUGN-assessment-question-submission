#include "led_app.h"
#include "hc595.h"

TIM_HandleTypeDef htim2; /* 1ms扫描定时器 */

static void check(HAL_StatusTypeDef status) { /* 错误时关闭显示 */
    if (status != HAL_OK) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET); /* OE先关闭 */
        __disable_irq();
        while (1) { }
    }
}

static void clock_init(void) { /* 8MHz倍频到72MHz */

    RCC_OscInitTypeDef o = {0};
    RCC_ClkInitTypeDef c = {0};
    o.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    o.HSEState = RCC_HSE_ON;
    o.PLL.PLLState = RCC_PLL_ON;
    o.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    o.PLL.PLLMUL = RCC_PLL_MUL9;
    check(HAL_RCC_OscConfig(&o));
    c.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 |
                  RCC_CLOCKTYPE_PCLK2;
    c.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    c.AHBCLKDivider = RCC_SYSCLK_DIV1;
    c.APB1CLKDivider = RCC_HCLK_DIV2; /* APB1为36MHz */
    c.APB2CLKDivider = RCC_HCLK_DIV1;
    check(HAL_RCC_ClockConfig(&c, FLASH_LATENCY_2));
}

static void gpio_init(void) { /* 595控制引脚 */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_AFIO_REMAP_SWJ_NOJTAG(); /* 保留SWD下载 */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET); /* OE先关闭 */
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = GPIO_PIN_0;
    gpio.Mode = GPIO_MODE_OUTPUT_PP; /* 推挽输出 */
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_RESET);
    gpio.Pin = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7; /* PA5时钟、PA6锁存、PA7数据 */
    HAL_GPIO_Init(GPIOA, &gpio);
    gpio.Pin = GPIO_PIN_13;
    HAL_GPIO_Init(GPIOC, &gpio);
}

static void timer_init(void) { /* 配置1ms扫描中断 */
    __HAL_RCC_TIM2_CLK_ENABLE();
    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 71; /* 72MHz分频到1MHz */
    htim2.Init.Period = 999; /* 每1000格触发 */
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    check(HAL_TIM_Base_Init(&htim2));
    HAL_NVIC_SetPriority(TIM2_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn); /* 开启TIM2中断 */
}

int main(void) {
    HAL_Init();
    gpio_init(); /* 先关闭输出 */
    clock_init();
    timer_init();
    led_app_init(); /* 清屏并启动扫描 */
    while (1) {
        led_app_poll(); /* 处理板载灯心跳 */
    }
}

void SysTick_Handler(void) { HAL_IncTick(); } /* 系统毫秒中断 */
void TIM2_IRQHandler(void) { HAL_TIM_IRQHandler(&htim2); } /* TIM2中断入口 */

void HardFault_Handler(void) { /* 异常时消隐 */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET); /* OE先关闭 */
    __disable_irq();
    while (1) { }
}
