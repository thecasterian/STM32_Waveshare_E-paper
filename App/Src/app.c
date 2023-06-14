#include <stdio.h>

#include "app.h"
#include "epaper.h"
#include "usart.h"

void app_main(void) {
    Epaper epaper = {
        .type = EPAPER_TYPE_2IN9BC,
        .hspi = &hspi1,
        .cs_port = CS_GPIO_Port,
        .cs_pin = CS_Pin,
        .dc_port = DC_GPIO_Port,
        .dc_pin = DC_Pin,
        .rst_port = RST_GPIO_Port,
        .rst_pin = RST_Pin,
        .busy_port = BUSY_GPIO_Port,
        .busy_pin = BUSY_Pin,
        .pwr_port = PWR_GPIO_Port,
        .pwr_pin = PWR_Pin,
    };
    Image image;

    image_alloc(
        &image,
        epaper_width(&epaper),
        epaper_height(&epaper),
        IMAGE_ROTATION_0
    );

    image_add_rectangle(&image, 10, 10, 30, 20, COLOR_RED_YELLOW);
    image_add_rectangle_border(&image, 25, 20, 40, 20, 3, COLOR_TRANSPARENT, COLOR_BLACK);

    epaper_init(&epaper);

    epaper_display(&epaper, &image);

    epaper_finalize(&epaper);

    image_free(&image);
}

int _write(int file, char *ptr, int len) {
    HAL_UART_Transmit(&huart6, (uint8_t *) ptr, len, 1000);
    return len;
}
