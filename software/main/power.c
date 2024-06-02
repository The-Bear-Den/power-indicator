#include "power.h"
#include <string.h>
#include "esp_log.h"
#include "esp_tls.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_tls.h"
#include "cJSON.h"

#define WEB_SERVER "www.api.amber.com.au"
#define WEB_PORT "443"
#define WEB_URL "https://api.amber.com.au/v1/sites"
#define DEFAULT_AMBER_API_KEY CONFIG_AMBER_API_KEY

static const char *TAG = "HTTPS_REQUEST";

/* Constants for the web server and the request */
static const char AMBER_SSL_REQUEST[] = "GET " WEB_URL " HTTP/1.1\r\n"
                             "Host: "WEB_SERVER"\r\n"
                             "User-Agent: esp-idf/1.0 esp32\r\n"
                             "Authorization: Bearer " CONFIG_AMBER_API_KEY "\r\n"
                             "Connection: close\r\n"
                             "\r\n";

/* Root cert for server, provided by the binary included in the build */
extern const uint8_t amber_dot_com_ca_pem_start[] asm("_binary_amber_dot_com_ca_pem_start");
extern const uint8_t amber_dot_com_ca_pem_end[]   asm("_binary_amber_dot_com_ca_pem_end");

static void https_get_request(esp_tls_cfg_t cfg, const char *WEB_SERVER_URL, const char *REQUEST)
{
    char buf[512];
    int ret, len;

    esp_tls_t *tls = esp_tls_init();
    if (!tls) {
        ESP_LOGE(TAG, "Failed to allocate esp_tls handle!");
        goto exit;
    }

    if (esp_tls_conn_http_new_sync(WEB_SERVER_URL, &cfg, tls) == 1) {
        ESP_LOGI(TAG, "Connection established...");
    } else {
        ESP_LOGE(TAG, "Connection failed...");
        goto cleanup;
    }

#ifdef CONFIG_EXAMPLE_CLIENT_SESSION_TICKETS
    /* The TLS session is successfully established, now saving the session ctx for reuse */
    if (save_client_session) {
        esp_tls_free_client_session(tls_client_session);
        tls_client_session = esp_tls_get_client_session(tls);
    }
#endif

    size_t written_bytes = 0;
    do {
        ret = esp_tls_conn_write(tls,
                                 REQUEST + written_bytes,
                                 strlen(REQUEST) - written_bytes);
        if (ret >= 0) {
            ESP_LOGI(TAG, "%d bytes written", ret);
            written_bytes += ret;
        } else if (ret != ESP_TLS_ERR_SSL_WANT_READ  && ret != ESP_TLS_ERR_SSL_WANT_WRITE) {
            ESP_LOGE(TAG, "esp_tls_conn_write  returned: [0x%02X](%s)", ret, esp_err_to_name(ret));
            goto cleanup;
        }
    } while (written_bytes < strlen(REQUEST));

    ESP_LOGI(TAG, "Reading HTTP response...");
    do {
        len = sizeof(buf) - 1;
        memset(buf, 0x00, sizeof(buf));
        ret = esp_tls_conn_read(tls, (char *)buf, len);

        if (ret == ESP_TLS_ERR_SSL_WANT_WRITE  || ret == ESP_TLS_ERR_SSL_WANT_READ) {
        ESP_LOGI(TAG, "%d bytes read", len);
            continue;
        } else if (ret < 0) {
            ESP_LOGE(TAG, "esp_tls_conn_read  returned [-0x%02X](%s)", -ret, esp_err_to_name(ret));
            break;
        } else if (ret == 0) {
            ESP_LOGI(TAG, "connection closed");
            break;
        }

        len = ret;
        ESP_LOGI(TAG, "%d bytes read", len);
        /* Print response directly to stdout as it is read */
        for (int i = 0; i < len; i++) {
            putchar(buf[i]);
        }
        putchar('\n'); // JSON output doesn't have a newline at end
    } while (1);

cleanup:
    esp_tls_conn_destroy(tls);
exit:
}

char* https_get_response(const char* request) {
    esp_tls_cfg_t cfg = {
        .cacert_buf = (const unsigned char *) amber_dot_com_ca_pem_start,
        .cacert_bytes = amber_dot_com_ca_pem_end - amber_dot_com_ca_pem_start,
    };

    https_get_request(cfg, WEB_SERVER, AMBER_SSL_REQUEST);

}

void test_request(void)
{
    esp_tls_cfg_t cfg = {
        .cacert_buf = (const unsigned char *) amber_dot_com_ca_pem_start,
        .cacert_bytes = amber_dot_com_ca_pem_end - amber_dot_com_ca_pem_start,
    };

    https_get_request(cfg, WEB_URL, AMBER_SSL_REQUEST);
}

// Function to fetch sites and return the first site ID
char* fetch_site_id(void) {
    char request[1024];
    sprintf(request, "GET %s HTTP/1.1\r\n"
                     "Authorization: Bearer %s\r\n"
                     "Host: api.amber.com.au\r\n"
                     "\r\n", WEB_URL, CONFIG_AMBER_API_KEY);

    // Assume https_get_response() is a function that makes an HTTPS request and returns the response
    char* response = https_get_response(request);

    if (response == NULL) {
        ESP_LOGE(TAG, "Failed to get a valid response for the site ID");
        return site_id;
    }

    // Parse JSON to extract the site ID
    cJSON *json = cJSON_Parse(response);
    cJSON *sites = cJSON_GetObjectItem(json, "sites");
    char* site_id = NULL;
    if (cJSON_GetArraySize(sites) > 0) {
        cJSON *site = cJSON_GetArrayItem(sites, 0);
        site_id = cJSON_GetObjectItem(site, "id")->valuestring;
    }
    cJSON_Delete(json);
    return site_id;
}

// Function to fetch current prices using site ID
double fetch_current_price(char* site_id) {
    if (site_id == NULL) {
        ESP_LOGE(TAG, "Invalid site ID provided");
        return -1.0; // Return an error code or handle as needed
    }

    char url[256];
    sprintf(url, "https://api.amber.com.au/v1/sites/%s/prices/current", site_id);

    char request[1024];
    sprintf(request, "GET %s HTTP/1.1\r\n"
                     "Authorization: Bearer %s\r\n"
                     "Host: api.amber.com.au\r\n"
                     "\r\n", url, CONFIG_AMBER_API_KEY);

    char* response = https_get_response(request);
    if (response == NULL) {
        ESP_LOGE(TAG, "Failed to get a valid response for the current price");
        return -1.0; // Handle error appropriately
    }

    // Parse JSON to extract the current price
    cJSON *json = cJSON_Parse(response);
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to parse response");
        free(response); // Always free memory allocated by https_get_response
        return -1.0;
    }

    cJSON *priceItem = cJSON_GetObjectItem(json, "price");
    if (priceItem == NULL) {
        ESP_LOGE(TAG, "Price information not found");
        cJSON_Delete(json);
        free(response);
        return -1.0;
    }

    double price = priceItem->valuedouble;

    cJSON_Delete(json);
    free(response);
    return price;
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
