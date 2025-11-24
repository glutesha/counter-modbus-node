#pragma once
#ifndef COUNTER_MODBUS_NODE_HTTP_H
#define COUNTER_MODBUS_NODE_HTTP_H
#include "esp_http_client.h"
#include "esp_log.h"
#include "config.h"

esp_err_t http_event_handler(esp_http_client_event_t *evt);
void send_request(const double volts, const double current, const double power);

#endif //COUNTER_MODBUS_NODE_HTTP_H
