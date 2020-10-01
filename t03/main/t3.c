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

#define CMD_MAX_LEN 20
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

static int HOURS = 0;
static int MINUTES = 0;
static int SECONDS = 0;

typedef struct {
    uint8_t addr;
    i2c_port_t port;
    uint8_t grid[8][128];          // Pixesl grid (16 * byte(8 bit)) * 128
    uint16_t changes;
} sh1106_t;

typedef struct {
    int seconds;  // the type of timer's event
    int minutes;
    int hours;
} timer_event_t;

#define TIMER_DIVIDER         16  //  Hardware timer clock divider
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds

QueueHandle_t queue = NULL;
QueueHandle_t ram = NULL;
QueueHandle_t error = NULL;

int time_secs_general = 0;
int time_secs_current = 0;
int timer_index = TIMER_1;

uint32_t mins = 0x00000000;
uint32_t hours = 0x00000000;
uint32_t ulNotifiedValueSecs;

bool write = false;

TaskHandle_t xTaskTimeOutput;
TaskHandle_t xTaskOledOutput;

xQueueHandle timer_queue;

void init_i2c() {
    i2c_config_t i2c_config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = GPIO_SDA,
        .scl_io_num = GPIO_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 1000000
    };
    i2c_param_config(SH1106_PORT, &i2c_config);
    i2c_driver_install(SH1106_PORT, I2C_MODE_MASTER, 0, 0, 0);
}

void init_sh1106(sh1106_t *display) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);                   // addind start 3 bits 010
    i2c_master_write_byte(cmd, (display->addr << 1) | I2C_MASTER_WRITE, true);  // Slave address adding
    i2c_master_write_byte(cmd, 0x00, true); // command stream
    i2c_master_write_byte(cmd, 0xAE, true); // off
    i2c_master_write_byte(cmd, 0xD5, true); // clock div
    i2c_master_write_byte(cmd, 0x80, true);
    i2c_master_write_byte(cmd, 0xA8, true); // multiplex
    i2c_master_write_byte(cmd, 0xFF, true);
    i2c_master_write_byte(cmd, 0x8D, true); // charge pump
    i2c_master_write_byte(cmd, 0x14, true);
    i2c_master_write_byte(cmd, 0x10, true); // high column
    i2c_master_write_byte(cmd, 0xB0, true);
    i2c_master_write_byte(cmd, 0xC8, true);
    i2c_master_write_byte(cmd, 0x00, true); // low column
    i2c_master_write_byte(cmd, 0x10, true);
    i2c_master_write_byte(cmd, 0x40, true);
    i2c_master_write_byte(cmd, 0xA1, true); // segment remap
    i2c_master_write_byte(cmd, 0xA6, true);
    i2c_master_write_byte(cmd, 0x81, true); // contrast
    i2c_master_write_byte(cmd, 0xFF, true);
    i2c_master_write_byte(cmd, 0xAF, true); // on
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(display->port, cmd, 10 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
}

void set_pixel_sh1106(sh1106_t *display, uint8_t x, uint8_t y, bool pixel_status) {
    uint8_t page = y / 8;

    if(pixel_status == true) {
        display->grid[page][x] |= (1 << (y % 8));
    }
    else {
        display->grid[page][x] &= ~(1 << (y % 8));
    }
    display->changes |= (1 << page);
}

void print_page(sh1106_t *display, uint8_t page) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (display->addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x80, true);                // single command
    i2c_master_write_byte(cmd, 0xB0 + page, true);         // Goes to our page
    i2c_master_write_byte(cmd, 0x40, true);                // set display start line
    i2c_master_write(cmd, display->grid[page], 128, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(display->port, cmd, 10 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
}

void refresh_sh1106(sh1106_t *display) {
    for(uint8_t y_page = 0; y_page < 16; y_page++) {
        if (display->changes & (1 << y_page)) {
            print_page(display, y_page);
        }       
    }
    display->changes = 0x0000;
}

uint8_t dewrite_byte(uint8_t num, bool up) {
    uint8_t result = 0x00;
    int byte_change = 0;
    uint8_t test;

    if(up == false) {
        byte_change = 3;
    }
    for(int i = 0; i < 8; i++) {
        test = 0x00;
        if(1 & (num >> byte_change)) {
            test += pow(2, i);
        }
        result |= test;
        if(i % 2) {
            byte_change++;
        }
    }
    return result;
}

void print_char(char ch, sh1106_t *display, uint8_t x_str, int size, int page) {
    int index = (ch - 32) * 6;
    uint8_t y_str = 0;

    while(x_str > 128) {
        x_str -= 128;
        y_str++;
    }
    if(size == 1) {
        for(uint8_t x = 0; x < 6; x++) {
            display->grid[page + y_str][x + x_str * 6] |=  (font6x8[index + x]);
        }
    }
    if(size == 2) {
        for(uint8_t x = 0; x < 12; x++) {
            display->grid[page + y_str][x + x_str * 12] |=  dewrite_byte(font6x8[index + x / 2], true);
            display->grid[page + y_str + 1][x + x_str * 12] |=  dewrite_byte(font6x8[index + x / 2], false);
        }
    }
}

void print_string(char *str, sh1106_t *display, int size, int page) {
    int len = strlen(str);

    for(uint8_t i = 0; i < len; i++) {
        print_char(str[i], display, i, size, page);
    }
}

static void IRAM_ATTR clock_isr(void *para) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint32_t timer_intr = timer_group_get_intr_status_in_isr(TIMER_GROUP_0);
    uint64_t timer_counter_value = timer_group_get_counter_value_in_isr(TIMER_GROUP_0, timer_index);

    xTaskNotifyFromISR(xTaskTimeOutput, 1, eIncrement, NULL);

    timer_group_clr_intr_status_in_isr(TIMER_GROUP_0, timer_index);
    timer_group_enable_alarm_in_isr(TIMER_GROUP_0, timer_index);
    portYIELD_FROM_ISR();
    return;
}

void clear_display(sh1106_t *display) {    
    for(uint8_t y = 0; y < 64; y++) {
        for(uint8_t x = 0; x < 128; x++) {
            set_pixel_sh1106(&display, x, y, 0);
        }
    }
}

static void time_output(void *param) {
    sh1106_t *display = (sh1106_t*)param;
    char buff[20];

    while(true) {
        xTaskNotifyWait(0x00000000, 0x00000000, &ulNotifiedValueSecs, portMAX_DELAY);
        if(ulNotifiedValueSecs == 60) {
            mins++;
            ulNotifiedValueSecs = 0;
        }
        if(mins == 60) {
            hours++;
            mins = 0;
        }
        if(hours == 24) {
            hours = 0;
        }
        bzero(&buff, 20);
        clear_display(&display);
        sprintf(&buff, "%d:%d:%d", ulNotifiedValueSecs, mins, hours);
        print_string(buff, &display, 2, 3);
        refresh_sh1106(&display);
    }
}

void draw_image(void *param) {
    sh1106_t *display = (sh1106_t*)param;
    char buff[20];

    while(true) {
        if(write == true) {
            bzero(&buff, 20);
            clear_display(&display);
            sprintf(&buff, "%d:%d:%d", ulNotifiedValueSecs, mins, hours);
            print_string(buff, &display, 2, 3);
            refresh_sh1106(&display);
        }
        vTaskDelay(20);
    }
}

void app_main() {
    sh1106_t display;

    gpio_set_direction(GPIO_NUM_32, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_32, 1);
    init_i2c();
    display.addr = SH1106_ADDR;
    display.port = SH1106_PORT;
    init_sh1106(&display);

    timer_queue = xQueueCreate(10, sizeof(uint32_t));
    timer_config_t clock_cfg = {
        .divider = TIMER_DIVIDER,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = TEST_WITH_RELOAD,
    };
    int timer_idx = 0;
    if(timer_init(TIMER_GROUP_0, timer_index, &clock_cfg) != ESP_OK) {
        printf("Дзвiночок 0\n");
    }
    if(timer_set_counter_value(TIMER_GROUP_0, timer_index, 0) != ESP_OK) {
        printf("Дзвiночок 1\n");
    }
    if(timer_set_alarm_value(TIMER_GROUP_0, timer_index, (1) * TIMER_SCALE) != ESP_OK) {     // means 5 sec.
        printf("Дзвiночок 2\n");
    }
    if(timer_enable_intr(TIMER_GROUP_0, timer_index) != ESP_OK) {
        printf("Дзвiночок 3\n");
    }
    if(timer_isr_register(TIMER_GROUP_0, timer_index, clock_isr, NULL, ESP_INTR_FLAG_IRAM, NULL) != ESP_OK) {
        printf("Дзвiночок 4\n");
    }
    if(timer_start(TIMER_GROUP_0, timer_index) != ESP_OK) {
        printf("Дзвiночок 5\n");
    }
    xTaskCreate(time_output, "time_output", 2048, (void*)&display, 3, &xTaskTimeOutput);
}

// reference to example - https://github.com/espressif/esp-idf/blob/master/examples/peripherals/timer_group/main/timer_group_example_main.c
























