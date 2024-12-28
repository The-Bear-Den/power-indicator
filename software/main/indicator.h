#pragma once

#include "driver/gpio.h"
#include <stdint.h>

/** Enum for colour specifiers. */
enum indicator_colour_specifier {
    RED,
    GREEN,
    BLUE,
    YELLOW,
    CYAN,
    MAGENTA,
    WHITE,
    ORANGE,
    PURPLE,
    BLACK,
};

/**
 * Initialise LED indicator module.
 *
 * @param data_pin GPIO pin to use as a data line for the WS2181b LEDs.
 *
 * @return @c true on success, else @c false.
 */
bool indicator_init(gpio_num_t data_pin);

/**
 * Set the LED inidicator to the specified colour.
 *
 * @param colour Struct specifying the RGB values.
 *
 * @return @c true on is colour was successfully set, else @c false.
 */
bool indicator_set_row(uint8_t row_idx, enum indicator_colour_specifier colour, uint8_t percent);

bool indicator_set_status(uint8_t status_idx, enum indicator_colour_specifier colour);
