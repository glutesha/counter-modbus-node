#include "modbus.h"

static const char *TAG = "NODE_MODBUS";

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

const uint16_t num_device_parameters = sizeof(device_parameters) / sizeof(device_parameters[0]);

esp_err_t mb_master_init(void) {
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

void modbus_read_master_task(void *arg) {
  double ring[MB_RING_SIZE][3];
  bool init = false;

  for (int i = 0; i < MB_RING_SIZE; i++) {
    for (int b = 0; b < 3; b++) {
      ring[i][b] = 0;
    }
  }

  while (1) {
    for (int i = 0; i < MB_RING_SIZE; i++) {
      bool get_correct = false;
      double vals[3] = {0, 0, 0};

      for (int cid = 0; cid < 3; cid++) {
        const mb_parameter_descriptor_t *param_info = NULL;
        esp_err_t err = mbc_master_get_cid_info(mbc_ctx, cid, &param_info);
        if (err == ESP_OK && param_info != NULL) {
          int data_buf[8];
          int type = 0;
          err = mbc_master_get_parameter(mbc_ctx, param_info->cid, data_buf, &type);
          if (err == ESP_OK) {
            if (!get_correct) {get_correct = true;}
            uint32_t raw = 0;
            memcpy(&raw, data_buf, param_info->param_size);
            int value = *(uint16_t*)data_buf;
            double correct = value;

            if (cid == 0) correct /= 10.0;
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
        for (int b = 0; b < 3; b++) {
          ring[i][b] = vals[b];
        }
        if (i == MB_RING_SIZE - 1) {
          init = true;
        }
        if (init) {
          double correct_vals[3] = {0, 0, 0};
          for (int b = 0; b < MB_RING_SIZE; b++) {
            for (int c = 0; c < 3; c++) {
              correct_vals[c] += ring[b][c];
            }
          }

          for (int c = 0; c < 3; c++) {
            correct_vals[c] /= MB_RING_SIZE;
          }

          send_request(correct_vals[0], correct_vals[1], correct_vals[2]);
        }
      }
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }
}
