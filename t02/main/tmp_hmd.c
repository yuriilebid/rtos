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
#include "freertos/timers.h"

#define DHT11_POWER 2
#define DHT11_DATA  4

#define TXPIN 16
#define RXPIN 17

#define LF_ASCII_CODE 13    // Enter button

#define CMD_MAX_LEN 20
#define ERR_MAX_LEN 40
#define HISTORY_SIZE 60

QueueHandle_t queue = NULL;
QueueHandle_t ram = NULL;
QueueHandle_t error = NULL;

int time_secs_general = 0;

typedef struct data_dht {
    int temperature;
    int humidity;
    int time_secs;
    int time_mins;
    int time_hours;
} t_data_dht;

int wait_status(_Bool status) {
    int count = 0;

    while (gpio_get_level(DHT11_DATA) == status) {
        count++;
        ets_delay_us(1);
    }
    return count;
}

void preparing_for_receiving_data() {
    gpio_set_direction(DHT11_DATA,  GPIO_MODE_OUTPUT);
    gpio_set_level(DHT11_DATA, 1);
    ets_delay_us(1500 * 1000);
    gpio_set_level(DHT11_DATA, 0);
    ets_delay_us(18000);
    gpio_set_level(DHT11_DATA, 1);
    ets_delay_us(30);
    gpio_set_direction(DHT11_DATA, GPIO_MODE_INPUT);

    wait_status(0);
    wait_status(1);
}

void weather(void *pvParameters) {
    int res = 0;
    uint8_t data[5];

    while(true) {
        // printf("\n\nError weather 0\n\n");
        t_data_dht measure;
        t_data_dht *dummy_buf;

        bzero(&data, sizeof(data));
        // printf("\n\nError weather 1\n\n");
        preparing_for_receiving_data();
        // printf("\n\nError weather 2\n\n");
        for (int i = 1, j = 0; i < 41; i++) {
            wait_status(0);
            res = wait_status(1);
            if (res > 28) {
                data[j] <<= 1;
                data[j] += 1;
            }
            else {
                data[j] <<= 1;
            }
            if (i % 8 == 0) {
                j++;
            }
        }
        // printf("\n\nError weather 3\n\n");
        if (data[0] + data[1] + data[2] + data[3] != data[4]) {
            printf("Invalid sum\n");
        }
        printf("temp = %d\n", data[2]);
        vTaskDelay(3500 / portTICK_PERIOD_MS);
        measure.temperature = data[2];
        measure.humidity = data[0];
        measure.time_secs = time_secs_general;
        if(uxQueueMessagesWaiting(ram) == HISTORY_SIZE) {
            xQueueReceive(ram, &dummy_buf, 5);
        }
        xQueueSendToBack(ram, &measure, 5);
        time_secs_general += 5;

    }
    vTaskDelete(NULL);
}

int get_full_number(int start_ind, char* str) {
    int number;
    char buff[12];

    bzero(&buff, 12);
    for(int i = start_ind; str[i] < 58 && str[i] > 47 && (i - start_ind) < 12; i++) {
        buff[i - start_ind] = str[i];
    }
    number = atoi(buff);
    return number;
}

void get_range_of_data(int size) {
    t_data_dht data[size + 1];
    char buff[20];
    int count = 0;

    for(int i = 0; i < size && xQueueReceive(ram, &data[i], CMD_MAX_LEN) == pdTRUE; i++) {
        bzero(&buff, 20);
        sprintf(&buff, "\n\rTime ago - %d s", data[i].time_secs);
        uart_write_bytes(UART_NUM_2, buff, strlen(buff));
        bzero(&buff, 20);
        sprintf(&buff, "\n\rTemperatue - %d c", data[i].temperature);
        uart_write_bytes(UART_NUM_2, buff, strlen(buff));
        bzero(&buff, 20);
        sprintf(&buff, "\n\rHumidity - %d %%\n\r", data[i].humidity);
        uart_write_bytes(UART_NUM_2, buff, strlen(buff));
        data[i + 1].humidity = 0;
        count++;
    }
    for(count--; count >= 0; count--) {
        xQueueSendToFront(ram, &data[count], 5);
    }
}

void handle_cmd(void *pvParameters) {
    while(true) {
        char cmd[CMD_MAX_LEN];
        bzero(&cmd, CMD_MAX_LEN);
        xQueueReceive(queue, &cmd, CMD_MAX_LEN);
        t_data_dht measure;

        if(strstr(cmd, "get logs") == cmd)  {
            int amount = get_full_number(9, cmd);
            get_range_of_data(amount);
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void input_getter(void *pvParameters) {
    uint8_t buff[CMD_MAX_LEN];
    uint8_t err[ERR_MAX_LEN];
    char* text_pick = "> ";

    while(true) {
        char *request = NULL;
        bool end_cmd = false;

        bzero(buff, CMD_MAX_LEN);
        bzero(err, ERR_MAX_LEN);
        uart_write_bytes(UART_NUM_2, text_pick, strlen(text_pick));
        for(int ind = 0; ind < (CMD_MAX_LEN - 1) && !end_cmd;) {
            uart_flush_input(UART_NUM_2);
            int exit = uart_read_bytes(UART_NUM_2, &buff[ind], 1, (200 / portTICK_PERIOD_MS));
            if(exit == 1) {
                char *tmp = (char *)&buff[ind];
                if(buff[ind] == LF_ASCII_CODE) {
                    end_cmd = true;
                    buff[ind] = '\0';
                }
                else if(buff[ind] == 127 && ind == 0) {
                    buff[ind] = '\0';
                }
                else if(buff[ind] == 127) {
                    uart_write_bytes(UART_NUM_2, "\033[D \033[D\033[D", strlen("\033[D \033[D"));
                    buff[ind] = '\0';
                    buff[ind - 1] = '\0';
                    ind--;
                }
                else {
                    uart_write_bytes(UART_NUM_2, (const char*)tmp, 1);
                    ind++;
                }
                vTaskDelay(5 / portTICK_PERIOD_MS);
            }
        }
        xQueueSendToBack(queue, &buff, 0);
        if(xQueueReceive(error, &err, 5) != errQUEUE_EMPTY) {
            uart_write_bytes(UART_NUM_2, "\n\r", 2);
            uart_write_bytes(UART_NUM_2, err, strlen((char*)err));

            text_pick = "\e[0;31m> \e[0;39m";
        }
        else {
            text_pick = "\e[0;32m> \e[0;39m";
        }
        uart_write_bytes(UART_NUM_2, "\n\r", 2);
    }
}

void app_main() {
    queue = xQueueCreate(1, CMD_MAX_LEN);
    error = xQueueCreate(1, ERR_MAX_LEN);
    ram = xQueueCreate(HISTORY_SIZE, sizeof(t_data_dht));
    const int uart_buffer_size = (1024 * 2);

    uart_config_t uart_cfg = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    uart_param_config(UART_NUM_2, &uart_cfg);
    uart_set_pin(UART_NUM_2, RXPIN, TXPIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_2, uart_buffer_size, 0, 0, NULL, 0);

    gpio_set_direction(DHT11_POWER, GPIO_MODE_OUTPUT);
    gpio_set_level(DHT11_POWER, 1);

    xTaskCreate(weather, "weather", 4048u, NULL, 2, NULL);
    xTaskCreate(input_getter, "input_getter", 4048u, NULL, 1, NULL);
    xTaskCreate(handle_cmd, "handle_cmd", 4048u, NULL, 1, NULL);
}
