#ifndef WAVESHARE_EPAPER_H
#define WAVESHARE_EPAPER_H

#include "gpio.h"
#include "spi.h"

typedef enum {
    EPAPER_TYPE_2IN9BC,        // 2.9" (B) and (C)
} EpaperType;

typedef struct {
    EpaperType type;

    SPI_HandleTypeDef *hspi;

    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;
    GPIO_TypeDef *dc_port;
    uint16_t dc_pin;
    GPIO_TypeDef *rst_port;
    uint16_t rst_pin;
    GPIO_TypeDef *busy_port;
    uint16_t busy_pin;
    GPIO_TypeDef *pwr_port;
    uint16_t pwr_pin;
} Epaper;

typedef enum {
    IMAGE_ROTATION_0,
    IMAGE_ROTATION_90,
    IMAGE_ROTATION_180,
    IMAGE_ROTATION_270,
} ImageRotation;

typedef struct {
    uint16_t width;
    uint16_t height;
    ImageRotation rotation;
    uint8_t *b_data;
    uint8_t *ry_data;
    uint16_t size;
} Image;

typedef enum {
    COLOR_WHITE = 0,
    COLOR_BLACK = 1,
    COLOR_RED_YELLOW = 2,
    COLOR_TRANSPARENT = 3,
} Color;

typedef enum {
    PAINT_FLAG_DEFAULT = 0,
    PAINT_FLAG_W_TO_B = 1 << 0,
    PAINT_FLAG_W_TO_RY = 1 << 1,
    PAINT_FLAG_B_TO_W = 1 << 2,
    PAINT_FLAG_B_TO_RY = 1 << 3,
    PAINT_FLAG_RY_TO_W = 1 << 4,
    PAINT_FLAG_RY_TO_B = 1 << 5,
    PAINT_FLAG_SWAP_W_B = (1 << 0) | (1 << 2),
    PAINT_FLAG_SWAP_W_RY = (1 << 1) | (1 << 4),
    PAINT_FLAG_SWAP_B_RY = (1 << 3) | (1 << 5),
} StampFlag;

typedef struct {
    uint16_t width;
    uint16_t height;
    const uint8_t *data;
} Stamp;

void epaper_init(Epaper *epaper);
void epaper_finalize(Epaper *epaper);

uint16_t epaper_width(Epaper *epaper);
uint16_t epaper_height(Epaper *epaper);

void epaper_display(Epaper *epaper, const Image *image);
void epaper_clear(Epaper *epaper);

void image_alloc(Image *image, uint16_t width, uint16_t height, ImageRotation rotation);
void image_free(Image *image);

void image_set_pixel(Image *image, int16_t x, int16_t y, Color color);
void image_fill(Image *image, Color color);

void image_add_rectangle(Image *image, int16_t x, int16_t y, uint16_t width, uint16_t height, Color color);
void image_add_rectangle_border(Image *image, int16_t x, int16_t y, uint16_t width, uint16_t height,
                                uint16_t border_width, Color color, Color border_color);

void image_add_stamp(Image *image, const Stamp *paint, int16_t x, int16_t y, StampFlag flag);

#endif
