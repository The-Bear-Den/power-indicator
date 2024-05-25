#include "indicator.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "neopixel.h"

#define TAG "indicator"
#define PIXEL_COUNT 29

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

bool indicator_set_colour(uint8_t red, uint8_t green, uint8_t blue)
{
    for (int ii = 0; ii < PIXEL_COUNT; ii++)
    {
        indicator.pixels[ii].index = ii;
        indicator.pixels[ii].rgb = NP_RGB(red, green, blue);
    };

    return neopixel_SetPixel(indicator.neopixel, indicator.pixels, PIXEL_COUNT);
}
