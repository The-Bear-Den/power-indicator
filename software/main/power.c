#include "power.h"
#include <string.h>
#include "esp_log.h"
#include "esp_tls.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_tls.h"
#include "cJSON.h"
#include <stdlib.h>

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

#define AMBER_GET_REQUEST_FORMAT_STRING "GET %s HTTP/1.1\r\n" \
                                        "Authorization: Bearer %s\r\n" \
                                        "Host: "WEB_SERVER"\r\n" \
                                        "User-Agent: esp-idf/1.0 esp32\r\n" \
                                        "Connection: close\r\n" \
                                        "\r\n"

/* Root cert for server, provided by the binary included in the build */
extern const uint8_t amber_dot_com_ca_pem_start[] asm("_binary_amber_dot_com_ca_pem_start");
extern const uint8_t amber_dot_com_ca_pem_end[]   asm("_binary_amber_dot_com_ca_pem_end");

#define SITE_ID_MAX_LEN 50

#define RESPONSE_BUFFER_SIZE 2048

struct power_handle
{
    char site_id[SITE_ID_MAX_LEN];
} handle;

/**
 * Function to make an HTTPS GET request
 *
 * @param cfg ESP TLS configuration
 * @param WEB_SERVER_URL URL of the web server
 * @param REQUEST Request to send to the server
 *
 * @return Pointer to the response buffer on success, NULL on failure
 *
 * @warning The calling function must free the memory allocated for the response buffer
 */
static char *https_get_request(esp_tls_cfg_t cfg, const char *WEB_SERVER_URL, const char *REQUEST)
{
    int ret, len;
    char *response = NULL;

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
    response = (char *)calloc(RESPONSE_BUFFER_SIZE, sizeof(char));
    if (response == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for response buffer");
        goto cleanup;
    }

    do {
        len = RESPONSE_BUFFER_SIZE - 1;
        ret = esp_tls_conn_read(tls, (char *)response, len);

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
            putchar(response[i]);
            //detect where the end of the HTTP header is
            if (i > 3 && response[i] == '\n' && response[i - 1] == '\r' && response[i - 2] == '\n' && response[i - 3] == '\r') {
                //skip the HTTP header
                len -= i + 1;
                memmove(response, response + i + 1, len);
                break;
            }
        }
        putchar('\n'); // JSON output doesn't have a newline at end
    } while (1);

cleanup:
    esp_tls_conn_destroy(tls);
exit:
    return response;
}

char* https_get_response(const char* request) {
    esp_tls_cfg_t cfg = {
        .cacert_buf = (const unsigned char *) amber_dot_com_ca_pem_start,
        .cacert_bytes = amber_dot_com_ca_pem_end - amber_dot_com_ca_pem_start,
    };

    return https_get_request(cfg, WEB_URL, request);
}

void test_request(void)
{
    esp_tls_cfg_t cfg = {
        .cacert_buf = (const unsigned char *) amber_dot_com_ca_pem_start,
        .cacert_bytes = amber_dot_com_ca_pem_end - amber_dot_com_ca_pem_start,
    };

    https_get_request(cfg, WEB_URL, AMBER_SSL_REQUEST);
}

/**
 * Function to fetch site ID
 *
 * @param site_id Pointer to a buffer to store the site ID. Must be at least @ref SITE_ID_MAX_LEN bytes long
 *
 * @return true if the site ID was fetched successfully, false otherwise
 */
static bool fetch_site_id(char *site_id)
{
    char request[1024];
    bool result = true;

    snprintf(request, sizeof(request), AMBER_GET_REQUEST_FORMAT_STRING, WEB_URL, CONFIG_AMBER_API_KEY);

    ESP_LOGI(TAG, "Requesting site ID from Amber API");
    char* response = https_get_response(request);

    if (response == NULL) {
        ESP_LOGE(TAG, "Failed to get a valid response for the site ID");
        return false;
    }

    // Parse JSON to extract the site ID
    cJSON *json = cJSON_Parse(response);
    if (json == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            ESP_LOGE(TAG, "Error Parsing Json before: %s\n", error_ptr);
        }
        result = false;
        goto exit;
    }

    ESP_LOGI(TAG, "JSON: %s", cJSON_Print(json));

    int num_sites = cJSON_GetArraySize(json);
    ESP_LOGI(TAG, "Number of sites: %d", num_sites);

    if (num_sites == 0) {
        ESP_LOGE(TAG, "No sites found");
        result = false;
        goto exit;
    }

    cJSON *site = cJSON_GetArrayItem(json, 0);
    snprintf(site_id, SITE_ID_MAX_LEN, "%s", cJSON_GetObjectItem(site, "id")->valuestring);

    cJSON_Delete(json);

exit:
    free(response);
    return result;
}

static cJSON* fetch_json(char* site_id) {
    if (site_id == NULL) {
        ESP_LOGE(TAG, "Invalid site ID provided");
        return NULL;
    }

    char url[256];
    sprintf(url, "https://api.amber.com.au/v1/sites/%s/prices/current", site_id);
    char request[1024];
    snprintf(request, sizeof(request), AMBER_GET_REQUEST_FORMAT_STRING, url, CONFIG_AMBER_API_KEY);
    char* response = https_get_response(request);

    cJSON *json = cJSON_Parse(response);
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to parse response, possible rate limit hit");
    } else {
        ESP_LOGI(TAG, "Received JSON: %s", cJSON_Print(json));
    }
    free(response); // Free response after parsing
    return json;
}


static double fetch_current_price(char* site_id) {
    cJSON *json = fetch_json(site_id);
    if (json == NULL) return DBL_MAX;

    cJSON *firstItem = cJSON_GetArrayItem(json, 0);
    if (firstItem == NULL) {
        ESP_LOGE(TAG, "Failed to access first item of the price data");
        cJSON_Delete(json);
        return DBL_MAX;
    }

    cJSON *perKwhItem = cJSON_GetObjectItem(firstItem, "perKwh");
    if (perKwhItem == NULL || !cJSON_IsNumber(perKwhItem)) {
        ESP_LOGE(TAG, "perKwh field not found or invalid");
        cJSON_Delete(json);
        return DBL_MAX;
    }

    double price = perKwhItem->valuedouble;
    ESP_LOGI(TAG, "Extracted perKwh value: %.2f", price);

    cJSON_Delete(json);
    return price / 100;
}

static char* fetch_current_descriptor(char* site_id) {
    cJSON *json = fetch_json(site_id);
    if (json == NULL) return NULL;

    cJSON *firstItem = cJSON_GetArrayItem(json, 0);
    if (firstItem == NULL) {
        ESP_LOGE(TAG, "Failed to access first item of the price data");
        cJSON_Delete(json);
        return NULL;
    }

    cJSON *descriptorItem = cJSON_GetObjectItem(firstItem, "descriptor");
    if (descriptorItem == NULL || !cJSON_IsString(descriptorItem)) {
        ESP_LOGE(TAG, "Descriptor field not found or invalid");
        cJSON_Delete(json);
        return NULL;
    }

    char* descriptor = strdup(descriptorItem->valuestring);
    cJSON_Delete(json);
    return descriptor; // Need to free this in the calling function
}


struct power_handle *power_init(struct power_config *config)
{
    (void)config;

    if (!fetch_site_id(handle.site_id))
    {
        ESP_LOGE(TAG, "Failed to fetch site ID");
        return NULL;
    }
    ESP_LOGI(TAG, "Site ID: %s", handle.site_id);

    return &handle;
}

double power_get_price(struct power_handle *handle)
{
    return fetch_current_price(handle->site_id);
}

enum power_price_descriptor power_get_price_descriptor(struct power_handle *handle)
{
    char *descriptor = fetch_current_descriptor(handle->site_id);
    if (descriptor == NULL) {
        ESP_LOGE(TAG, "Failed to fetch descriptor");
        return POWER_PRICE_NEUTRAL;  // Default if fetch fails
    }

    enum power_price_descriptor result = POWER_PRICE_NEUTRAL;  // Default value
    if (strcmp(descriptor, "negative") == 0) {
        result = POWER_PRICE_NEGATIVE;
    } else if (strcmp(descriptor, "extremelyLow") == 0) {
        result = POWER_PRICE_EXTREME_LOW;
    } else if (strcmp(descriptor, "veryLow") == 0) {
        result = POWER_PRICE_VERY_LOW;
    } else if (strcmp(descriptor, "low") == 0) {
        result = POWER_PRICE_LOW;
    } else if (strcmp(descriptor, "neutral") == 0) {
        result = POWER_PRICE_NEUTRAL;
    } else if (strcmp(descriptor, "high") == 0) {
        result = POWER_PRICE_HIGH;
    } else if (strcmp(descriptor, "spike") == 0) {
        result = POWER_PRICE_SPIKE;
    }

    free(descriptor);
    return result;
}
