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

#define LF_ASCII_CODE 13

#define CMD_MAX_LEN 20
#define ERR_MAX_LEN 40
#define HISTORY_SIZE 60

QueueHandle_t queue = NULL;
QueueHandle_t ram = NULL;
QueueHandle_t error = NULL;

TaskHandle_t xTaskWeather;

bool command_line_status = true;

int time_secs_general = 0;
int time_secs_current = 0;

typedef struct data_dht {
    int temperature;
    int humidity;
    int time_secs;
    int time_mins;
    int time_hours;
} t_data_dht;
