#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include <string.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "wrappers.h"
#include "driver/dac.h"
#include "driver/ledc.h"
#include <regex.h> 
#include "get_dht11_data.h"
#include "libmx.h"
#include "esp_types.h"
#include "freertos/queue.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"
#include "sh1106.h"
#include "driver/i2c.h"
#include <unistd.h>
#include "esp_err.h"
#include "driver/i2s.h"
#include <strings.h>
#include <ctype.h>



/* Config */
#define COMMAND_LINE_MAX_LENGTH 1024
#define UART_PORT 				UART_NUM_1
#define NEWLINE 				"\n\r"
#define OLED_GPIO				32				

/* Errors */
#define WRONG_SYNTAX_LED_ON_OFF 10
#define INVALID_ARGUMENT        11
#define LED_BUSY                12
#define NO_SUCH_COMMAND         13
#define WRONG_SYNTAX_PULSE      14
#define WRONG_FREQUENCY_VALUE   15

/* LED PINS */
#define LED1 27
#define LED2 26
#define LED3 33

/* LED states */
#define LED_IS_OFF				20
#define LED_IS_ON 				21
#define LED_IS_PULSING 			22

struct led_settings_description {
	int   led_id;
	float freq;
};

typedef struct {
    int type;  // the type of timer's event
} timer_event_t;


int led1_state;
int led2_state;
int led3_state;
int current_time;

xQueueHandle  global_input_queue;
QueueHandle_t uart0_queue;
QueueHandle_t dht11_data_queue;
TaskHandle_t  xTaskWeather;
TaskHandle_t  xTaskClock;


void user_input();
void cmd_handler();
void execute(char **cmd, int len);
void uart_init(int baud_rate);

/* dht11 fucntions */
void dht11_monitor();
void tehu(char **cmd);

/* help */
void help_command();

/* time */
void time_command(char **cmd);
void timer_task(void *arg);

/* sound */
void sound_task();

/* led functions */
void led_on(char **cmd, int len);
void led_off(char **cmd, int len);
void led_pulse(char **cmd, int len);
void led_pulsing_task(void *settings);
