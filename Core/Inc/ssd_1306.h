#ifndef __SSD1306_SPI_H__
#define __SSD1306_SPI_H__

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <string.h>

/* ==================================================================== */
/* OLED HARDWARE CONFIGURATION                                          */
/* ==================================================================== */

// Make sure hspi1 is defined in your main.c!
extern SPI_HandleTypeDef hspi1;

// Safely mapped to GPIOB to avoid USART2 conflict
#define OLED_CS_PORT       GPIOB
#define OLED_CS_PIN        GPIO_PIN_0

#define OLED_DC_PORT       GPIOB
#define OLED_DC_PIN        GPIO_PIN_1

#define OLED_RES_PORT      GPIOB
#define OLED_RES_PIN       GPIO_PIN_2

/* ==================================================================== */
/* OLED DISPLAY SETTINGS                                                */
/* ==================================================================== */

#define SSD1306_WIDTH      128
#define SSD1306_HEIGHT     64
#define SSD1306_BUFFER_SIZE (SSD1306_WIDTH * SSD1306_HEIGHT / 8)

typedef enum {
    SSD1306_COLOR_BLACK = 0x00,
    SSD1306_COLOR_WHITE = 0x01
} SSD1306_Color_t;

/* ==================================================================== */
/* FUNCTION PROTOTYPES                                                  */
/* ==================================================================== */

// Setup & Control
void SSD1306_Init(void);
void SSD1306_UpdateScreen(void);
void SSD1306_Clear(void);

// Drawing
void SSD1306_DrawPixel(uint8_t x, uint8_t y, SSD1306_Color_t color);

// Text
void SSD1306_SetCursor(uint8_t x, uint8_t y);
void SSD1306_WriteChar(char ch, SSD1306_Color_t color);
void SSD1306_WriteString(char* str, SSD1306_Color_t color);

#endif // __SSD1306_SPI_H__
