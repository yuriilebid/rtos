#include "wrappers.h"



void gpio_set_direction_wrapper(int gpio, int mode) {
	if (gpio_set_direction(gpio, mode) != ESP_OK) {
		ESP_LOGI("gpio_set_direction: ", "%s", "some error occured.");
		exit(1);
	}
}



void gpio_set_level_wrapper(int gpio, int level) {
	if (gpio_set_level(gpio, level) != ESP_OK) {
		ESP_LOGI("gpio_set_level ", "%s", "some error occured.");
		exit(1);
	}
}



void gpio_set(int gpio, int mode, int level) {
	gpio_set_direction_wrapper(gpio, mode);
	gpio_set_level_wrapper(gpio, level);
}



void dac_output_enable_wrapper(int dac_channel) {
	if (dac_output_enable(dac_channel) != ESP_OK) {
		ESP_LOGI("dac_output_enable ", "%s", "some error occured.");
		exit(1);
	}
} 



void led_mode(int gpio_led, int set) {
    gpio_set(gpio_led, GPIO_MODE_OUTPUT, set);
}



void all_led_set(int mode) {
    led_mode(GPIO_LED1, mode);
    led_mode(GPIO_LED2, mode);
    led_mode(GPIO_LED3, mode);
}



void led_set_by_id(int led_id, int mode) {
    if (led_id == 1)
        led_mode(GPIO_LED1, mode);
    if (led_id == 2)
        led_mode(GPIO_LED2, mode);
    if (led_id == 3)
        led_mode(GPIO_LED3, mode);
}
