#pragma once
#ifndef COUNTER_MODBUS_NODE_WIFI_H
#define COUNTER_MODBUS_NODE_WIFI_H

#include "config.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"

void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
void wifi_init_sta(void);

#endif //COUNTER_MODBUS_NODE_WIFI_H
