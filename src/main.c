#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "driver/uart.h"
#include "mbcontroller.h"
#include "esp_wifi.h"
#include "secrets.h"
#include "nvs_flash.h"

#define HOME_NAME SECRETS_HOME_NAME
#define HTTP_ENDPOINT_HOSTNAME SECRETS_HTTP_ENDPOINT_HOSTNAME
#define HTTP_ENDPOINT_PORT SECRETS_HTTP_ENDPOINT_PORT

#define ESP_WIFI_SSID      SECRETS_ESP_WIFI_SSID
#define ESP_WIFI_PASS      SECRETS_ESP_WIFI_PASSWORD
#define ESP_MAXIMUM_RETRY  10

#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 1024

#define MB_UART_PORT    UART_NUM_1
#define MB_BAUD_RATE    9600
#define MB_SLAVE_ADDR   1

static const char *TAG = "MB_MASTER";

static void *mbc_ctx = NULL;
static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;
static char local_response_buffer[MAX_HTTP_OUTPUT_BUFFER + 1];

esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
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

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void) {
    s_wifi_event_group = xEventGroupCreate();

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = ESP_WIFI_SSID,
            .password = ESP_WIFI_PASS,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 ESP_WIFI_SSID, ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 ESP_WIFI_SSID, ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

static const mb_parameter_descriptor_t device_parameters[] = {
    { .cid = 0,
      .param_key = "Voltage",
      .param_units = "V",
      .mb_slave_addr = MB_SLAVE_ADDR,
      .mb_param_type = MB_PARAM_INPUT,
      .mb_reg_start = 0x00,
      .mb_size = 1,
      .param_type = PARAM_TYPE_U16,
      .param_size = sizeof(uint16_t),
      .access = PAR_PERMS_READ
    },
    { .cid = 1,
      .param_key = "Current",
      .param_units = "A",
      .mb_slave_addr = MB_SLAVE_ADDR,
      .mb_param_type = MB_PARAM_INPUT,
      .mb_reg_start = 0x03,
      .mb_size = 1,
      .param_type = PARAM_TYPE_U16,
      .param_size = sizeof(uint16_t),
      .access = PAR_PERMS_READ
    },
    { .cid = 2,
      .param_key = "Power",
      .param_units = "W",
      .mb_slave_addr = MB_SLAVE_ADDR,
      .mb_param_type = MB_PARAM_INPUT,
      .mb_reg_start = 0x08,
      .mb_size = 1,
      .param_type = PARAM_TYPE_U16,
      .param_size = sizeof(uint16_t),
      .access = PAR_PERMS_READ
    }
};

static const uint16_t num_device_parameters = sizeof(device_parameters) / sizeof(device_parameters[0]);

void uart_init(void) {
    uart_config_t uart_config = {
        .baud_rate = MB_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT
    };
    ESP_ERROR_CHECK(uart_param_config(MB_UART_PORT, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(MB_UART_PORT, 10, 9, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}

static esp_err_t mb_master_init(void) {
    mb_communication_info_t comm = {
        .ser_opts.port = MB_UART_PORT,
        .ser_opts.mode = MB_RTU,
        .ser_opts.baudrate = MB_BAUD_RATE,
        .ser_opts.parity = MB_PARITY_NONE,
        .ser_opts.uid = MB_SLAVE_ADDR,
        .ser_opts.response_tout_ms = 1000,
        .ser_opts.data_bits = UART_DATA_8_BITS,
        .ser_opts.stop_bits = UART_STOP_BITS_1
    };

    esp_err_t err = mbc_master_create_serial(&comm, &mbc_ctx);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mbc_master_create_serial failed: 0x%x", err);
        return err;
    }

    err = mbc_master_set_descriptor(mbc_ctx, device_parameters, num_device_parameters);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mbc_master_set_descriptor failed: 0x%x", err);
        return err;
    }

    err = mbc_master_start(mbc_ctx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mbc_master_start failed: 0x%x", err);
        return err;
    }

    ESP_LOGI(TAG, "Modbus master started");
    return ESP_OK;
}

void send_request(const double volts, const double current, const double power) {
    esp_http_client_config_t config = {
        .host = HTTP_ENDPOINT_HOSTNAME,
        .port = HTTP_ENDPOINT_PORT,
        .path = "/create_point",
        .method = HTTP_METHOD_POST,
        .event_handler = http_event_handler,
        .disable_auto_redirect = true,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    const double resistance = (current == 0.0) ? 0.0 : volts / current;
    char result[200];
    snprintf(result, sizeof(result),
         "{ \"home_num\": \"%s\", \"volts\": %.2f, \"ampers\": %.2f, \"power\": %.2f, \"resistans\": %.2f }",
         HOME_NAME, volts, current, power, resistance);

    ESP_LOGE(TAG, "Sending data: %s", result);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, result, strlen(result));
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %"PRId64,
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

static void master_task(void *arg) {
    while (1) {
        bool get_correct = false;
        double vals[3] = {0, 0, 0};

        for (uint16_t cid = 0; cid < num_device_parameters; cid++) {
            const mb_parameter_descriptor_t *param_info = NULL;
            esp_err_t err = mbc_master_get_cid_info(mbc_ctx, cid, &param_info);
            if (err == ESP_OK && param_info != NULL) {
                uint8_t data_buf[8];
                uint8_t type = 0;
                err = mbc_master_get_parameter(mbc_ctx, param_info->cid, data_buf, &type);
                if (err == ESP_OK) {
                    if (!get_correct) {get_correct = true;}
                    uint32_t raw = 0;
                    memcpy(&raw, data_buf, param_info->param_size);
                    uint16_t value = *(uint16_t*)data_buf;
                    double correct = value;

                    if (cid == 0) correct /=  10.0;
                    if (cid == 1) correct /= 100.0;

                    vals[cid] = correct;
                    ESP_LOGI(TAG, "CID %d %s = %f %s", cid, param_info->param_key, correct, param_info->param_units);

                } else {
                    if (get_correct) {get_correct = false;}
                    ESP_LOGE(TAG, "Read CID %d error: 0x%x", cid, err);
                }
            } else {
                ESP_LOGE(TAG, "CID info %d not found: 0x%x", cid, err);
            }
        }

        if (get_correct) {
            send_request(vals[0], vals[1], vals[2]);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void) {
    uart_init();
    wifi_init_sta();
    if (mb_master_init() == ESP_OK) {
        xTaskCreate(master_task, "master_task", 8192, NULL, 5, NULL);
    }
}
