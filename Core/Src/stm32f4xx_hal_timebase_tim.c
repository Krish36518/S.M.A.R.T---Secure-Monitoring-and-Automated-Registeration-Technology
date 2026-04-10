#include "stm32f4xx_hal.h"

// Define the global handle so stm32f4xx_it.c can see it!
TIM_HandleTypeDef htim5;

HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
  RCC_ClkInitTypeDef    clkconfig;
  uint32_t              uwTimclock = 0;
  uint32_t              uwPrescalerValue = 0;
  uint32_t              pFLatency;

  /* 1. Enable TIM5 clock */
  __HAL_RCC_TIM5_CLK_ENABLE();

  /* 2. Get clock configuration to calculate the correct prescaler */
  HAL_RCC_GetClockConfig(&clkconfig, &pFLatency);
  uwTimclock = HAL_RCC_GetPCLK1Freq();

  // If APB1 clock divider is not 1, the timer clock is doubled
  if (clkconfig.APB1CLKDivider != RCC_HCLK_DIV1) {
      uwTimclock *= 2;
  }

  /* Compute the prescaler value to have TIM5 counter clock equal to 1MHz */
  uwPrescalerValue = (uint32_t) ((uwTimclock / 1000000U) - 1U);

  /* 3. Initialize TIM5 */
  htim5.Instance = TIM5;
  htim5.Init.Period = (1000000U / 1000U) - 1U; // 1ms interrupt
  htim5.Init.Prescaler = uwPrescalerValue;
  htim5.Init.ClockDivision = 0;
  htim5.Init.CounterMode = TIM_COUNTERMODE_UP;

  if(HAL_TIM_Base_Init(&htim5) == HAL_OK)
  {
    /* 4. Enable the TIM5 global Interrupt */
    HAL_NVIC_EnableIRQ(TIM5_IRQn);

    /* 5. Configure the SysTick IRQ priority (Highest priority required for HAL delays) */
    if (TickPriority < (1UL << __NVIC_PRIO_BITS))
    {
      HAL_NVIC_SetPriority(TIM5_IRQn, TickPriority, 0U);
      uwTickPrio = TickPriority;
    }
    else
    {
      return HAL_ERROR;
    }

    /* 6. CRITICAL STEP: Start the TIM time Base generation in interrupt mode! */
    return HAL_TIM_Base_Start_IT(&htim5);
  }

  /* Return function status */
  return HAL_ERROR;
}

// Optional but required by HAL to prevent compilation warnings
void HAL_SuspendTick(void)
{
  __HAL_TIM_DISABLE_IT(&htim5, TIM_IT_UPDATE);
}

void HAL_ResumeTick(void)
{
  __HAL_TIM_ENABLE_IT(&htim5, TIM_IT_UPDATE);
}
