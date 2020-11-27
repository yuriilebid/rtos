#include "header.h"

/*
 * Turns ledX off if it`s on or if it`s pulsing.
 * if len == 2 => "led off" => turns all leds off.
 * if len == 3 => "led on 1-3" => turns 1-3 led off.
 * if len <= 0 or >3 => error.
 */



static void turn_off_all_leds() {
    all_led_set(0);
    led1_state = LED_IS_OFF;
    led2_state = LED_IS_OFF;
    led3_state = LED_IS_OFF;
}



static void invalid_led_number() {
    char *msg = "\e[31minvalid led number| led on/off [1-3]\e[0m\n\r";
    uart_write_bytes(UART_PORT, msg, strlen(msg));
}



static void led_off_syntax_error() {
    char *msg = "\e[31minvalid syntax| led off [1-3]\e[0m\n\r";
    uart_write_bytes(UART_PORT, msg, strlen(msg));
}



static void turn_off_specific_led(char **cmd) {
    int led_num = atoi(cmd[2]);

    if (led_num == 0 || led_num > 3)
        invalid_led_number();
    else {
        led_set_by_id(led_num, 0);
        if (led_num == 1) led1_state = LED_IS_OFF;
        if (led_num == 2) led2_state = LED_IS_OFF;
        if (led_num == 3) led3_state = LED_IS_OFF;
    }
}



void led_off(char **cmd, int len) {
    if (len == 2)
        turn_off_all_leds();
    else if (len == 3)
        turn_off_specific_led(cmd);
    else
        led_off_syntax_error();
}
