#pragma once

#include "driver/gpio.h"
#include <stdint.h>

/** Structure for specifiying RGB colour. */
struct indicator_colour
{
    uint8_t red;   /**< Red element */
    uint8_t green; /**< Green element */
    uint8_t blue;  /**< Blue element */
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
bool indicator_set_colour(struct indicator_colour colour);
