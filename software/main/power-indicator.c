#include <stdio.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "indicator.h"
#include "network.h"
#include "power.h"

#define UPDATE_RATE_MS 5000

static const char *TAG = "power-indicator";

static void main_task(void *arg)
{
    struct power_handle *handle = (struct power_handle *)arg;

    float price;
    while (1)
    {
        price = power_get_price(handle);
        ESP_LOGI(TAG, "Price $%.2f\n", price);
        (void)indicator_set_colour(0xff, 0x00, 0x00);
        vTaskDelay(pdMS_TO_TICKS(UPDATE_RATE_MS));
    }
}

struct power_handle *power;

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

    ok = network_init();
    if (!ok)
    {
        ESP_LOGE(TAG, "network initialisation failed.");
    }

    ok = indicator_init(GPIO_NUM_2);
    if (!ok)
    {
        ESP_LOGE(TAG, "indicator initialisation failed.");
    }

    power = power_init(&power_config);
    if (!power)
    {
        ESP_LOGE(TAG, "failed to initialise power module.");
    }

    xTaskCreate(main_task, "main", 4096, &power, 3, NULL);
}
