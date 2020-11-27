#include "header.h"



static int led_gpio_determine(int led_id) {
    int res = LED1;

    if (led_id == 1)
        res = LED1;
    else if (led_id == 2)
        res = LED2;
    else if (led_id == 3)
        res = LED3;
    return res;
}



static int led_timer_determiner(int led_id) {
    int timer = LEDC_TIMER_1;
     if (led_id == 1)
        timer = LEDC_TIMER_1;
    else if (led_id == 2)
        timer = LEDC_TIMER_2;
    else if (led_id == 3)
        timer = LEDC_TIMER_3;
    return timer;
}



static int led_channel_determiner(int led_id) {
    int channel = LEDC_CHANNEL_1;
     if (led_id == 1)
        channel = LEDC_CHANNEL_1;
    else if (led_id == 2)
        channel = LEDC_CHANNEL_2;
    else if (led_id == 3)
        channel = LEDC_CHANNEL_3;
    return channel;
}



void led_pulsing_task(void *settings) {
    struct led_settings_description *data = (struct led_settings_description *)settings;
    int led_id = data->led_id;
    float freq = data->freq;

    // timer configuration.
    ledc_timer_config_t ledc_timer;
    ledc_timer.speed_mode      = LEDC_HIGH_SPEED_MODE;
    ledc_timer.freq_hz         = freq * 100;
    ledc_timer.duty_resolution = LEDC_TIMER_8_BIT; // 256
    ledc_timer.timer_num       = led_timer_determiner(led_id);
    if(ledc_timer_config(&ledc_timer) != ESP_OK) 
        ESP_LOGI("ledc_timer_config ", "%s", "some error occured");

    // chanel configuration.
    ledc_channel_config_t ledc_channel;
    ledc_channel.gpio_num   = led_gpio_determine(led_id);
    ledc_channel.speed_mode = LEDC_HIGH_SPEED_MODE;
    ledc_channel.channel    = led_channel_determiner(led_id);
    ledc_channel.intr_type  = LEDC_INTR_FADE_END;
    ledc_channel.timer_sel  = led_timer_determiner(led_id);
    ledc_channel.duty       = 0;

    if (ledc_channel_config(&ledc_channel) != ESP_OK) 
        ESP_LOGI("ledc_channel_config ", "%s", "some error occured");
    if (ledc_fade_func_install(0) != ESP_OK) 
        ESP_LOGI("ledc_fade_func_install ", "%s", "some error occured");

     while(1) {
        if (led_id == 1 && led1_state == LED_IS_OFF)
            vTaskDelete(NULL);
        else if (led_id == 2 && led2_state == LED_IS_OFF)
            vTaskDelete(NULL);
        else if (led_id == 3 && led3_state == LED_IS_OFF)
            vTaskDelete(NULL);

        // ascending.
        ledc_set_fade_with_time(ledc_channel.speed_mode, ledc_channel.channel, 255, 1000);
        ledc_fade_start(ledc_channel.speed_mode, ledc_channel.channel, LEDC_FADE_WAIT_DONE);
        // descending.
        ledc_set_fade_with_time(ledc_channel.speed_mode, ledc_channel.channel, 0, 1000);
        ledc_fade_start(ledc_channel.speed_mode, ledc_channel.channel, LEDC_FADE_WAIT_DONE);
        vTaskDelay(1);
    }
}
