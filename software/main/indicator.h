#pragma once

#include "driver/gpio.h"
#include <stdint.h>

bool indicator_init(gpio_num_t data_pin);

bool indicator_set_colour(uint8_t red, uint8_t green, uint8_t blue);
