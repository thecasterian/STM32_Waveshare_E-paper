#include <stdlib.h>
#include <string.h>

#include "epaper.h"

static void send_command_data(Epaper *epaper, uint8_t cmd, uint8_t *data, uint16_t size);
static void wait_busy(Epaper *epaper);
static Color change_color(Color color, StampFlag flag);

static int16_t min(int16_t x, int16_t y);
static int16_t max(int16_t x, int16_t y);

void epaper_init(Epaper *epaper) {
    /* Power on. */
    HAL_GPIO_WritePin(epaper->pwr_port, epaper->pwr_pin, GPIO_PIN_SET);

    /* Reset. */
    HAL_GPIO_WritePin(epaper->rst_port, epaper->rst_pin, GPIO_PIN_SET);
    HAL_Delay(200);
    HAL_GPIO_WritePin(epaper->rst_port, epaper->rst_pin, GPIO_PIN_RESET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(epaper->rst_port, epaper->rst_pin, GPIO_PIN_SET);
    HAL_Delay(200);

    /* Send initialization commands. */
    switch (epaper->type) {
    case EPAPER_TYPE_2IN9BC:
        send_command_data(epaper, 0x06, (uint8_t []){0x17, 0x17, 0x17}, 3);
        send_command_data(epaper, 0x04, NULL, 0);
        wait_busy(epaper);
        /* Resolution: 128 x 296; LUT from OTP; BWR mode. */
        send_command_data(epaper, 0x00, (uint8_t []){0x8F}, 1);
        /* Resolution: 128 x 296. */
        send_command_data(epaper, 0x61, (uint8_t []){0x80, 0x01, 0x28}, 3);
        /* Polarity: white is 0 for both channels (DDX = 00); Border data with LUTW. */
        send_command_data(epaper, 0x50, (uint8_t []){0x87}, 1);
    }

    HAL_Delay(500);
}

void epaper_finalize(Epaper *epaper) {
    /* Send finalization commands. */
    send_command_data(epaper, 0x02, NULL, 0);
    wait_busy(epaper);
    send_command_data(epaper, 0x07, (uint8_t []){0xA5}, 1);

    /* Power off. */
    HAL_GPIO_WritePin(epaper->pwr_port, epaper->pwr_pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(epaper->rst_port, epaper->rst_pin, GPIO_PIN_RESET);
}

uint16_t epaper_width(Epaper *epaper) {
    switch (epaper->type) {
    case EPAPER_TYPE_2IN9BC:
        return 128;
    }
    return 0;
}

uint16_t epaper_height(Epaper *epaper) {
    switch (epaper->type) {
    case EPAPER_TYPE_2IN9BC:
        return 296;
    }
    return 0;
}

void epaper_display(Epaper *epaper, const Image *image) {
    if (image->width != epaper_width(epaper) || image->height != epaper_height(epaper)) {
        /* Image size does not match the e-paper size. */
        return;
    }

    send_command_data(epaper, 0x10, image->b_data, image->size);
    send_command_data(epaper, 0x13, image->ry_data, image->size);
    send_command_data(epaper, 0x12, NULL, 0);
    wait_busy(epaper);
}

void epaper_clear(Epaper *epaper) {
    uint16_t size = (epaper_width(epaper) / 8) * epaper_height(epaper);

    HAL_GPIO_WritePin(epaper->cs_port, epaper->cs_pin, GPIO_PIN_RESET);

    HAL_GPIO_WritePin(epaper->dc_port, epaper->dc_pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(epaper->hspi, (uint8_t []){0x10}, 1, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(epaper->dc_port, epaper->dc_pin, GPIO_PIN_SET);
    for (uint16_t i = 0; i < size; i++) {
        HAL_SPI_Transmit(epaper->hspi, (uint8_t []){0x00}, 1, HAL_MAX_DELAY);
    }

    HAL_GPIO_WritePin(epaper->dc_port, epaper->dc_pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(epaper->hspi, (uint8_t []){0x13}, 1, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(epaper->dc_port, epaper->dc_pin, GPIO_PIN_SET);
    for (uint16_t i = 0; i < size; i++) {
        HAL_SPI_Transmit(epaper->hspi, (uint8_t []){0x00}, 1, HAL_MAX_DELAY);
    }

    HAL_GPIO_WritePin(epaper->cs_port, epaper->cs_pin, GPIO_PIN_SET);

    send_command_data(epaper, 0x12, NULL, 0);
    wait_busy(epaper);
}

void image_alloc(Image *image, uint16_t width, uint16_t height, ImageRotation rotation) {
    image->width = width;
    image->height = height;
    image->rotation = rotation;

    image->size = (width / 8) * height;

    image->b_data = malloc(image->size);
    image->ry_data = malloc(image->size);

    if (image->b_data == NULL || image->ry_data == NULL) {
        /* Alloation failed. */
        free(image->b_data);
        free(image->ry_data);
        return;
    }

    /* Clear the image. */
    memset(image->b_data, 0x00, image->size);
    memset(image->ry_data, 0x00, image->size);
}

void image_free(Image *image) {
    free(image->b_data);
    free(image->ry_data);
}

void image_set_pixel(Image *image, int16_t x, int16_t y, Color color) {
    if (x < 0 || x >= (int16_t)(image->width) || y < 0 || y >= (int16_t)(image->height)) {
        /* Out of bounds. */
        return;
    }

    uint16_t idx = (uint16_t)(y) * (image->width / 8) + (uint16_t)(x) / 8;
    uint16_t bit = 7 - (uint16_t)(x) % 8;

    switch (color) {
    case COLOR_WHITE:
        image->b_data[idx] &= ~(1 << bit);
        image->ry_data[idx] &= ~(1 << bit);
        break;
    case COLOR_BLACK:
        image->b_data[idx] |= (1 << bit);
        image->ry_data[idx] &= ~(1 << bit);
        break;
    case COLOR_RED_YELLOW:
        image->b_data[idx] &= ~(1 << bit);
        image->ry_data[idx] |= (1 << bit);
        break;
    default:
        break;
    }
}

void image_fill(Image *image, Color color) {
    switch (color) {
    case COLOR_WHITE:
        memset(image->b_data, 0x00, image->size);
        memset(image->ry_data, 0x00, image->size);
        break;
    case COLOR_BLACK:
        memset(image->b_data, 0xFF, image->size);
        memset(image->ry_data, 0x00, image->size);
        break;
    case COLOR_RED_YELLOW:
        memset(image->b_data, 0x00, image->size);
        memset(image->ry_data, 0xFF, image->size);
        break;
    default:
        break;
    }
}

void image_add_rectangle(Image *image, int16_t x, int16_t y, uint16_t width, uint16_t height, Color color) {
    int16_t min_x = max(x, 0);
    int16_t min_y = max(y, 0);
    int16_t max_x = min(x + (int16_t)(width), (int16_t)(image->width));
    int16_t max_y = min(y + (int16_t)(height), (int16_t)(image->height));

    for (int16_t j = min_y; j < max_y; j++) {
        for (int16_t i = min_x; i < max_x; i++) {
            image_set_pixel(image, i, j, color);
        }
    }
}

void image_add_rectangle_border(Image *image, int16_t x, int16_t y, uint16_t width, uint16_t height,
                                uint16_t border_width, Color color, Color border_color) {
    if (border_width * 2 >= width || border_width * 2 >= height) {
        /* Border too large. */
        return;
    }

    if (border_width > 0) {
        image_add_rectangle(image, x, y, width, border_width, border_color);
        image_add_rectangle(image, x, y + (int16_t)(height) - (int16_t)(border_width), width, border_width,
                            border_color);
        image_add_rectangle(image, x, y + (int16_t)(border_width), border_width, height - border_width * 2,
                            border_color);
        image_add_rectangle(image, x + (int16_t)(width) - (int16_t)(border_width), y + (int16_t)(border_width),
                            border_width, height - border_width * 2, border_color);
        image_add_rectangle(image, x + (int16_t)(border_width), y + (int16_t)(border_width), width - border_width * 2,
                            height - border_width * 2, color);
    } else {
        image_add_rectangle(image, x, y, width, height, color);
    }
}

void image_add_stamp(Image *image, const Stamp *paint, int16_t x, int16_t y, StampFlag flag) {
    if (x + (int16_t)(paint->width) < 0 || x >= (int16_t)(image->width) || y + (int16_t)(paint->height) < 0 ||
        y >= (int16_t)(image->height)) {
        /* Out of bounds. */
        return;
    }

    uint16_t width = paint->width < image->width - x ? paint->width : image->width - x;
    uint16_t height = paint->height < image->height - y ? paint->height : image->height - y;

    for (uint16_t j = 0; j < height; j++) {
        for (uint16_t i = 0; i < width; i++) {
            uint16_t image_idx = (y + j) * (image->width / 8) + (x + i) / 8;
            uint16_t paint_idx = j * (paint->width / 4) + i / 4;
            uint16_t image_bit = 7 - (x + i) % 8;
            uint16_t paint_bit = (3 - i % 4) * 2;

            Color v = (paint->data[paint_idx] >> paint_bit) & 0x03;
            v = change_color(v, flag);

            switch (v) {
            case COLOR_WHITE:
                /* White */
                image->b_data[image_idx] &= ~(1 << image_bit);
                image->ry_data[image_idx] &= ~(1 << image_bit);
                break;
            case COLOR_BLACK:
                /* Black. */
                image->b_data[image_idx] |= (1 << image_bit);
                image->ry_data[image_idx] &= ~(1 << image_bit);
                break;
            case COLOR_RED_YELLOW:
                /* Red/yellow. */
                image->b_data[image_idx] &= ~(1 << image_bit);
                image->ry_data[image_idx] |= (1 << image_bit);
                break;
            default:
                /* Transparent; nothing to do. */
                break;
            }
        }
    }
}

static void send_command_data(Epaper *epaper, uint8_t cmd, uint8_t *data, uint16_t size) {
    HAL_GPIO_WritePin(epaper->cs_port, epaper->cs_pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(epaper->dc_port, epaper->dc_pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(epaper->hspi, &cmd, 1, HAL_MAX_DELAY);
    if (data != NULL) {
        HAL_GPIO_WritePin(epaper->dc_port, epaper->dc_pin, GPIO_PIN_SET);
        HAL_SPI_Transmit(epaper->hspi, (uint8_t *)data, size, HAL_MAX_DELAY);
        HAL_GPIO_WritePin(epaper->dc_port, epaper->dc_pin, GPIO_PIN_RESET);
    }
    HAL_GPIO_WritePin(epaper->cs_port, epaper->cs_pin, GPIO_PIN_SET);
}

static void wait_busy(Epaper *epaper) {
    while (HAL_GPIO_ReadPin(epaper->busy_port, epaper->busy_pin) == GPIO_PIN_RESET) {
        HAL_Delay(100);
    }
}

static Color change_color(Color color, StampFlag flag) {
    switch (color) {
    case COLOR_WHITE:
        if (flag & PAINT_FLAG_W_TO_B) {
            return COLOR_BLACK;
        } else if (flag & PAINT_FLAG_W_TO_RY) {
            return COLOR_RED_YELLOW;
        }
        break;
    case COLOR_BLACK:
        if (flag & PAINT_FLAG_B_TO_W) {
            return COLOR_WHITE;
        } else if (flag & PAINT_FLAG_B_TO_RY) {
            return COLOR_RED_YELLOW;
        }
        break;
    case COLOR_RED_YELLOW:
        if (flag & PAINT_FLAG_RY_TO_W) {
            return COLOR_WHITE;
        } else if (flag & PAINT_FLAG_RY_TO_B) {
            return COLOR_BLACK;
        }
        break;
    default:
        break;
    }

    return color;
}

static int16_t min(int16_t x, int16_t y) {
    return x < y ? x : y;
}

static int16_t max(int16_t x, int16_t y) {
    return x > y ? x : y;
}
