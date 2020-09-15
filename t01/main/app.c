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

#define LF_ASCII_CODE 10    // Enter button

#define CMD_MAX_LEN 10      // Max cmd len "led pulse"

void handle_cmd(char *cmd) {
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
    printf("\n\n");
}

void app_main() {
    char cmd[CMD_MAX_LEN];

    while(true) {
        bool end_cmd = false;
        bzero(&cmd, CMD_MAX_LEN);
        /* 
         * (CMD_MAX_LEN - 1) = max len - 1 byte for '\0'
         */
        for(int ind = 0; ind < (CMD_MAX_LEN - 1) && !end_cmd;) {
            if(scanf("%c", &cmd[ind]) != -1) {
                if(cmd[ind] == LF_ASCII_CODE) {
                    end_cmd = true;
                    cmd[ind] = '\0';
                }
                else {
                    printf("%c", cmd[ind]);
                    ind++;
                }
                vTaskDelay(10 / portTICK_PERIOD_MS);
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        handle_cmd(cmd);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
