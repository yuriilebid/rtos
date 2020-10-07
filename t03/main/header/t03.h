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
#include "font6x8.h"
#include "driver/i2c.h"
#include <math.h>

#define DHT11_POWER 2
#define DHT11_DATA  4

#define TXPIN 16
#define RXPIN 17

#define LF_ASCII_CODE 13        // Enter button

#define CMD_MAX_LEN 30
#define ERR_MAX_LEN 40
#define HISTORY_SIZE 60

#define GPIO_SDA GPIO_NUM_21
#define GPIO_SCL GPIO_NUM_22
#define SH1106_ADDR 0x3C        // Deafault sh1106  address
#define SH1106_PORT I2C_NUM_0

#define TIMER_DIVIDER         16  //  Hardware timer clock divider
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds
#define TIMER_INTERVAL0_SEC   (3.4179) // sample test interval for the first timer
#define TIMER_INTERVAL1_SEC   (5.78)   // sample test interval for the second timer
#define TEST_WITHOUT_RELOAD   0        // testing will be done without auto reload
#define TEST_WITH_RELOAD      1        // testing will be done with auto reload

uint32_t HOURS = 0;
uint32_t MINUTES = 0;
int seconds = 0;

typedef struct {
    uint8_t addr;
    i2c_port_t port;
    uint8_t grid[8][128];          // Pixesl grid (16 * byte(8 bit)) * 128
    uint16_t changes;
} sh1106_t;

#define TIMER_DIVIDER         16  //  Hardware timer clock divider
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds

QueueHandle_t queue = NULL;
QueueHandle_t error = NULL;

int time_secs_general = 0;
int time_secs_current = 0;
int timer_index = TIMER_1;

bool command_line_status = false;

sh1106_t display;

uint32_t mins = 0x00000000;
uint32_t hours = 0x00000000;
uint32_t ulNotifiedValueSecs;

bool write = false;

TaskHandle_t xTaskTimeOutput;
TaskHandle_t xTaskOledOutput;
