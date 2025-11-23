#include <stdio.h>
#include <driver/uart.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <string.h>
#include <esp_log.h>
#include "sdkconfig.h"

static const char* TAG = "MAIN";

QueueHandle_t uart_event_queue;

void uart_init(void)
{
    uart_config_t uart_config = {
        .baud_rate =  9600,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT
    };

    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1,10,9, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, 1024 * 2, 1024 * 2, 30, &uart_event_queue, 0));
}

void readValues(void* param) {
    uart_event_t event;
    uint16_t value = 0;

    vTaskDelay(100 / portTICK_PERIOD_MS);
}

static esp_err_t master_init(void)
{
    // Initialize Modbus controller
    mb_communication_info_t comm = {
        .ser_opts.port = MB_PORT_NUM,
#if CONFIG_MB_COMM_MODE_ASCII
        .ser_opts.mode = MB_ASCII,
#elif CONFIG_MB_COMM_MODE_RTU
        .ser_opts.mode = MB_RTU,
#endif
        .ser_opts.baudrate = MB_DEV_SPEED,
        .ser_opts.parity = MB_PARITY_NONE,
        .ser_opts.uid = 0,
        .ser_opts.response_tout_ms = 1000,
        .ser_opts.data_bits = UART_DATA_8_BITS,
        .ser_opts.stop_bits = UART_STOP_BITS_1
    };

    esp_err_t err = mbc_master_create_serial(&comm, &master_handle);
    MB_RETURN_ON_FALSE((master_handle != NULL), ESP_ERR_INVALID_STATE, TAG,
                       "mb controller initialization fail.");
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                       "mb controller initialization fail, returns(0x%x).", (int)err);

    const uint8_t override_command = 0x41;
    // Delete the handler for specified command, if available
    err = mbc_delete_handler(master_handle, override_command);
    MB_RETURN_ON_FALSE((err == ESP_OK || err == ESP_ERR_INVALID_STATE), ESP_ERR_INVALID_STATE, TAG,
                       "could not override handler, returned (0x%x).", (int)err);
    err = mbc_set_handler(master_handle, override_command, my_custom_handler);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                       "could not override handler, returned (0x%x).", (int)err);
    mb_fn_handler_fp handler = NULL;
    err = mbc_get_handler(master_handle, override_command, &handler);
    MB_RETURN_ON_FALSE((err == ESP_OK && handler == my_custom_handler), ESP_ERR_INVALID_STATE, TAG,
                       "could not get handler for command %d, returned (0x%x).", (int)override_command, (int)err);

    // Set UART pin numbers
    err = uart_set_pin(MB_PORT_NUM, CONFIG_MB_UART_TXD, CONFIG_MB_UART_RXD,
                       CONFIG_MB_UART_RTS, UART_PIN_NO_CHANGE);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                       "mb serial set pin failure, uart_set_pin() returned (0x%x).", (int)err);

    err = mbc_master_set_descriptor(master_handle, &device_parameters[0], num_device_parameters);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                       "mb controller set descriptor fail, returns(0x%x).", (int)err);

    // Set driver mode to Half Duplex
    err = uart_set_mode(MB_PORT_NUM, UART_MODE_RS485_HALF_DUPLEX);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                       "mb serial set mode failure, uart_set_mode() returned (0x%x).", (int)err);

    vTaskDelay(5);

    err = mbc_master_start(master_handle);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                       "mb controller start fail, returned (0x%x).", (int)err);

    ESP_LOGI(TAG, "Modbus master stack initialized...");
    return err;
}


void app_main(void) {
    uart_init();
    xTaskCreate(readValues, "readvalues", 1024 * 2, NULL, tskIDLE_PRIORITY, NULL);
}
