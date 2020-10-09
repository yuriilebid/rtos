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
#include "driver/dac.h"
#include "driver/spi_master.h"
#define GPIO_POWER 2
#define GPIO_DATA 4

#define GPIO_SDA GPIO_NUM_21
#define GPIO_SCL GPIO_NUM_22
#define SH1106_ADDR 0x3C            // Deafault sh1106  address
#define SH1106_PORT I2C_NUM_0

#define LED_ONE 26
#define LED_TWO 27
#define LED_THREE 33

#define UPDATE_DELAY (200u / portTICK_PERIOD_MS)

#define BW_RATE 0x2Du          // Data rate and power mode control
#define POWER_CTL 0x2Du        // Power-saving features control
#define LOW_POWER 0x08u        // 4-th bit gow low poerw

#define PIN_MISO 12
#define PIN_MOSI 13
#define PIN_CLK 14
#define PIN_CS 15

#define BUTTON_ONE 39
#define BUTTON_TWO 18

#define EN_ACCEL 23

/** @brief ADXL345 register read flag. */
#define ADXL345_REG_READ_FLAG 0x80u
/** @brief ADXL345 register multibyte flag. */
#define ADXL345_REG_MB_FLAG 0x40u
/** @brief ADXL345 register: DATAX0. */
#define ADXL345_REG_DATAX0 0x32u

typedef struct {
    uint8_t addr;
    i2c_port_t port;
    uint8_t grid[16][128];          // Pixesl grid (16 * byte(8 bit)) * 128
    uint16_t changes;
} sh1106_t;

static int x = 0;
static int y = 0;
static int z = 0;
static int led_focus = LED_TWO;

static spi_device_handle_t spi;

sh1106_t display;

int module(int num) {
    if(num < 0) {
        return -1 * num;
    }
    return num;
}

void trans_packet(uint8_t address, uint8_t data) {
	spi_transaction_t packet = {
		.flags     = SPI_TRANS_USE_RXDATA,
		.cmd       = address,
		.tx_buffer = &data,
		.length    = 8
	};
	spi_device_polling_transmit(spi, &packet);
}

void adx1345_read(int16_t *accs) {
    uint8_t tx_buffer[3u * sizeof(uint16_t)];
    spi_transaction_t packet = {
        .tx_buffer = tx_buffer,
    	.cmd       = ADXL345_REG_READ_FLAG |
                   ADXL345_REG_MB_FLAG |
                   ADXL345_REG_DATAX0,
        .length    = sizeof(tx_buffer) * 8,
    	.rx_buffer = accs
    };
    spi_device_polling_transmit(spi, &packet);
}

void spi_init() {
    spi_bus_config_t bus_config = {
    	.miso_io_num   = PIN_MISO,
    	.mosi_io_num   = PIN_MOSI,
    	.sclk_io_num   = PIN_CLK,
    	.quadwp_io_num = -1,
    	.quadhd_io_num = -1
    };
    spi_device_interface_config_t device_config = {
    	.clock_speed_hz = 1000000,
    	.mode           = 3,
    	.spics_io_num   = PIN_CS,
    	.command_bits   = 8,
    	.queue_size     = 1
    };
    spi_bus_initialize(VSPI_HOST, &bus_config, 0);
    spi_bus_add_device(VSPI_HOST, &device_config, &spi);
}

void pulse_led(void *pvParameters) {
	ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .freq_hz = 100,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_1
    };
    ledc_timer_config(&timer_config);

    while(true) {
    	ledc_channel_config_t channel_config = {
            .gpio_num = led_focus,
            .speed_mode = LEDC_HIGH_SPEED_MODE,
            .channel = LEDC_CHANNEL_1,
            .intr_type = LEDC_INTR_FADE_END,
            .timer_sel = LEDC_TIMER_1,
            .duty = 0
        };
        ledc_channel_config(&channel_config);
        ledc_fade_func_install(0);
        ledc_set_fade_with_time(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, 200, 500);
        ledc_fade_start(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, LEDC_FADE_WAIT_DONE);
        ledc_set_fade_with_time(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, 0, 500);
        ledc_fade_start(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, LEDC_FADE_WAIT_DONE);
        vTaskDelay(50);
    }
}

void acceleration(void *pvParameters) {
	int16_t accs[3];

    gpio_set_direction(EN_ACCEL, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_NUM_5, GPIO_MODE_INPUT);
    trans_packet(BW_RATE, LOW_POWER);
	vTaskDelay(UPDATE_DELAY);
	adx1345_read(accs);
	while(true) {
		adx1345_read(accs);
		x = module(accs[0]);
        y = module(accs[1]);
        z = module(accs[2]);
        vTaskDelay(10);
	}
}

void leds_control(void *pvParameters) {
	int led_ind = 0;
	int start_y = module(y);
	int leds_arr[3] = {LED_TWO, LED_ONE, LED_THREE};
    int past_led = 0;

	while(true) {
	    printf("%d  %d  %d\n", x, y, z);
        if(start_y - y > 70) {
        	led_ind++;
        	if(led_ind == 3) {
        		led_ind = 0;
        	}
        	led_focus = leds_arr[led_ind];
        	vTaskDelay(100);
        }
        else if(y - start_y > 70) {
        	led_ind--;
        	if(led_ind == -1) {
        		led_ind = 2;
        	}
        	led_focus = leds_arr[led_ind];
        	vTaskDelay(100);
        }
	    vTaskDelay(10);
    }
}

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

void init_sh1106() {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);                   // addind start 3 bits 010
    i2c_master_write_byte(cmd, (display.addr << 1) | I2C_MASTER_WRITE, true);  // Slave address adding
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
    i2c_master_cmd_begin(display.port, cmd, 10 / portTICK_PERIOD_MS);
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

void set_pixel_sh1106(uint8_t x, uint8_t y, bool pixel_status) {
    uint8_t page = y / 8;

    if(pixel_status == true) {
        display.grid[page][x] |= (1 << (y % 8));
    }
    else {
        display.grid[page][x] &= ~(1 << (y % 8));
    }
    display.changes |= (1 << page);
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

void print_page(uint8_t page) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (display.addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x80, true);                // single command
    i2c_master_write_byte(cmd, 0xB0 + page, true);         // Goes to our page
    i2c_master_write_byte(cmd, 0x40, true);                // set display start line
    i2c_master_write(cmd, display.grid[page], 128, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(display.port, cmd, 10 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
}


/*
 * @Function : 
 *            refresh_sh1106
 *
 * @Description : 
 *               updates all display with array <display.grid>
*/

void refresh_sh1106() {
    for(uint8_t y_page = 0; y_page < 16; y_page++) {
        if (display.changes & (1 << y_page)) {
            print_page(y_page);
        }       
    }
    display.changes = 0x0000;
}


/*
 * @Function : 
 *            dewrite_byte
 *
 * @Parameters : 
 *              num        - Index of ASCII symbol from font6x8.h
 *              up         - Part of symbol to overwrite, <true> if upper byte
 *                           <fasle> if lower byte
 *
 * @Description : 
 *               Upscales symbol X2 from 6 X 8 to 12 X 16
 *
 * @Return : 
 *          result        - byte, witch represents 1/2 of symbol
*/

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


/*
 * @Function : 
 *            print_char
 *
 * @Parameters : 
 *              ch            - Index of ASCII symbol from font6x8.h
 *              x_str         - Index of position on horizontal line of display (dht1106)
 *              size          - Size of symbol to print (<1> - 6 X 8; 
 *                                                       <2> - 12 X 16);
 *              page          - Each 8 horizontal lines, of display(dht1106) (128 X 8 pixels)
 *                              Select line to print on
 *
 * @Description : 
 *               Prints symbol on display
 *
*/

void print_char(char ch, uint8_t x_str, int size, int page) {
    int index = (ch - 32) * 6;
    uint8_t y_str = 0;

    while(x_str > 128) {
        x_str -= 128;
        y_str++;
    }
    if(size == 1) {
        for(uint8_t x = 0; x < 6; x++) {
            display.grid[page + y_str][x + x_str * 6] |=  (font6x8[index + x]);
        }
    }
    if(size == 2) {
        for(uint8_t x = 0; x < 12; x++) {
            display.grid[page + y_str][x + x_str * 12] |=  dewrite_byte(font6x8[index + x / 2], true);
            display.grid[page + y_str + 1][x + x_str * 12] |=  dewrite_byte(font6x8[index + x / 2], false);
        }
    }
}

/*
 * @Function : 
 *            clear_display
 *
 * @Description : 
 *               clear array of display(dht1106)
*/

void clear_display() {    
    for(uint8_t y = 0; y < 64; y++) {
        for(uint8_t x = 0; x < 128; x++) {
            set_pixel_sh1106(x, y, 0);
        }
    }
    refresh_sh1106();
}


/*
 * @Function : 
 *            print_string
 *
 * @Parameters : 
 *              str           - String to print on display
 *              size          - Size of symbols to print (<1> - 6 X 8; 
 *                                                       <2> - 12 X 16);
 *              page          - Each 8 horizontal lines, of display(dht1106) (128 X 8 pixels)
 *                              Select line to print on
 *
 * @Description : 
 *               Prints symbol on display
 *
*/

void print_string(char *str, int size, int page) {
    int len = strlen(str);

    for(uint8_t i = 0; i < len; i++) {
        print_char(str[i], i, size, page);
    }
}

void alarm_window() {

}

void main_window() {

}

void led_window(void *pvParameters) {
    while(true) {
    	char buff[20];

        bzero(&buff, 20);
        clear_display();
        sprintf(&buff, " 1  2  3");
        print_string(buff , 2, 3);
        refresh_sh1106();
    	vTaskDelay(20);
    }
}

void button_one(void *pvParameters) {
    int press_count = 0;
    int led_status = 0;

    gpio_set_direction(BUTTON_ONE, GPIO_MODE_INPUT);
    // gpio_set_direction(LED_ONE, GPIO_MODE_OUTPUT);
    while(true) {
        if(gpio_get_level(BUTTON_ONE) == 1) {
            if(press_count < 4 && led_status == 0) {
                press_count++;
            }
            else if(press_count > 0 && led_status == 1) {
                press_count--;
            }
        }
        else {
            if(press_count >= 40 && led_status == 0) {
                led_status = 1;
            }
            else if(press_count <= 0 && led_status == 1) {
                led_status = 0;
            }
            if(led_status == 1) {
            	leds_control(NULL);
            }
        }
        vTaskDelay(15);
    }
}


void app_main() {
    gpio_set_direction(GPIO_NUM_32, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_32, 1);
    init_i2c();
    display.addr = SH1106_ADDR;
    display.port = SH1106_PORT;
    init_sh1106();

    print_string("Hello", 2, 2);
    refresh_sh1106();
    spi_init();
    xTaskCreate(acceleration, "acceleration", 2048u, NULL, 10, NULL);
    // xTaskCreate(leds_control, "leds_control", 2048u, NULL, 10, 0);
    // xTaskCreate(button_one, "button_one", 2048u, NULL, 10, NULL);
    xTaskCreate(led_window, "led_window", 12040u, NULL, 2, NULL);
}
