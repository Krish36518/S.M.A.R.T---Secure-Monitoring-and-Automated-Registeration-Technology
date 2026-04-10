#include "stm32f4xx_hal.h"

void HAL_MspInit(void)
{
	//1.Set up the priority grouping
	HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

	//2.Enable the required system exceptions
	SCB->SHCSR |= 0X7 << 16; //USAGE FAULT,MEMORY FAULT AND BUS FAULT ENABLED
	//3.configure priority fir system exceptions
	HAL_NVIC_SetPriority(MemoryManagement_IRQn, 0, 0);
	HAL_NVIC_SetPriority(UsageFault_IRQn, 0, 0);
	HAL_NVIC_SetPriority(BusFault_IRQn, 0, 0);

}


void  HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htimer)
{
	//1.Enable the clock
	__HAL_RCC_TIM5_CLK_ENABLE();
	//2.Configure Nvic
	HAL_NVIC_EnableIRQ(TIM5_IRQn);
	//3.Set priority
	HAL_NVIC_SetPriority(TIM5_IRQn, 15, 0);
}
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef gpio_uart = {0};

    // --- Is this USART1 (Fingerprint Scanner) being initialized? ---
    if(huart->Instance == USART1)
    {
        __HAL_RCC_USART1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        gpio_uart.Pin = GPIO_PIN_9 | GPIO_PIN_10;
        gpio_uart.Mode = GPIO_MODE_AF_PP;
        gpio_uart.Pull = GPIO_PULLUP;
        gpio_uart.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        gpio_uart.Alternate = GPIO_AF7_USART1;
        HAL_GPIO_Init(GPIOA, &gpio_uart);

        HAL_NVIC_EnableIRQ(USART1_IRQn);
        HAL_NVIC_SetPriority(USART1_IRQn, 15, 0);
    }
    // --- Or is this USART2 (PC Console) being initialized? ---
    else if(huart->Instance == USART2)
    {
        __HAL_RCC_USART2_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_USART2_CLK_SLEEP_DISABLE();

        gpio_uart.Pin = GPIO_PIN_2 | GPIO_PIN_3;
        gpio_uart.Mode = GPIO_MODE_AF_PP;
        gpio_uart.Pull = GPIO_PULLUP;
        gpio_uart.Speed = GPIO_SPEED_FREQ_LOW;
        gpio_uart.Alternate = GPIO_AF7_USART2;
        HAL_GPIO_Init(GPIOA, &gpio_uart);

        HAL_NVIC_EnableIRQ(USART2_IRQn);
        HAL_NVIC_SetPriority(USART2_IRQn, 15, 0);
    }
    // --- Or is this USART6 (ESP32) being initialized? ---
    else if(huart->Instance == USART6)
    {
        __HAL_RCC_USART6_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        gpio_uart.Pin = GPIO_PIN_11 | GPIO_PIN_12;
        gpio_uart.Mode = GPIO_MODE_AF_PP;
        gpio_uart.Pull = GPIO_PULLUP;
        gpio_uart.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        gpio_uart.Alternate = GPIO_AF8_USART6; // Note: UART6 uses AF8!
        HAL_GPIO_Init(GPIOA, &gpio_uart);

        HAL_NVIC_EnableIRQ(USART6_IRQn);
        HAL_NVIC_SetPriority(USART6_IRQn, 15, 0);
    }
}
// 2. This is a special background function. When HAL_SPI_Init() runs,
// it automatically looks for this function to set up the physical pins!
void HAL_SPI_MspInit(SPI_HandleTypeDef* spiHandle)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(spiHandle->Instance == SPI1)
  {
    __HAL_RCC_SPI1_CLK_ENABLE();

    // Enable clocks for BOTH Port A and Port B
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    // 1. Configure PA5 (Clock)
    GPIO_InitStruct.Pin = GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // 2. Configure PB5 (The new MOSI pin!)
    GPIO_InitStruct.Pin = GPIO_PIN_5;
    // The Mode, Pull, Speed, and Alternate function remain exactly the same
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  }
}

