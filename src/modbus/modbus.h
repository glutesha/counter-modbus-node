#pragma once
#ifndef COUNTER_MODBUS_NODE_MODBUS_H
#define COUNTER_MODBUS_NODE_MODBUS_H
#include "mbcontroller.h"
#include "config.h"
#include "http/http.h"

esp_err_t mb_master_init(void);
void modbus_read_master_task(void *arg);

#endif //COUNTER_MODBUS_NODE_MODBUS_H
