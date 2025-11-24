#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "wifi/wifi.h"
#include "uart/uart.h"
#include "modbus/modbus.h"
#include "http/http.h"

static const char *TAG = "NODE_MASTER";

void app_main(void) {
    uart_init();
    wifi_init_sta();
    if (mb_master_init() == ESP_OK) {
        xTaskCreate(modbus_read_master_task, "master_task", 8192, NULL, 5, NULL);
    } else {
        ESP_LOGE(TAG, "mb_master_init failed!");
    }
}
