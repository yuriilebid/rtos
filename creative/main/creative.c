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
#include "driver/i2s.h"
#include "driver/spi_master.h"

#define DHT11_POWER 2
#define DHT11_DATA  4

#define EN_AMP 23

#define GPIO_LED1 26
#define GPIO_LED2 27
#define GPIO_LED3 33

#define TXPIN 16
#define RXPIN 17

#define LF_ASCII_CODE 13        // Enter button

#define CMD_MAX_LEN 30
#define ERR_MAX_LEN 40
#define HISTORY_SIZE 60
#define PULSE_MAX_LEN 30

#define DHT11_POWER 2
#define DHT11_DATA  4

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

uint8_t HOURS = 0;
uint8_t MINUTES = 0;
uint8_t seconds = 0;

uint8_t TEMPERATURELVL = 0;
uint8_t HUMIDITYLVL = 0;

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
QueueHandle_t pulse = NULL;

int time_secs_general = 0;
int time_secs_current = 0;
int timer_index = TIMER_1;
int timer_index_alarm = TIMER_0;

bool command_line_status = false;

sh1106_t display;

uint32_t mins = 0x00000000;
uint32_t hours = 0x00000000;
uint32_t ulNotifiedValueSecs;

bool write = false;
bool sound_status = false;

TaskHandle_t xTaskTimeOutput;
TaskHandle_t xAlarmTimeOutput;
TaskHandle_t xTaskOledOutput;


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
    i2c_master_write_byte(cmd, 0x01, true); // contrast
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


/*
 * @Function : 
 *            clock_isr
 *
 * @Parameters : 
 *              para        - NULL (needs to be an interrupt function)
 *
 * @Description : 
 *               Notifies about timers counts to 1 seconds to main task. Set timers to default value
 *               and starts it again
*/

static void IRAM_ATTR clock_isr(void *para) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint32_t timer_intr = timer_group_get_intr_status_in_isr(TIMER_GROUP_0);
    uint64_t timer_counter_value = timer_group_get_counter_value_in_isr(TIMER_GROUP_0, timer_index);

    xTaskNotifyFromISR(xTaskTimeOutput, 1, eNoAction, NULL);

    timer_group_clr_intr_status_in_isr(TIMER_GROUP_0, timer_index);
    timer_group_enable_alarm_in_isr(TIMER_GROUP_0, timer_index);
    portYIELD_FROM_ISR();
}


/*
 * @Function : 
 *            clock_alarm
 *
 * @Parameters : 
 *              para        - NULL (needs to be an interrupt function)
 *
 * @Description : 
 *               Notifies about timers counts to 1 seconds to main task. Set timers to default value
 *               and starts it again
*/

static void IRAM_ATTR clock_alarm(void *para) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint32_t timer_intr = timer_group_get_intr_status_in_isr(TIMER_GROUP_0);
    uint64_t timer_counter_value = timer_group_get_counter_value_in_isr(TIMER_GROUP_0, timer_index_alarm);

    xTaskNotifyFromISR(xAlarmTimeOutput, 1, eNoAction, NULL);

    timer_group_clr_intr_status_in_isr(TIMER_GROUP_0, timer_index_alarm);
    timer_group_enable_alarm_in_isr(TIMER_GROUP_0, timer_index_alarm);
    portYIELD_FROM_ISR();
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
}


/*
 * @Function : 
 *            set_time
 *
 * @Description : 
 *               Change global variables <seconds> <minutes> <hours>
 *               according to change of seconds
*/

void set_time() {
    if(seconds >= 60) {
        MINUTES++;
        seconds = 0;
    }
    if(MINUTES >= 60) {
        HOURS++;
        MINUTES = 0;
    }
    if(HOURS >= 24) {
        HOURS = 0;
    }
}


/*
 * @Function : 
 *             wait_status
 *
 * @Parameters :
 *               status        - true (high voltage)
 *                             - false (low voltage)
 *
 * @Description : 
 *                Waits for voltage level to change to <status>
 *
 * @Return :
 *           count        - amount of microseconds before changing
 *                          voltage
*/

int wait_status(_Bool status) {
    int count = 0;

    while (gpio_get_level(DHT11_DATA) == status) {
        count++;
        ets_delay_us(1);
    }
    return count;
}


/*
 * @Function : 
 *             preparing_for_receiving_data
 *
 * @Description : 
 *                Waits 1.5 seconds to measure first temp 
 *                and humidity values
*/

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


/*
 * @Function : 
 *             weather
 *
 * @Description : 
 *                Gets temperature and humidity measurements
 *                from dht11
*/

void weather(void *pvParameters) {
    int res = 0;
    uint8_t data[5];

    while(true) {
        bzero(&data, sizeof(data));
        preparing_for_receiving_data();
        
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
        if (data[0] + data[1] + data[2] + data[3] != data[4]) {
            printf("Invalid sum\n");
        }
        TEMPERATURELVL = data[2];
        HUMIDITYLVL = data[0];
        vTaskDelay(10);
    }
}


/*
 * @Function : 
 *             command_line_arrow
 *
 * @Description : 
 *                Set color of command line arrow ">"
 *                - green if no errors
 *                - red if errors ocurred
 *                and writes it to UART console.
*/

void command_line_arrow() {
    uint8_t err[CMD_MAX_LEN];
    char* text_pick = "> ";
    char cmd[CMD_MAX_LEN];
    bzero(&cmd, CMD_MAX_LEN);
    bzero(&err, CMD_MAX_LEN);

    if(xQueueReceive(error, &err, 5) != errQUEUE_EMPTY) {
        text_pick = "\e[0;31m> \e[0;39m";
    }
    else {
        text_pick = "\e[0;32m> \e[0;39m";
    }
    uart_write_bytes(UART_NUM_2, "\n\r", 2);
    uart_write_bytes(UART_NUM_2, text_pick, strlen(text_pick));
}


/*
 * @Function : 
 *            draw_image
 *
 * @Description : 
 *               Sets new time string on display(dht1106)
 *               Adds '0' where amount of digits are less than max number 
 *               of digits (6:5:30 -> 06:05:30)
*/

void draw_image() {
    set_time();
    clear_display();
    char time[20];
    char humidity_temperature[20];

    bzero(&time, 20);
    if(HOURS < 10 && MINUTES < 10 && seconds < 10) {
        sprintf(&time, " %c%d:%c%d:%c%d", '0', HOURS, '0', MINUTES, '0', seconds);
    }
    else if(HOURS < 10 && MINUTES < 10) {
        sprintf(&time, " %c%d:%c%d:%d", '0', HOURS, '0', MINUTES, seconds);
    }
    else if(HOURS < 10) {
        sprintf(&time, " %c%d:%d:%d", '0', HOURS, MINUTES, seconds);   
    }
    else if(MINUTES < 10 && seconds < 10) {
        sprintf(&time, " %d:%c%d:%c%d", HOURS, '0', MINUTES, '0', seconds);
    }
    else if(MINUTES < 10) {
        sprintf(&time, " %d:%c%d:%d", HOURS, '0', MINUTES, seconds);
    }
    else if(seconds < 10) {
        sprintf(&time, " %d:%d:%c%d", HOURS, MINUTES, '0', seconds);
    }
    else {
        sprintf(&time, " %d:%d:%d", HOURS, MINUTES, seconds);
    }
    print_string(time , 2, 5);

    bzero(&humidity_temperature, 10);
    sprintf(&humidity_temperature, "%d%%    %dC", HUMIDITYLVL, TEMPERATURELVL);
    print_string(humidity_temperature , 2, 2);
    print_string("humidity  temperature" , 1, 1);

    refresh_sh1106();
}


/*
 * @Function : 
 *            timer_initialisation
 *
 * @Description : 
 *               Configures time to count up for 1 seconds, and use function <clock_isr>
 *               as an interrupt to notify changes of timer
*/

void timer_initialisation() {
    timer_config_t clock_cfg = {
        .divider = TIMER_DIVIDER,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = TEST_WITH_RELOAD,
    };
    timer_init(TIMER_GROUP_0, timer_index, &clock_cfg);
    timer_set_counter_value(TIMER_GROUP_0, timer_index, 0);
    timer_set_alarm_value(TIMER_GROUP_0, timer_index, (1) * TIMER_SCALE);
    timer_enable_intr(TIMER_GROUP_0, timer_index);
    timer_isr_register(TIMER_GROUP_0, timer_index, clock_isr, NULL, ESP_INTR_FLAG_IRAM, NULL);
    timer_start(TIMER_GROUP_0, timer_index);
}


/*
 * @Function : 
 *            time_output
 *
 * @Parameters : 
 *              param        - NULL (needs to be a vTask)
 *
 * @Description : 
 *               Call display updates each interrupt(1 second)
*/

void time_output(void *param) {
    timer_initialisation();

    while(true) {
        xTaskNotifyWait(0x00000000, 0x00000000, NULL, portMAX_DELAY);
        seconds++;
        draw_image();
    }
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

    while(true) {
        char *request = NULL;
        bool end_cmd = false;

        bzero(buff, CMD_MAX_LEN);
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
        command_line_status = true;
        xQueueSendToBack(queue, &buff, 0);
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}


/*
 * @Function : 
 *             get_users_time
 *
 * @Parameters : 
 *               cmd        - users input from console
 *
 * @Description : 
 *                change time variables according to input from console
*/

void get_users_time(char *cmd) {
    char time[3];
    int ind = 9;
    int start_ind = 0;

    bzero(&time, 3);
    for(; cmd[ind] > 47 && cmd[ind] < 58; ind++) {
        time[ind - 9] = cmd[ind];
    }
    printf("time = %s\n", time);
    HOURS = atoi(time);
    if(strlen(cmd) > ind) {
        bzero(&time, 3);
        start_ind = ind + 1;
        for(ind += 1; cmd[ind] > 47 && cmd[ind] < 58; ind++) {
            time[ind - start_ind] = cmd[ind];
        }
        printf("%s\n", time);
        MINUTES = atoi(time);
    }
    if(strlen(cmd) > ind) {
        bzero(&time, 3);
        start_ind = ind + 1;
        for(ind += 1; cmd[ind] > 47 && cmd[ind] < 58; ind++) {
            time[ind - start_ind] = cmd[ind];
        }
        printf("%s\n", time);
        seconds = atoi(time);
    }
    printf("%d %d %d\n", HOURS, MINUTES, seconds);
}


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
                false;
            }
            else if(atoi(&cmd[12]) >= 2 || atoi(&cmd[12]) <= 0) {
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
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}


/*
 * @Function : 
 *            time_output
 *
 * @Parameters : 
 *              param        - NULL (needs to be a vTask)
 *
 * @Description : 
 *               Call display updates each interrupt(1 second)
*/

void alarm_ding(void *param) {
    timer_initialisation();

    while(true) {
        xTaskNotifyWait(0x00000000, 0x00000000, NULL, portMAX_DELAY);
        sound_status = true;
        timer_pause(TIMER_GROUP_0, timer_index_alarm);
        vTaskDelay(2000);
        sound_status = false;
        vTaskDelay(20);
    }
}


/*
 * @Function : 
 *            set_timer
 *
 * @Parameters :
 *              secs        - amount of seconds timer should count for
 *
 * @Description : 
 *               Configures time to count up for <secs> seconds, and use function <clock_isr>
 *               as an interrupt to notify changes of timer
*/

void set_timer(int secs) {
    timer_config_t clock_cfg = {
        .divider = TIMER_DIVIDER,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = TEST_WITH_RELOAD,
    };
    timer_init(TIMER_GROUP_0, timer_index_alarm, &clock_cfg);
    timer_set_counter_value(TIMER_GROUP_0, timer_index_alarm, 0);
    timer_set_alarm_value(TIMER_GROUP_0, timer_index_alarm, (secs) * TIMER_SCALE);
    timer_enable_intr(TIMER_GROUP_0, timer_index_alarm);
    timer_isr_register(TIMER_GROUP_0, timer_index_alarm, clock_alarm, NULL, ESP_INTR_FLAG_IRAM, NULL);
    timer_start(TIMER_GROUP_0, timer_index_alarm);
}


/*
 * @Function : 
 *             count_secs_to_alarm
 *
 * @Parameters :
 *               cmd        - command line string from UART2 (console)
 *
 * @Description : 
 *                calculate amount of seconds to set timer. Used for setting alarm
 *
 * @Return :
 *           time_to_alarm_secs        - amount of seconds for seting timer
*/

int count_secs_to_alarm(char* cmd) {
    int time_to_alarm_secs = 0;
    int current_time_secs = HOURS * 3600 + MINUTES * 60 + seconds;
    char time[3];
    int ind = 10;
    int start_ind = 0;

    bzero(&time, 3);
    for(; cmd[ind] > 47 && cmd[ind] < 58; ind++) {
        time[ind - 10] = cmd[ind];
    }
    time_to_alarm_secs += 3600 * atoi(time);   
    if(strlen(cmd) > ind) {
        bzero(&time, 3);
        start_ind = ind + 1;
        for(ind += 1; cmd[ind] > 47 && cmd[ind] < 58; ind++) {
            time[ind - start_ind] = cmd[ind];
        }
        time_to_alarm_secs += 60 * atoi(time);   
    }
    if(strlen(cmd) > ind) {
        bzero(&time, 3);
        start_ind = ind + 1;
        for(ind += 1; cmd[ind] > 47 && cmd[ind] < 58; ind++) {
            time[ind - start_ind] = cmd[ind];
        }
        time_to_alarm_secs += atoi(time);   
    }
    if(time_to_alarm_secs < current_time_secs) {
        printf("curren : %d\n", current_time_secs);
        time_to_alarm_secs += ((3600 * 24) - current_time_secs);
    }
    else {
        printf("curren : %d\n", current_time_secs);
        time_to_alarm_secs = (time_to_alarm_secs - current_time_secs);
    }
    return time_to_alarm_secs - 1;
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

        if(strstr(cmd, "set time") == cmd)  {
            get_users_time(cmd);
            command_line_arrow();
        }
        else if(strstr(cmd, "led on") == cmd) {
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
        }
        else if(strstr(cmd, "get temperature") == cmd) {
            char buff[30];

            bzero(buff, 30);
            sprintf(&buff, "\n\rTemperature: %d", TEMPERATURELVL);
            uart_write_bytes(UART_NUM_2, buff, strlen(buff));
            command_line_arrow();
        }
        else if(strstr(cmd, "get humidity") == cmd) {
            char buff[30];

            bzero(buff, 30);
            sprintf(&buff, "\n\rHumidity: %d", HUMIDITYLVL);
            uart_write_bytes(UART_NUM_2, buff, strlen(buff));
            command_line_arrow();
        }
        else if(strstr(cmd, "sound on") == cmd) {
            sound_status = true;
            command_line_arrow();
        }
        else if(strstr(cmd, "sound off") == cmd) {
            sound_status = false;
            command_line_arrow();
        }
        else if(strstr(cmd, "set alarm") == cmd) {
            int time_to_alaram = count_secs_to_alarm(cmd);
            printf("%d\n", time_to_alaram);
            set_timer(time_to_alaram);
            command_line_arrow();
        }
        else if(command_line_status == true) {
            command_line_arrow();
        }
        command_line_status = false;
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}


/*
 * @Function : 
 *             uart_init
 *
 * @Description : 
 *                Configure UART interface. UART used for console input
*/

void uart_init() {
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
}


/*
 * @Function : 
 *             sound_task
 *
 * @Parameters : 
 *               pvParameters        - NULL (needs to be a vTask)
 *
 * @Description : 
 *                Play sound if through i2s if <sound_status> is True
*/

void sound_task(void *pvParameters) {
    gpio_set_direction(EN_AMP, GPIO_MODE_OUTPUT);
    gpio_set_level(EN_AMP, 1);
    dac_output_enable(DAC_CHANNEL_1);

    static const int i2s_num = 0;
    static const i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN,
        .sample_rate      = 44100,
        .bits_per_sample  = 16,
        .channel_format   = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .intr_alloc_flags = 0,
        .dma_buf_count    = 2,
        .dma_buf_len      = 1024,
        .use_apll         = 1
    };
    i2s_driver_install(i2s_num, &i2s_config, 0, NULL); 
    i2s_set_pin(i2s_num, NULL);
    size_t i2s_bytes_write = 0;
    uint8_t audio_table[]= {0xFF};
    i2s_stop(0);

    while(true) {
        if(sound_status) {
            i2s_start(0);
        }
        else {
            i2s_stop(0);
        }
        i2s_write(i2s_num, audio_table, 1, &i2s_bytes_write, 0);
        vTaskDelay(10);
    }
}

void app_main() {
    queue = xQueueCreate(1, CMD_MAX_LEN);
    error = xQueueCreate(1, ERR_MAX_LEN);
    pulse = xQueueCreate(1, PULSE_MAX_LEN);

    gpio_set_direction(DHT11_POWER, GPIO_MODE_OUTPUT);
    gpio_set_level(DHT11_POWER, 1);
    uart_init();
    gpio_set_direction(GPIO_NUM_32, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_32, 1);
    init_i2c();
    display.addr = SH1106_ADDR;
    display.port = SH1106_PORT;
    init_sh1106();
    
    xTaskCreate(weather, "weather", 2048u, NULL, 1, 0);
    xTaskCreate(time_output, "time_output", 12040u, NULL, 3, &xTaskTimeOutput);
    xTaskCreate(input_getter, "input_getter", 4048u, NULL, 1, NULL);
    xTaskCreate(handle_cmd, "handle_cmd", 4048u, NULL, 1, NULL);
    xTaskCreate(pwm_pulsing, "pwm_pulsing", 4048u, NULL, 3, NULL);
    xTaskCreate(sound_task, "sound_task", 12040u, NULL, 2, NULL);
    xTaskCreate(alarm_ding, "alarm_ding", 4048u, NULL, 3, &xAlarmTimeOutput);
}
