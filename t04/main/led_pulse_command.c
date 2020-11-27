#include "header.h"



static void wrong_frequence_value() {
    char *msg = "\e[31mwrong frequency value | \e[33m f must be >= 0.0 and <= 2.0\e[0m\n\r";
    uart_write_bytes(UART_PORT, msg, strlen(msg));
}



static void led_pulse_wrong_syntax() {
    char *msg = "\e[31mwrong syntax | \e[33m led pulse [1-3] [f=x.y](frequency; 0 <= x <= 2, 0 <= y <= 9; optional; by default f=1)\e[0m\n\r";
    uart_write_bytes(UART_PORT, msg, strlen(msg));
}



static float freq_determine(char *subcmd) {
    if (subcmd == NULL) 
        return -1;
    char *freq_str = mx_strnew(5);
    int index = 0;

    for (int i = 2; subcmd[i]; ++i) {
        freq_str[index] = subcmd[i];
        index++;
    }
    float freq = atof(freq_str);
    free(freq_str);
    return freq;
}



/*
 * Checks, whether str matches 
 * frequency subcommand pattern.
 * ^f=x.y$ , where 0 <= x <= 2, 0 <= y <= 9
 */
static int freq_match(char *substr) {
    if (substr == NULL)
        return 0;
    regex_t regex;
    int reti = regcomp(&regex, "^f=[0-2].[0-9]$", 0);

    if (reti) 
        return 0;
    reti = regexec(&regex, substr, 0, NULL, 0);
    if (!reti) 
        return 1;
    return  0;
}



/*
 * Creates tasks, which make led(s) pulse.
 * Also sets led(s) frequency.
 */
void led_pulse(char **cmd, int len) {
    int err = 0;
    int led_num;
    float freq = 1;

    if (freq_match(cmd[2]))
        freq = freq_determine(cmd[2]);
    else if (freq_match(cmd[3]))
        freq = freq_determine(cmd[3]);
    else if ((cmd[2] && atoi(cmd[2]) == 0) || (cmd[3] && atoi(cmd[3]) == 0)) {
        led_pulse_wrong_syntax();
        return;
    }

    if (freq < 0.0 || freq > 2.0) {
        wrong_frequence_value();
        return;
    }

    if (len > 4) {
        led_pulse_wrong_syntax();
        return;
    }
    else if ((cmd[2] == NULL || freq_match(cmd[2])) && err == 0) {
        led1_state = LED_IS_PULSING;
        led2_state = LED_IS_PULSING;
        led3_state = LED_IS_PULSING;

        struct led_settings_description *data1 = (struct led_settings_description *)malloc(sizeof(struct led_settings_description));
        data1->led_id = 1;
        data1->freq   = freq;
        xTaskCreate(led_pulsing_task, "led_pulsing_task", 4040, (void *)data1, 10, NULL);

        struct led_settings_description *data2 = (struct led_settings_description *)malloc(sizeof(struct led_settings_description));
        data2->led_id = 2;
        data2->freq   = freq;
        xTaskCreate(led_pulsing_task, "led2_pulsing", 4040, (void *)data2, 10, NULL);   

        struct led_settings_description *data3 = (struct led_settings_description *)malloc(sizeof(struct led_settings_description));
        data3->led_id = 3;
        data3->freq   = freq;
        xTaskCreate(led_pulsing_task, "led3_pulsing", 4040, (void *)data3, 10, NULL); 
        
        vTaskDelay(10);
        free(data1);
        free(data2);
        free(data3);      
    }
    else if (err == 0) {
        struct led_settings_description *data = (struct led_settings_description *)malloc(sizeof(struct led_settings_description));
        led_num = atoi(cmd[2]);
        data->led_id = led_num;
        data->freq = freq;

        if (led_num == 1)
            led1_state = LED_IS_PULSING;
        else if (led_num == 2)
            led2_state = LED_IS_PULSING;
        else if (led_num == 3)
            led3_state = LED_IS_PULSING;

        char task_name[14];
        bzero(task_name, 14);
        sprintf(task_name, "led%d_pulsing", led_num);
        xTaskCreate(led_pulsing_task, task_name, 4040, (void *)data, 10, NULL);
        vTaskDelay(10);
        free(data);
    }
}


