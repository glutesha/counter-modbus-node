#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "mbcontroller.h"

#define MB_UART_PORT    UART_NUM_1
#define MB_BAUD_RATE    9600
#define MB_SLAVE_ADDR   1

static const char *TAG = "MB_MASTER";

static void *mbc_ctx = NULL;

static const mb_parameter_descriptor_t device_parameters[] = {
    { .cid = 0,
      .param_key = "Voltage",
      .param_units = "V",
      .mb_slave_addr = MB_SLAVE_ADDR,
      .mb_param_type = MB_PARAM_INPUT,
      .mb_reg_start = 0x00,
      .mb_size = 1,
      .param_type = PARAM_TYPE_U16,
      .param_size = sizeof(int16_t),
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
      .param_size = sizeof(int16_t),
      .access = PAR_PERMS_READ
    },
    { .cid = 2,
      .param_key = "Power",
      .param_units = "W",
      .mb_slave_addr = MB_SLAVE_ADDR,
      .mb_param_type = MB_PARAM_INPUT   ,
      .mb_reg_start = 0x08,
      .mb_size = 1,
      .param_type = PARAM_TYPE_U16,
      .param_size = sizeof(int16_t),
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



static void master_task(void *arg) {
    while (1) {
        for (uint16_t cid = 0; cid < num_device_parameters; cid++) {
            const mb_parameter_descriptor_t *param_info = NULL;
            esp_err_t err = mbc_master_get_cid_info(mbc_ctx, cid, &param_info);
            if (err == ESP_OK && param_info != NULL) {
                uint8_t data_buf[8];
                uint8_t type = 0;
                err = mbc_master_get_parameter(mbc_ctx, param_info->cid, data_buf, &type);
                if (err == ESP_OK) {
                    uint32_t raw = 0;
                    memcpy(&raw, data_buf, param_info->param_size);
                    int value = *(int*)&raw;
                    float correctvalue = value;
                    if (cid == 0) {
                        correctvalue = correctvalue / 10.0;
                    }
                    if (cid == 1) {
                        correctvalue = correctvalue / 100.0;
                    }
                    ESP_LOGI(TAG, "CID %d %s = %f %s", cid, param_info->param_key, correctvalue, param_info->param_units);
                } else {
                    ESP_LOGE(TAG, "Read CID %d error: 0x%x", cid, err);
                }
            } else {
                ESP_LOGE(TAG, "CID info %d not found: 0x%x", cid, err);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void) {
    uart_init();
    if (mb_master_init() == ESP_OK) {
        xTaskCreate(master_task, "master_task", 4096, NULL, 5, NULL);
    }
}
