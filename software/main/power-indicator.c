#include <stdio.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "indicator.h"
#include "network.h"
#include "power.h"

#define UPDATE_RATE_MS 60000

static const char *TAG = "power-indicator";

/** Array of colours used to indicate power price. Indexed by enum power_price_descriptor. */
struct indicator_colour power_colours[POWER_PRICE_NUM] = {
    {0xFF, 0xFF, 0xFF}, {0x00, 0xFF, 0xFF}, {0x00, 0xFF, 0x00}, {0xFF, 0xA5, 0x00},
    {0xFF, 0x8C, 0x00}, {0xFF, 0x00, 0x00}, {0x80, 0x00, 0x80},
};

/** Colour to use on an error condition. */
struct indicator_colour error_colour = {};

static void main_task(void *arg)
{
    struct power_handle *handle = (struct power_handle *)arg;

    while (1)
    {
        double price = power_get_price(handle);
        enum power_price_descriptor price_descriptor = power_get_price_descriptor(handle);

        if (price < 0) {
            ESP_LOGE(TAG, "Failed to fetch current price");
        } else {
            ESP_LOGI(TAG, "Current Price: $%.2f, Price descriptor: %d", price, price_descriptor);
        }

        if (price_descriptor < POWER_PRICE_NUM) {
            indicator_set_colour(power_colours[price_descriptor]);
        } else {
            indicator_set_colour(error_colour);
        }

        vTaskDelay(pdMS_TO_TICKS(UPDATE_RATE_MS));
    }
}

struct power_handle *power;

void app_main(void)
{
    bool ok;
    struct power_config power_config = {0};

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ok = network_init();
    if (!ok) {
        ESP_LOGE(TAG, "network initialisation failed.");
    }

    ok = indicator_init(CONFIG_POWER_INDICATOR_DATA_PIN);
    if (!ok) {
        ESP_LOGE(TAG, "indicator initialisation failed.");
    }

    power = power_init(&power_config);
    if (!power) {
        ESP_LOGE(TAG, "failed to initialise power module.");
    }

    xTaskCreate(main_task, "main", 8192, power, 3, NULL);
}
