/*
 * esp32_at_io.c
 * Black Pill (STM32F411) Interrupt-Driven Port
 */

#include "esp32_at_io.h"
#include "main.h"
#include <string.h>

#define RING_BUFFER_SIZE (1024 * 10)

typedef struct {
  uint8_t data [ RING_BUFFER_SIZE ];
  volatile uint16_t tail;
  volatile uint16_t head;
} ring_buffer_t;

ring_buffer_t wifi_rx_buffer;
volatile uint8_t rx_byte; /* The 1-byte trap */

/* Private function prototypes -----------------------------------------------*/
static void esp32_io_error_handler(void);

/* Exported functions -------------------------------------------------------*/

/*
 * esp32_at_io.c
 * BARE-METAL REGISTER POLLING FIX
 */


int8_t esp32_io_init(void) {
  /* Give the ESP32 2 seconds to boot up and finish its spam */
  HAL_Delay(2000);

  /* Clear any lingering hardware panic flags before we start */
  __HAL_UART_CLEAR_OREFLAG(ESP32_UART_HANDLE);
  __HAL_UART_CLEAR_NEFLAG(ESP32_UART_HANDLE);
  __HAL_UART_CLEAR_FEFLAG(ESP32_UART_HANDLE);

  /* We are NOT turning on DMA or Interrupts.
     We will poll the bare metal silicon directly! */
  return 0;
}

void esp32_io_deinit(void) {
  HAL_UART_DeInit(ESP32_UART_HANDLE);
}

int8_t esp32_io_send(uint8_t *p_data, uint32_t length) {
  if ( HAL_UART_Transmit(ESP32_UART_HANDLE, p_data, length, 1000) != HAL_OK ) {
    return -1;
  }
  return 0;
}

int32_t esp32_io_recv(uint8_t *buffer, uint32_t length) {
  uint32_t read_data = 0;

  while ( length-- ) {
    uint32_t tick_start = HAL_GetTick();
    while (1) {

      /* THE BARE METAL HACK: Talk directly to USART1 */
      if (__HAL_UART_GET_FLAG(&huart6, UART_FLAG_RXNE) == SET) {
        /* Read the physical register instantly */
        *buffer++ = (uint8_t)(USART6->DR & 0x00FF);
        read_data++;
        break; /* Got the byte, move to the next one! */
      }

      /* Did the hardware crash because a byte came in too fast? */
      if (__HAL_UART_GET_FLAG(&huart6, UART_FLAG_ORE) == SET) {
        __HAL_UART_CLEAR_OREFLAG(&huart6); /* Wipe it clean */
      }

      /* Timeout if the ESP32 stops talking for 500ms */
      if ((HAL_GetTick() - tick_start) > 500) {
        return read_data;
      }
    }
  }
  return read_data;
}

int32_t esp32_io_recv_nb(uint8_t *buffer, uint32_t length) {
  uint32_t read_data = 0;

  while ( length-- ) {
    uint32_t tick_start = HAL_GetTick();
    while (1) {
      if (__HAL_UART_GET_FLAG(&huart6, UART_FLAG_RXNE) == SET) {
        *buffer++ = (uint8_t)(USART6->DR & 0x00FF);
        read_data++;
        break;
      }
      if (__HAL_UART_GET_FLAG(&huart6, UART_FLAG_ORE) == SET) {
        __HAL_UART_CLEAR_OREFLAG(&huart6);
      }
      /* Non-blocking uses a much shorter timeout (50ms) */
      if ((HAL_GetTick() - tick_start) > 50) {
        return read_data;
      }
    }
  }
  return read_data;
}
/**
 * @brief  The Background Receiver Callback (Replaces the broken RxEventCallback)
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  if ( huart == ESP32_UART_HANDLE ) {
    /* 1. Save the caught byte into the ring buffer */
    wifi_rx_buffer.data[wifi_rx_buffer.tail] = rx_byte;

    /* 2. Move the tail forward */
    wifi_rx_buffer.tail = (wifi_rx_buffer.tail + 1) % RING_BUFFER_SIZE;

    /* 3. Re-arm the trap to catch the very next byte */
    HAL_UART_Receive_IT(ESP32_UART_HANDLE, (uint8_t *)&rx_byte, 1);
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
  if (huart == ESP32_UART_HANDLE) {
      esp32_io_error_handler();
  }
}

static void esp32_io_error_handler(void) {
  /* Self-Healing Error Handler */
  __HAL_UART_CLEAR_OREFLAG(ESP32_UART_HANDLE);
  __HAL_UART_CLEAR_NEFLAG(ESP32_UART_HANDLE);
  __HAL_UART_CLEAR_FEFLAG(ESP32_UART_HANDLE);

  /* Restart the interrupt if it crashed */
  HAL_UART_Receive_IT(ESP32_UART_HANDLE, (uint8_t *)&rx_byte, 1);
}
int32_t esp32_io_recv_long(uint8_t *buffer, uint32_t length) {
  uint32_t read_data = 0;

  while ( length-- ) {
    uint32_t tick_start = HAL_GetTick();
    while (1) {
      if (__HAL_UART_GET_FLAG(&huart6, UART_FLAG_RXNE) == SET) {
        *buffer++ = (uint8_t)(USART6->DR & 0x00FF);
        read_data++;
        break;
      }
      if (__HAL_UART_GET_FLAG(&huart6, UART_FLAG_ORE) == SET) {
        __HAL_UART_CLEAR_OREFLAG(&huart6);
      }

      /* THE MASSIVE 20-SECOND AWS TRAP */
      if ((HAL_GetTick() - tick_start) > 20000) {
        return read_data;
      }
    }
  }
  return read_data;
}
