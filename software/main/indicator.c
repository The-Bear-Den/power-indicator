#include "indicator.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "neopixel.h"

#define TAG "indicator"
#define PIXEL_COUNT CONFIG_POWER_INDICATOR_NUM_LED

struct indicator_handle
{
    tNeopixelContext *neopixel;
    tNeopixel pixels[PIXEL_COUNT];
} indicator;

bool indicator_init(gpio_num_t data_pin)
{
    if (indicator.neopixel)
    {
        ESP_LOGW(TAG, "Indicator alreay intialised.");
        return true;
    }

    indicator.neopixel = neopixel_Init(PIXEL_COUNT, data_pin);
    if (!indicator.neopixel)
    {
        ESP_LOGE(TAG, "Failed to initialise indicator.");
        return false;
    }

    return true;
}

bool indicator_set_colour(struct indicator_colour colour)
{
    for (int ii = 0; ii < PIXEL_COUNT; ii++)
    {
        indicator.pixels[ii].index = ii;
        indicator.pixels[ii].rgb = NP_RGB(colour.red, colour.green, colour.blue);
    };

    return neopixel_SetPixel(indicator.neopixel, indicator.pixels, PIXEL_COUNT);
}
