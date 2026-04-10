/*
 * hc_sr04.h
 * Bare-metal HC-SR04 Driver for FreeRTOS
 */

#ifndef HC_SR04_H
#define HC_SR04_H

#include "stm32f4xx_hal.h"

/* Struct to hold your custom payload data */
typedef struct {
    float distance_cm;
    uint32_t flight_time_us;
} hcsr04_data_t;

/* Function Prototypes */
void init_distance_sensor(TIM_HandleTypeDef *htim, GPIO_TypeDef *trig_port, uint16_t trig_pin, GPIO_TypeDef *echo_port, uint16_t echo_pin);
int8_t get_distance_reading(hcsr04_data_t *sensor_data);

#endif /* HC_SR04_H */
