#include <stdio.h>
#include "driver/gpio.h"
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "driver/ledc.h"
#include "driver/timer.h"
#include <math.h>
#include "string.h"
#include "driver/i2c.h"
#include <driver/adc.h>
#include "font6x8.h"
#include "driver/dac.h"

#define GPIO_SDA GPIO_NUM_21
#define GPIO_SCL GPIO_NUM_22
#define SH1106_ADDR 0x3C            // Deafault sh1106  address
#define SH1106_PORT I2C_NUM_0

uint8_t brightness_max = 100;

int seconds_to_die = 30;
int miliseconds = 99;

typedef struct {
    uint8_t addr;
    i2c_port_t port;
    uint8_t grid[8][128];          // Pixesl grid (16 * byte(8 bit)) * 128
    uint16_t changes;
} sh1106_t;

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

void set_contrast(sh1106_t *display) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (display->addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x00, true);                // single command
    i2c_master_write_byte(cmd, 0X81, true);                // Goes to our page
    i2c_master_write_byte(cmd, brightness_max, true);      // set display start line
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(display->port, cmd, 10 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
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


void oled(void *pvParameters) {
    sh1106_t display;

    gpio_set_direction(GPIO_NUM_32, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_32, 1);
    init_i2c();
    display.addr = SH1106_ADDR;
    display.port = SH1106_PORT;
    init_sh1106(&display);
    
    // refresh_sh1106(&display);
    while(true) {
        char output[10];

        bzero(output, 10);
        for(uint8_t y = 0; y < 64; y++) {
            for(uint8_t x = 0; x < 128; x++) {
                set_pixel_sh1106(&display, x, y, 0);
            }
        }
        sprintf(output, "  %d:%d", seconds_to_die, miliseconds);
        print_string(output, &display, 2, 3);
        refresh_sh1106(&display);
        printf("%d\n", miliseconds);
        vTaskDelay(5);
    }
}

void timer_change(void *pvParameters) {
    while(true) {
        if(miliseconds <= 0) {
            miliseconds = 99;
            seconds_to_die--;
        }
        if(seconds_to_die == 0) {
            seconds_to_die = 30;
        }
        miliseconds--;
        vTaskDelay(1);
    }
}

void get_light(void *pvParameters) {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_0);
    while(true) {
        brightness_max = ((4095 - adc1_get_raw(ADC1_CHANNEL_0)) / 16) + 10;
        vTaskDelay(1);
    }
    vTaskDelete(NULL);
}

void beep_sound(void *pvParameters) {
    while(true) {
        for(int i = 0; i < 100; i++) {
            for (int j = 100; j < 150; j++) {
                dac_output_voltage(DAC_CHANNEL_1, j);
                // ets_delay_us(1);
                vTaskDelay(1/100);
            }
        }
        vTaskDelay(seconds_to_die * 5 - 5);
    }
    vTaskDelete(NULL);
}

void app_main() {
    gpio_set_direction(GPIO_NUM_5, GPIO_MODE_INPUT);
    dac_output_enable(DAC_CHANNEL_1);

    xTaskCreate(beep_sound, "beep_sound", 2048u, NULL, 5, 0);
    xTaskCreate(oled, "oled", 4048u, NULL, 1, 0);
    xTaskCreate(timer_change, "timer_change", 4048u, NULL, 1, 0); 
}
