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

#define LF_ASCII_CODE 13

#define CMD_MAX_LEN 20
#define ERR_MAX_LEN 100
#define PULSE_MAX_LEN 30

QueueHandle_t queue = NULL;
QueueHandle_t error = NULL;
QueueHandle_t pulse = NULL;


/*
 * @Function : 
 *            pwm_pulsing
 *
 * @Parameters : 
 *              pvParameters        - NULL (needs to be a Task)
 *
 * @Description : 
 *               Sets led's gpio pulsing via PWM configurations.
 *               Controls from <pulse> queue
*/

void pwm_pulsing(void *pvParameters) {
    bool status = false;

    ledc_timer_config_t timer_config;
    timer_config.speed_mode = LEDC_HIGH_SPEED_MODE;
    timer_config.freq_hz = 100;
    timer_config.duty_resolution = LEDC_TIMER_8_BIT;
    timer_config.timer_num = LEDC_TIMER_1;

    ledc_timer_config(&timer_config);

    ledc_channel_config_t channel_config;
    channel_config.gpio_num = GPIO_LED1;
    channel_config.speed_mode = LEDC_HIGH_SPEED_MODE;
    channel_config.channel = LEDC_CHANNEL_1;
    channel_config.intr_type = LEDC_INTR_FADE_END;
    channel_config.timer_sel = LEDC_TIMER_1;
    channel_config.duty = 0;

    ledc_channel_config(&channel_config);
    ledc_fade_func_install(0);

    while(true) {
        char cmd[PULSE_MAX_LEN];

        bzero(&cmd, PULSE_MAX_LEN);
        if(xQueueReceive(pulse, &cmd, PULSE_MAX_LEN) > 0) {
            if(atoi(&cmd[10]) > 3 || atoi(&cmd[10]) < 0) {
                xQueueSendToBack(error, "\e[1m\e[0;31mNo such led.\e[0;39m Available leds: 1, 2, 3", 0);
                false;
            }
            else if(atoi(&cmd[12]) >= 2 || atoi(&cmd[12]) <= 0) {
                xQueueSendToBack(error, "\e[1m\e[0;31mInvalid Hz number.\e[0;39m Available Hz 0 < f < 2", 0);
                status = false;
            }
            else {
                switch(atoi(&cmd[10])) {
                    case 1:
                        channel_config.gpio_num = GPIO_LED1;
                        break;
                    case 2:
                        channel_config.gpio_num = GPIO_LED2;
                        break;
                    case 3:
                        channel_config.gpio_num = GPIO_LED3;
                        break;
                }
                timer_config.freq_hz = atoi(&cmd[12]) * 100;
                ledc_timer_config(&timer_config);
                ledc_channel_config(&channel_config);
                ledc_fade_func_install(0);
                status = true;
            }
        }
        if(status) {
            ledc_set_fade_with_time(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, 200, 1000);
            ledc_fade_start(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, LEDC_FADE_WAIT_DONE);
            ledc_set_fade_with_time(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, 0, 1000);
            ledc_fade_start(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, LEDC_FADE_WAIT_DONE);
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}


/*
 * @Function : 
 *             handle_cmd
 *
 * @Parameters : 
 *               pvParameters        - NULL (needs to be a vTask)
 *
 * @Description : 
 *                Manage witch function to call according to
 *                console input.
*/

void handle_cmd(void *pvParameters) {
    while(true) {
        char cmd[CMD_MAX_LEN];
        bzero(&cmd, CMD_MAX_LEN);
        xQueueReceive(queue, &cmd, CMD_MAX_LEN);
        if(strstr(cmd, "led on") == cmd) {
            if(strstr(cmd, " 1") != NULL) {
                gpio_set_direction(GPIO_LED1, GPIO_MODE_OUTPUT);
                gpio_set_level(GPIO_LED1, 1);
            }
            else if(strstr(cmd, " 2") != NULL) {
                gpio_set_direction(GPIO_LED2, GPIO_MODE_OUTPUT);
                gpio_set_level(GPIO_LED2, 1);
            }
            else if(strstr(cmd, " 3") != NULL) {
                gpio_set_direction(GPIO_LED3, GPIO_MODE_OUTPUT);
                gpio_set_level(GPIO_LED3, 1);
            }
            else {
                xQueueSendToBack(error, "\e[1m\e[0;31mNo such led.\e[0;39m Available leds: 1, 2, 3", 0);
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        else if(strstr(cmd, "led off") == cmd) {
            if(strstr(cmd, " 1") != NULL) {
                gpio_set_direction(GPIO_LED1, GPIO_MODE_DISABLE);
            }
            else if(strstr(cmd, " 2") != NULL) {
                gpio_set_direction(GPIO_LED2, GPIO_MODE_DISABLE);
            }
            else if(strstr(cmd, " 3") != NULL) {
                gpio_set_direction(GPIO_LED3, GPIO_MODE_DISABLE);
            }
            else {
                xQueueSendToBack(error, "\e[1m\e[0;31mNo such led.\e[0;39m Available leds: 1, 2, 3", 0);
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        else if(strstr(cmd, "led pulse") == cmd) {
            xQueueSendToBack(pulse, &cmd, 0);
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    }
    printf("\n\n");
}


/*
 * @Function : 
 *             input_getter
 *
 * @Parameters : 
 *               pvParameters        - NULL (needs to be a vTask)
 *
 * @Description : Gets input from UART console with managin function buttons.
 *                Sends input to queue "queue".
*/

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
            uart_write_bytes(UART_NUM_2, (char*)err, strlen((char*)err));

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
    pulse = xQueueCreate(1, PULSE_MAX_LEN);
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
    
    xTaskCreate(input_getter, "input_getter", 4048, NULL, 2, NULL);
    xTaskCreate(handle_cmd, "handle_cmd", 4048, NULL, 1, NULL);
    xTaskCreate(pwm_pulsing, "pwm_pulsing", 4048, NULL, 3, NULL);
}
