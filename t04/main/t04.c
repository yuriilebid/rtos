#include "header.h"

/* @ This is CLI for board ESP32, which supports commands for:
 *  1. led manipulation.
 *  2. getting data from dht11 sensor.
 *  3. tune digital clock, which is displayed on oled dispaly. 
 *  4. making noise a.k.a sound.
 ***Communication through UART2.
 *
 * Support commands: 
 *  >led on [1-3] 
 *  >led off [1-3]
 *  >led pulse [1-3]
 *  *if led num. is not specified, all leds are pulse/on/off.
 *  >tehu [-f] - prints log with temperature and humidity.
 *  >time [set/reset] [00:00:00]
 *  >sound [on/off]
 */


void inline global_variables_init() {
    led1_state = LED_IS_OFF;
    led2_state = LED_IS_OFF;
    led3_state = LED_IS_OFF;

    global_input_queue = xQueueCreate(5, COMMAND_LINE_MAX_LENGTH);
    current_time = 0;
}



void app_main() {
    // turn on oled display.
    gpio_set_direction(OLED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(OLED_GPIO, 1);

    global_variables_init();
    uart_init(9600);
    init_i2c_driver();

    /*
     * Four tasks are running constantly.
     * user_input - gets user`s input from UART2 and sends it  to cmd_handler
     *  task through queue global_input_queue. If no data in UART2, task sleeps.
     * cmd_handler - receives user`s input as string from user_input task
     *  through queue. Splits received string in array and determines which
     *  command to execute.
     * dht11_monitor - monitors dht11 temperature/humidity sensor every 5 seconds and
     *  sends got data in Queue(dht11_data_queue)
     * timer_task - digital clock implementation. Every second increment variable, which
     *  represents time.
     * sound_task - makes sound, when user write `sound on` command. `sound off` disables sound.
     * PS. user_input and cmd_handler are declared in input.c
     *     timer_task is declared in digital_clock.c
     */
    xTaskCreate(user_input,    "user_input",    12040, NULL, 10, NULL);
    xTaskCreate(cmd_handler,   "cmd_handler",   12040, NULL, 10, NULL);
    xTaskCreate(sound_task,    "sound",         12040, NULL, 10, NULL);
    xTaskCreate(dht11_monitor, "dht11_monitor", 12040, NULL, 10,  &xTaskWeather);
    xTaskCreate(timer_task,    "timer",         12040, NULL, 10,  &xTaskClock);
}
