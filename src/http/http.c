#include "http.h"

static const char *TAG = "NODE_HTTP";
static char full_path[sizeof(HTTP_ENDPOINT_PATH) + 32];

esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    static char *output_buffer;
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
            }
            break;
        default:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
    }
    return ESP_OK;
}

void send_request(const double volts, const double current, const double power) {
    snprintf(full_path, sizeof(full_path), "%s/%s", HTTP_ENDPOINT_PATH, SECRETS_HOME_ID);

    esp_http_client_config_t config = {
        .host = HTTP_ENDPOINT_HOSTNAME,
        .port = HTTP_ENDPOINT_PORT,
        .path = full_path,
        .method = HTTP_METHOD_POST,
        .event_handler = http_event_handler,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    char result[200];
    snprintf(result, sizeof(result),

         "{ \"volts\": %.2f, \"ampers\": %.2f, \"power\": %.2f }", volts, current, power);

    ESP_LOGE(TAG, "Sending data: %s", result);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, result, strlen(result));
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %lld",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}
