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
enum indicator_colour_specifier power_colours[POWER_PRICE_NUM] = {
    WHITE, CYAN, GREEN, ORANGE, ORANGE, RED, PURPLE,
};

#define NETWORK_STATUS_INDEX 0
#define POWER_STATUS_INDEX 1

#define AMBER_PRICE_ROW_INDEX 1

static void main_task(void *arg)
{
    struct power_handle *handle = (struct power_handle *)arg;

    while (1)
    {
        double price = power_get_price(handle);
        enum power_price_descriptor price_descriptor = power_get_price_descriptor(handle);

        if (price < 0)
        {
            ESP_LOGE(TAG, "Failed to fetch current price");
        }
        else
        {
            ESP_LOGI(TAG, "Current Price: $%.2f, Price descriptor: %d", price, price_descriptor);
        }

        if (price_descriptor < POWER_PRICE_NUM)
        {
            indicator_set_row(AMBER_PRICE_ROW_INDEX, power_colours[price_descriptor], 100);
            indicator_set_status(POWER_STATUS_INDEX, GREEN);
        }
        else
        {
            indicator_set_status(POWER_STATUS_INDEX, RED);
        }

        vTaskDelay(pdMS_TO_TICKS(UPDATE_RATE_MS));
    }
}

struct power_handle *power;

static bool app_start_network(void)
{
    indicator_set_status(0, YELLOW);
    bool ok = network_init();
    if (!ok)
    {
        ESP_LOGE(TAG, "network initialisation failed.");
        indicator_set_status(NETWORK_STATUS_INDEX, RED);
        return false;
    }

    indicator_set_status(NETWORK_STATUS_INDEX, GREEN);
    return true;
}

void error_trap(void)
{
    while (1)
    {
        ESP_LOGE(TAG, "FAILURE OCCURED!");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    bool ok;
    struct power_config power_config = {0};

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ok = indicator_init(CONFIG_POWER_INDICATOR_DATA_PIN);
    if (!ok)
    {
        ESP_LOGE(TAG, "indicator initialisation failed.");
        error_trap();
    }

    ok = app_start_network();
    if (!ok)
    {
        error_trap();
    }

    power = power_init(&power_config);
    if (!power)
    {
        ESP_LOGE(TAG, "failed to initialise power module.");
        indicator_set_status(POWER_STATUS_INDEX, RED);
        error_trap();
    }
    indicator_set_status(POWER_STATUS_INDEX, YELLOW);

    xTaskCreate(main_task, "main", 8192, power, 3, NULL);
}
