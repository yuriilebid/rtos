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

#define GPIO_SDA GPIO_NUM_21
#define GPIO_SCL GPIO_NUM_22
#define SH1106_ADDR 0x3C            // Deafault sh1106  address
#define SH1106_PORT I2C_NUM_0

uint8_t brightness_max = 100;

typedef struct {
    uint8_t addr;
    i2c_port_t port;
    uint8_t grid[8][128];          // Pixesl grid (16 * byte(8 bit)) * 128
    uint16_t changes;
} sh1106_t;


/*
 * @Function : 
 *            init_i2c
 *
 * @Description : 
 *               Configurates i2c interface. I2C used for display(sh1106)
*/

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


/*
 * @Function : 
 *            init_sh1106
 *
 * @Description : 
 *               Configurates display(sh1106) settings
*/

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


/*
 * @Function : 
 *            set_pixel_sh1106
 *
 * @Parameters : 
 *              x                - position of horisontal line of display(dht1106)
 *              y                - position of vertical line of display(dht1106)
 *              pixel_status     - <true> to light on pixel, <false>  to light off
 *
 * @Description : 
 *               Sets pixel status (on/off) in display array of pixels
*/

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


/*
 * @Function : 
 *            print_page
 *
 * @Parameters : 
 *              page        - Each 8 horizontal lines, of display(dht1106) (128 X 8 pixels)
 *
 * @Description : 
 *               Updates one page of display (128 X 8 pixels) with array <display.grid>
*/

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


/*
 * @Function : 
 *            refresh_sh1106
 *
 * @Description : 
 *               updates all display with array <display.grid>
*/

void refresh_sh1106(sh1106_t *display) {
    for(uint8_t y_page = 0; y_page < 16; y_page++) {
        if (display->changes & (1 << y_page)) {
            print_page(display, y_page);
        }       
    }
    display->changes = 0x0000;
}


/*
 * @Function : 
 *            set_contrast
 *
 * @Parameters :
 *              display        - structure handles sh1106(display)
 *
 * @Description : 
 *               Set brightness according to value of global variable <brightness_max>
*/

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


/*
 * @Function : 
 *            oled
 *
 * @Parameters :
 *              pvParameters        - NULL (needs to be a task)
 *
 * @Description : 
 *               Sets sh1106(display) and control of changing pictures frames (each 10 ticks)
*/

void oled(void *pvParameters) {
    sh1106_t display;

    gpio_set_direction(GPIO_NUM_32, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_32, 1);
    init_i2c();
    display.addr = SH1106_ADDR;
    display.port = SH1106_PORT;
    init_sh1106(&display);
    for(uint8_t y = 0; y < 64; y++) {
        for(uint8_t x = 0; x < 128; x++) {
            set_pixel_sh1106(&display, x, y, 1);
        }
    }
    refresh_sh1106(&display);
    while(true) {
        set_contrast(&display);
        refresh_sh1106(&display);
        printf("%d\n", brightness_max);
        vTaskDelay(10);
    }
}


/*
 * @Function : 
 *            get_light
 *
 * @Parameters :
 *              pvParameters        - NULL (needs to be a task)
 *
 * @Description : 
 *               Gets current brightness around device. Uses ADC (Analog)
 *               to digital converter
*/

void get_light(void *pvParameters) {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_0);
    while(true) {
        brightness_max = ((4095 - adc1_get_raw(ADC1_CHANNEL_0)) / 16) + 10;
        vTaskDelay(10);
    }
    vTaskDelete(NULL);
}


void app_main() {
    xTaskCreate(get_light, "get_light", 4048u, NULL, 1, 0);
    xTaskCreate(oled, "oled", 4048u, NULL, 1, 0); 
}
