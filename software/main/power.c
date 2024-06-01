#include "power.h"

#include <string.h>
#include "esp_log.h"
#include "esp_tls.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Constants for the web server and the request */
#define WEB_SERVER "www.api.amber.com.au"
#define WEB_PORT "443"
#define WEB_URL "https://api.amber.com.au/v1/sites"
#define DEFAULT_AMBER_API_KEY CONFIG_AMBER_API_KEY

static const char *TAG = "HTTPS_REQUEST";

/* HTTP request specifics */
static const char AMBER_SSL_REQUEST[] = "GET " WEB_URL " HTTP/1.1\r\n"
                                        "Host: " WEB_SERVER "\r\n"
                                        "User-Agent: esp-idf/1.0 esp32\r\n"
                                        "Authorization: Bearer " CONFIG_AMBER_API_KEY "\r\n"
                                        "\r\n";

/* Root cert for server, provided by the binary included in the build */
extern const uint8_t amber_dot_com_ca_pem_start[] asm("_binary_amber_dot_com_ca_pem_start");
extern const uint8_t amber_dot_com_ca_pem_end[]   asm("_binary_amber_dot_com_ca_pem_end");

/* Function to make an HTTPS GET request */
static void https_get_request_using_cacert_buf(void)
{
    ESP_LOGI(TAG, "https_request using cacert_buf");
    esp_tls_cfg_t cfg = {
        .cacert_buf = (const unsigned char *) amber_dot_com_ca_pem_start,
        .cacert_bytes = amber_dot_com_ca_pem_end - amber_dot_com_ca_pem_start,
    };

    char buf[512];
    int ret, len;

    esp_tls_t *tls = esp_tls_init();
    if (!tls) {
        ESP_LOGE(TAG, "Failed to allocate esp_tls handle!");
        return;
    }

    if (esp_tls_conn_http_new_sync(WEB_URL, &cfg, tls) != 1) {
        ESP_LOGE(TAG, "Connection failed...");
        goto cleanup;
    }

    ESP_LOGI(TAG, "Connection established...");

    size_t written_bytes = 0;
    do {
        ret = esp_tls_conn_write(tls,
                                 AMBER_SSL_REQUEST + written_bytes,
                                 strlen(AMBER_SSL_REQUEST) - written_bytes);
        if (ret < 0) {
            ESP_LOGE(TAG, "esp_tls_conn_write returned: [0x%02X](%s)", ret, esp_err_to_name(ret));
            goto cleanup;
        }
        written_bytes += ret;
    } while (written_bytes < strlen(AMBER_SSL_REQUEST));

    ESP_LOGI(TAG, "Reading HTTP response...");
    do {
        len = sizeof(buf) - 1;
        memset(buf, 0x00, sizeof(buf));
        ret = esp_tls_conn_read(tls, (char *)buf, len);
        if (ret < 0) {
            ESP_LOGE(TAG, "esp_tls_conn_read returned [-0x%02X](%s)", -ret, esp_err_to_name(ret));
            break;
        }
        if (ret == 0) {
            ESP_LOGI(TAG, "connection closed");
            break;
        }
        ESP_LOGD(TAG, "%d bytes read", ret);
        /* Print response directly to stdout as it is read */
        for (int i = 0; i < ret; i++) {
            putchar(buf[i]);
        }
        putchar('\n');
    } while (1);

cleanup:
    esp_tls_conn_destroy(tls);
}

void start_fetch_pricing_task(void)
{
    // Initialize network interface and start the HTTPS request task
    xTaskCreate(&https_get_request_using_cacert_buf, "https_get_task", 8192, NULL, 5, NULL);
}

struct power_handle
{
    uint8_t reserved;
} handle;

struct power_handle *power_init(struct power_config *config)
{
    (void)config;

    return &handle;
}

double power_get_price(struct power_handle *handle)
{
    (void)handle;

    return 0.0;
}

enum power_price_descriptor power_get_price_descriptor(struct power_handle *handle, const char *descriptor)
{
    if (strcmp(descriptor, "extremelyLow") == 0) return POWER_PRICE_EXTREME_LOW;
    if (strcmp(descriptor, "veryLow") == 0) return POWER_PRICE_VERY_LOW;
    if (strcmp(descriptor, "low") == 0) return POWER_PRICE_LOW;
    if (strcmp(descriptor, "neutral") == 0) return POWER_PRICE_NEUTRAL;
    if (strcmp(descriptor, "high") == 0) return POWER_PRICE_HIGH;
    if (strcmp(descriptor, "spike") == 0) return POWER_PRICE_SPIKE;

    return POWER_PRICE_NEUTRAL; // Default if no match
}
