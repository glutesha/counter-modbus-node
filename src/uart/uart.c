#include "uart.h"

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
    ESP_ERROR_CHECK(uart_set_pin(MB_UART_PORT, MB_UART_TX, MB_UART_RX, MB_RTS_PIN, UART_PIN_NO_CHANGE));
}
