/*
 * it.c
 *
 *  Created on: Mar 7, 2026
 *      Author: Krish
 */
#include "stm32f4xx_hal.h"

extern TIM_HandleTypeDef htim5;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart6;
/*
void SysTick_Handler(void)
{
	HAL_IncTick();
	HAL_SYSTICK_IRQHandler();
}
*/
void TIM5_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&htim5);
}



void EXTI0_IRQHandler(void)
{
	HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}
void USART6_IRQHandler(void)
{

  HAL_UART_IRQHandler(&huart6);

}
void USART2_IRQHandler(void)
{

  HAL_UART_IRQHandler(&huart2);

}
void USART1_IRQHandler(void)
{

  HAL_UART_IRQHandler(&huart1);

}
