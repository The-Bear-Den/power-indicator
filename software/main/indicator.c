#include "indicator.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "neopixel.h"

#define TAG "indicator"
#define FULL_BRIGHTNESS 10
#define PARTIAL_BRIGHTNESS 2

#define MATRIX_WIDTH CONFIG_LED_MATRIX_WIDTH
#define MATRIX_HEIGHT CONFIG_LED_MATRIX_HEIGHT
#define PIXEL_COUNT (MATRIX_WIDTH * MATRIX_HEIGHT)

#define STATUS_SEGMENTS CONFIG_LED_MATRIX_STATUS_SEGMENTS

/** Structure for specifiying RGB colour. */
struct indicator_colour
{
    uint8_t red;   /**< Red element */
    uint8_t green; /**< Green element */
    uint8_t blue;  /**< Blue element */
};

/** Array of colours corresponding to the @c indicator_colour_specifier values. */
static struct indicator_colour colours[] = {
    [RED] = {255, 0, 0},       [GREEN] = {0, 255, 0},    [BLUE] = {0, 0, 255},
    [YELLOW] = {255, 255, 0},  [CYAN] = {0, 255, 255},   [PURPLE] = {255, 0, 255},
    [WHITE] = {255, 255, 255}, [ORANGE] = {255, 165, 0}, [BLACK] = {0, 0, 0}};

struct indicator_handle
{
    tNeopixelContext *neopixel;
    tNeopixel pixels[PIXEL_COUNT];
} indicator;

/**
 * Helper function to set the colour of a given neopixel. Main reason is to allow for configuration
 * of a global brightness setting.
 */
static void set_colour(tNeopixel *pixel, enum indicator_colour_specifier colour, uint8_t brightness)
{
    brightness = (brightness > 100) ? 100 : brightness;

    pixel->rgb =
        NP_RGB((colours[colour].red * brightness) / 100, (colours[colour].green * brightness) / 100,
               (colours[colour].blue * brightness) / 100);
}

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

    for (int ii = 0; ii < PIXEL_COUNT; ii++)
    {
        indicator.pixels[ii].index = ii;
    };

    return neopixel_SetPixel(indicator.neopixel, indicator.pixels, PIXEL_COUNT);
}

static void update_row(uint8_t row_idx, enum indicator_colour_specifier colour, uint8_t percent)
{
    uint32_t pos_offset = row_idx * MATRIX_WIDTH;
    uint32_t num_lit_positions = (percent * MATRIX_WIDTH) / 100;
    uint32_t remainder = (percent * MATRIX_WIDTH) % 100;

    // Determine the direction of iteration
    int start =
        (row_idx % 2 == 0) ? (MATRIX_WIDTH - 1) : 0;  // Even rows: reverse, Odd rows: forward
    int end = (row_idx % 2 == 0) ? -1 : MATRIX_WIDTH; // Reverse uses -1 as the end condition
    int step = (row_idx % 2 == 0) ? -1 : 1;           // Step: -1 for reverse, 1 for forward

    for (int ii = start; ii != end; ii += step)
    {
        if (num_lit_positions > 0)
        {
            num_lit_positions--;
            set_colour(&indicator.pixels[ii + pos_offset], colour, FULL_BRIGHTNESS);
        }
        else if (remainder)
        {
            remainder = 0;
            set_colour(&indicator.pixels[ii + pos_offset], colour, PARTIAL_BRIGHTNESS);
        }
        else
        {
            set_colour(&indicator.pixels[ii + pos_offset], BLACK, 100);
        }
    }
}

bool indicator_set_row(uint8_t row_idx, enum indicator_colour_specifier colour, uint8_t percent)
{
    if (!row_idx)
    {
        ESP_LOGE(TAG, "Index 0 is reserved for the status row.\n");
        return false;
    }

    if (row_idx >= MATRIX_HEIGHT)
    {
        ESP_LOGE(TAG, "Row index (%u) exceeds maxium allowed value (%u)\n", row_idx,
                 MATRIX_HEIGHT - 1);
        return false;
    }

    if (percent > 100)
    {
        percent = 100;
    }

    update_row(row_idx, colour, percent);

    return neopixel_SetPixel(indicator.neopixel, indicator.pixels, PIXEL_COUNT);
}

bool indicator_set_status(uint8_t status_idx, enum indicator_colour_specifier colour)
{
    if (status_idx >= STATUS_SEGMENTS)
    {
        ESP_LOGE(TAG, "Status index (%u) exceeds maxium allow value (%u)\n", status_idx,
                 STATUS_SEGMENTS);
        return false;
    }

    uint32_t num_pixels = (MATRIX_WIDTH / STATUS_SEGMENTS);
    if (status_idx == STATUS_SEGMENTS - 1)
    {
        num_pixels += (MATRIX_WIDTH % STATUS_SEGMENTS);
    }
    uint32_t pos_offset = (MATRIX_WIDTH / STATUS_SEGMENTS) * status_idx;

    for (int ii = MATRIX_WIDTH - 1; num_pixels; ii--, num_pixels--)
    {
        set_colour(&indicator.pixels[ii - pos_offset], colour, FULL_BRIGHTNESS);
    }

    return neopixel_SetPixel(indicator.neopixel, indicator.pixels, PIXEL_COUNT);
}
