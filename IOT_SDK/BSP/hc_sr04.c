/*
 * hc_sr04.c
 * Bare-metal HC-SR04 Driver for FreeRTOS
 */

#include "hc_sr04.h"
#include <stdio.h>

/* Private hardware variables */
static TIM_HandleTypeDef *hc_timer;
static GPIO_TypeDef *t_port;
static uint16_t t_pin;
static GPIO_TypeDef *e_port;
static uint16_t e_pin;

/**
 * @brief Custom microsecond delay using the hardware timer.
 */
static void delay_us(uint16_t us) {
    __HAL_TIM_SET_COUNTER(hc_timer, 0);
    while (__HAL_TIM_GET_COUNTER(hc_timer) < us);
}

void init_distance_sensor(TIM_HandleTypeDef *htim, GPIO_TypeDef *trig_port, uint16_t trig_pin, GPIO_TypeDef *echo_port, uint16_t echo_pin) {
    printf("[HC-SR04] Initializing Distance Sensor...\r\n");

    hc_timer = htim;
    t_port = trig_port;
    t_pin = trig_pin;
    e_port = echo_port;
    e_pin = echo_pin;

    /* Start the timer in the background */
    HAL_TIM_Base_Start(hc_timer);

    printf("[HC-SR04] Initialization complete.\r\n");
}

int8_t get_distance_reading(hcsr04_data_t *sensor_data) {
    uint32_t local_time = 0;

    if (sensor_data == NULL) {
        printf("[HC-SR04] ERROR: Invalid NULL pointer for distance read.\r\n");
        return -1;
    }

    /* 1. Send a 10-microsecond HIGH pulse on the TRIG pin */
    HAL_GPIO_WritePin(t_port, t_pin, GPIO_PIN_SET);
    delay_us(10);
    HAL_GPIO_WritePin(t_port, t_pin, GPIO_PIN_RESET);

    /* 2. Wait for the ECHO pin to go HIGH (with a 50ms safety timeout) */
    uint32_t tick_start = HAL_GetTick();
    while (!(HAL_GPIO_ReadPin(e_port, e_pin))) {
        if ((HAL_GetTick() - tick_start) > 50) {
            printf("[HC-SR04] ERROR: Timeout. Sensor disconnected or blocked.\r\n");
            return -1;
        }
    }

    /* 3. Measure exactly how many microseconds the ECHO pin stays HIGH */
    __HAL_TIM_SET_COUNTER(hc_timer, 0);
    while (HAL_GPIO_ReadPin(e_port, e_pin)) {
        local_time = __HAL_TIM_GET_COUNTER(hc_timer);
        if (local_time > 38000) break; /* Sound wave travelled too far */
    }

    sensor_data->flight_time_us = local_time;
    sensor_data->distance_cm = (local_time * 0.0343f) / 2.0f;

    printf("[HC-SR04] Distance: %.2f cm, Flight Time: %lu us\r\n", sensor_data->distance_cm, sensor_data->flight_time_us);

    return 0;
}
