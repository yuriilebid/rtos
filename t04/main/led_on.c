#include "header.h"

/* 
 * @ turns on specified led(s)
 */



static void led_is_pulsing_error() {
	char *msg = "\e[31myou can not turn on led(s), while they are pulsing. turn off led(s) [1-3]\e[0m\n\r";
    uart_write_bytes(UART_PORT, msg, strlen(msg));
}



static void invalid_led_number() {
	char *msg = "\e[31minvalid led number| led on/off [1-3]\e[0m\n\r";
    uart_write_bytes(UART_PORT, msg, strlen(msg));
}


static void led_on_syntax_error() {
	char *msg = "\e[31minvalid syntax| led on [1-3]\e[0m\n\r";
    uart_write_bytes(UART_PORT, msg, strlen(msg));
}



static void turn_on_all_leds() {
	if (led1_state == LED_IS_PULSING 
		|| led2_state  == LED_IS_PULSING 
		|| led3_state == LED_IS_PULSING)
        led_is_pulsing_error();
    else
        all_led_set(1);
}



static void turn_on_specific_led(char **cmd) {
	int led_num = atoi(cmd[2]);

    if ((led_num == 1 && led1_state == LED_IS_PULSING) 
        || (led_num == 2 && led2_state == LED_IS_PULSING) 
        || (led_num == 3 && led3_state == LED_IS_PULSING))
        led_is_pulsing_error();
    else if (led_num <= 0 || led_num > 3)
        invalid_led_number();
    else
        led_set_by_id(led_num, 1);
}



void led_on(char **cmd, int len) {
    if (len == 2)
        turn_on_all_leds();
    else if (len == 3)
        turn_on_specific_led(cmd);
    else
        led_on_syntax_error();
}
