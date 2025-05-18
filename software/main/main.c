#include "cJSON.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_client.h"
#include "nvs_flash.h"

#include "indicator.h"
#include "network.h"

static const char *TAG = "power-indicator";

/** Array of colours used to indicate power price. Indexed by enum power_price_descriptor. */
enum indicator_colour_specifier power_colours[CONFIG_LED_MATRIX_HEIGHT] = {
    BLUE, CYAN, RED, PURPLE, YELLOW, YELLOW, YELLOW,
};

#define NETWORK_STATUS_INDEX 0
#define MQTT_STATUS_INDEX 1

static void handle_range(int row_idx, cJSON *object)
{
    cJSON *value = cJSON_GetObjectItem(object, "value");
    cJSON *upper_value = cJSON_GetObjectItem(object, "upper_value");

    if (cJSON_IsNumber(value) && cJSON_IsNumber(upper_value))
    {
        int percent = (value->valueint * 100) / upper_value->valueint;
        ESP_LOGI(TAG, "Row %d: Handling range with value=%d, upper_value=%d (percent=%d%%)",
                 row_idx, value->valueint, upper_value->valueint, percent);

        indicator_set_row(row_idx + 1, power_colours[row_idx], percent);
    }
    else
    {
        ESP_LOGW(TAG, "Row %d: Invalid range data (value or upper_value not a number)", row_idx);
    }
}

static void handle_percent(int row_idx, cJSON *object)
{
    cJSON *value = cJSON_GetObjectItem(object, "value");

    if (cJSON_IsNumber(value))
    {
        ESP_LOGI(TAG, "Row %d: Handling percent with value=%d", row_idx, value->valueint);
        indicator_set_row(row_idx + 1, power_colours[row_idx], value->valueint);
    }
    else
    {
        ESP_LOGW(TAG, "Row %d: Invalid percent data (value is not a number)", row_idx);
    }
}

static void process_energy_topic(const char *json_data, size_t json_length)
{
    cJSON *root = cJSON_ParseWithLength(json_data, json_length);
    if (!root)
    {
        ESP_LOGE(TAG, "Error parsing JSON: %s", cJSON_GetErrorPtr());
        return;
    }

    cJSON *rows = cJSON_GetObjectItem(root, "rows");
    if (!cJSON_IsArray(rows))
    {
        ESP_LOGE(TAG, "'rows' is missing or not an array");
        cJSON_Delete(root);
        return;
    }

    cJSON *item = NULL;
    int iteration = 0;

    cJSON_ArrayForEach(item, rows)
    {
        if (!cJSON_IsObject(item))
        {
            ESP_LOGW(TAG, "Skipping invalid item in 'rows' at index %d", iteration);
            iteration++;
            continue;
        }

        cJSON *name_item = cJSON_GetObjectItem(item, "name");
        const char *name = (cJSON_IsString(name_item)) ? name_item->valuestring : "unknown";

        cJSON *type = cJSON_GetObjectItem(item, "type");
        if (!cJSON_IsString(type))
        {
            ESP_LOGW(TAG, "Skipping item '%s': 'type' is missing or not a string", name);
            iteration++;
            continue;
        }

        ESP_LOGI(TAG, "Processing item %d: name='%s', type='%s'", iteration, name,
                 type->valuestring);

        if (strcmp(type->valuestring, "range") == 0)
        {
            handle_range(iteration, item);
        }
        else if (strcmp(type->valuestring, "percent") == 0)
        {
            handle_percent(iteration, item);
        }
        else
        {
            ESP_LOGW(TAG, "Unknown type for item '%s': %s", name, type->valuestring);
        }

        iteration++;
    }

    cJSON_Delete(root);
}

static void handle_mqtt_event_data(esp_mqtt_event_handle_t event)
{
    ESP_LOGI(TAG, "TOPIC=%.*s, Data Len:%d", event->topic_len, event->topic, event->data_len);
    ESP_LOGD(TAG, "DATA=%.*s", event->data_len, event->data);

    if (event->topic_len == strlen(CONFIG_ENERGY_TOPIC)
        && strncmp(event->topic, CONFIG_ENERGY_TOPIC, event->topic_len) == 0)
    {
        process_energy_topic(event->data, event->data_len);
    }
}

/**
 * Log an error message if the error code is non-zero
 *
 * Helper function that logs error messages using ESP's logging system. Only logs
 * if the provided error code is non-zero. The error is logged at the ERROR level
 * with both a descriptive message and the hexadecimal error code.
 *
 * @param[in] message Descriptive message about where/what the error is
 * @param[in] error_code The error code to check and potentially log
 */
static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0)
    {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id,
                               void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");

        msg_id = esp_mqtt_client_subscribe(client, CONFIG_ENERGY_TOPIC, 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        indicator_set_status(MQTT_STATUS_INDEX, GREEN);

        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        indicator_set_status(MQTT_STATUS_INDEX, YELLOW);
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        handle_mqtt_event_data(event);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        indicator_set_status(MQTT_STATUS_INDEX, RED);
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            log_error_if_nonzero("reported from esp-tls",
                                 event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",
                                 event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)",
                     strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_BROKER_URL,
        .credentials.username = CONFIG_BROKER_USERNAME,
        .credentials.authentication.password = CONFIG_BROKER_PASSWORD,
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    if (!client)
    {
        ESP_LOGE(TAG, "Failed to init mqtt client.");
    }

    /* The last argument may be used to pass data to the event handler, in this example
     * mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

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

    mqtt_app_start();
}
