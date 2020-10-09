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

static int x = 0;
static int y = 0;
static int z = 0;

static int led_focus = LED_TWO;

static spi_device_handle_t spi;

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

void alarm_window() {

}

void main_window() {

}

void button_one(void *pvParameters) {
    int press_count = 0;
    int led_status = 0;

    gpio_set_direction(BUTTON_ONE, GPIO_MODE_INPUT);
    // gpio_set_direction(LED_ONE, GPIO_MODE_OUTPUT);
    while(true) {
        if(gpio_get_level(BUTTON_ONE) == 1) {
            if(press_count < 40 && led_status == 0) {
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
        vTaskDelay(10);
    }
}

void app_main() {
    spi_init();

    xTaskCreate(acceleration, "acceleration", 2048u, NULL, 10, 0);
    // xTaskCreate(leds_control, "leds_control", 2048u, NULL, 10, 0);
    xTaskCreate(button_one, "button_one", 2048u, NULL, 5, 0);
}
