#include <stdio.h>
#include "driver/gpio.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "driver/ledc.h"
#include "driver/timer.h"
#include "driver/uart.h"

#define GPIO_LED1 26
#define GPIO_LED2 27
#define GPIO_LED3 33

#define TXPIN 16
#define RXPIN 17

#define LF_ASCII_CODE 10    // Enter button

#define CMD_MAX_LEN 15      // Max cmd len "led pulse"

QueueHandle_t queue = NULL;

void handle_cmd(void *pvParameters) {
    while(true) {
        char cmd[CMD_MAX_LEN];
        bzero(&cmd, CMD_MAX_LEN);
        xQueueReceive(queue, &cmd, 100);
        if(strstr(cmd, "led on") == cmd) {
            if(strstr(cmd, " 1") != NULL) {
                gpio_set_direction(GPIO_LED1, GPIO_MODE_OUTPUT);
                gpio_set_level(GPIO_LED1, 1);
            }
            if(strstr(cmd, " 2") != NULL) {
                gpio_set_direction(GPIO_LED2, GPIO_MODE_OUTPUT);
                gpio_set_level(GPIO_LED2, 1);
            }
            if(strstr(cmd, " 3") != NULL) {
                gpio_set_direction(GPIO_LED3, GPIO_MODE_OUTPUT);
                gpio_set_level(GPIO_LED3, 1);
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        else if(strstr(cmd, "led off") == cmd) {
            if(strstr(cmd, " 1") != NULL) {
                gpio_set_direction(GPIO_LED1, GPIO_MODE_DISABLE);
            }
            if(strstr(cmd, " 2") != NULL) {
                gpio_set_direction(GPIO_LED2, GPIO_MODE_DISABLE);
            }
            if(strstr(cmd, " 3") != NULL) {
                gpio_set_direction(GPIO_LED3, GPIO_MODE_DISABLE);
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        else if(strstr(cmd, "led pulse") == cmd) {
            if(strstr(cmd, " 1") != NULL) {
                gpio_set_direction(GPIO_LED1, GPIO_MODE_DISABLE);
            }
            if(strstr(cmd, " 2") != NULL) {
                gpio_set_direction(GPIO_LED2, GPIO_MODE_DISABLE);
            }
            if(strstr(cmd, " 3") != NULL) {
                gpio_set_direction(GPIO_LED3, GPIO_MODE_DISABLE);
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    }
    printf("\n\n");
}

void clear_str(int len) {
    printf("\r");
    for(int i = 0; i < len + 1; i++) {
        printf(" ");
    }
    printf("\r");
}

void print_input(char *input) {
    int len = strlen(input);

    printf("\r");
    for(int i = 0; i < len + 1; i++) {
        printf(" ");
    }
    printf("\r");
    printf("%s", input);
}

void input_getter(void *pvParameters) {
    while(true) {
        char cmd[CMD_MAX_LEN];
        char *request = NULL;
        bool end_cmd = false;
        uint8_t buff[CMD_MAX_LEN];

        bzero(&cmd, CMD_MAX_LEN);
        bzero(&buff, CMD_MAX_LEN);
        clear_str(CMD_MAX_LEN);
        for(int ind = 0; ind < (CMD_MAX_LEN - 1) && !end_cmd;) {
            uart_flush_input(UART_NUM_2);
            if(uart_read_bytes(UART_NUM_2, &buff[ind], 1, (50 / portTICK_PERIOD_MS)) == 1) {
                char *tmp = (char *)buff;
                printf("%s", tmp);
                uart_write_bytes(UART_NUM_2, (const char*)tmp, strlen(tmp));
                ind++;
        //         if(cmd[ind] == LF_ASCII_CODE) {
        //             end_cmd = true;
        //             cmd[ind] = '\0';
        //         }
        //         else if(cmd[ind] == 8) {
        //             printf("\033[D");
        //             cmd[ind] = '\0';
        //             cmd[ind - 1] = '\0';
        //             ind--;
        //         }
        //         else
        //             ind++;
        //         print_input(cmd);
        //         vTaskDelay(10 / portTICK_PERIOD_MS);
        //     }
        //     vTaskDelay(10 / portTICK_PERIOD_MS);
        // }
        // xQueueSendToBack(queue, &cmd, 0);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        else {
            printf("hui\n");
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
    }
}
}

void app_main() {
    queue = xQueueCreate(1, CMD_MAX_LEN);
    const int uart_buffer_size = (1024 * 2);

    uart_config_t uart_cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
        .rx_flow_ctrl_thresh = 122
    };
    uart_param_config(UART_NUM_2, &uart_cfg);
    uart_set_pin(UART_NUM_2, RXPIN, TXPIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_2, uart_buffer_size, 0, 0, NULL, 0);
    
    xTaskCreate(input_getter, "input_getter", 4048, NULL, 2, NULL);
    // xTaskCreate(handle_cmd, "handle_cmd", 4048, NULL, 1, NULL);
}
