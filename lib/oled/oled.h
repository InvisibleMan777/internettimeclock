#ifndef OLED_H
#define OLED_H

#include "esp_lcd_panel_io.h"

int initialize_oled(uint32_t i2c_addr, gpio_num_t sda_gpio, gpio_num_t scl_gpio);
int write_to_oled(const char *line1, const char *line2);

#endif