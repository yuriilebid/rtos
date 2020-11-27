#include "header.h"

#define TIMER_DIVIDER         80 //  Hardware timer clock divider
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds
#define TIMER_INTERVAL1_SEC   1.0 // how often the interrupt is triggered (1 sec)
#define TEST_WITH_RELOAD      1        
#define DAY_IN_SECONDS        86400

/* @ Digital clock implementation.
 * time_task performs in infinite loop and suspends on xTaskNotifyWait until 
 *  interrupt sends notification.
 * Each second interrupt occures and sends notification.
 * After getting notification from interrupt time_task increments global var.
 *  current_time by 1. Current time represents time in seconds.
 * Also time_task converts current_time in hours:minutes:seconds and prints it
 *  on OLED display.
 */



/*
 * Interrupt, that occurs each seconds and sends
 *  notification to time_task.
 */
static void IRAM_ATTR timer_interrupt(void *para) {
    xTaskNotifyFromISR(xTaskClock, 1, eNoAction, NULL);
    timer_group_clr_intr_status_in_isr(TIMER_GROUP_0, TIMER_0);
    timer_group_enable_alarm_in_isr(TIMER_GROUP_0, TIMER_0);
    portYIELD_FROM_ISR();
    return;
}



/*
 * Initialize selected timer of the timer group 0
 * timer_idx - the timer number to initialize
 * timer_interval_sec - the interval of alarm to set
 */
void timer_initialization(int timer_idx, double timer_interval_sec) {
    timer_config_t config = {
        .divider     = TIMER_DIVIDER,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en  = TIMER_PAUSE,
        .alarm_en    = TIMER_ALARM_EN,
        .auto_reload = true,
    };
    timer_init(TIMER_GROUP_0, timer_idx, &config);

    /* Timer's counter will initially start from value below.
       Also, if auto_reload is set, this value will be automatically reload on alarm */
    timer_set_counter_value(TIMER_GROUP_0, timer_idx, 0x00000000ULL);

    /* Configure the alarm value and the interrupt on alarm. */
    timer_set_alarm_value(TIMER_GROUP_0, timer_idx, timer_interval_sec * TIMER_SCALE);
    timer_enable_intr(TIMER_GROUP_0, timer_idx);
    timer_isr_register(TIMER_GROUP_0, timer_idx, timer_interrupt, NULL, ESP_INTR_FLAG_IRAM, NULL);
    timer_start(TIMER_GROUP_0, timer_idx);
}



void print_current_time_on_display(sh1106_t *display) {
    int hours   = current_time / 3600;
    int minutes = (current_time - (3600 * hours)) / 60;
    int seconds = current_time - (3600 * hours) - (60 * minutes);

    char hours_str[30];
    bzero(hours_str, 30);
    if (hours < 10) sprintf(hours_str, "0%d", hours);
    else            sprintf(hours_str, "%d", hours);
    char minutes_str[30];
    bzero(minutes_str, 30);
    if (minutes < 10) sprintf(minutes_str, "0%d", minutes);
    else              sprintf(minutes_str, "%d", minutes);
    char seconds_str[30];
    bzero(seconds_str, 30);
    if (seconds < 10) sprintf(seconds_str, "0%d", seconds);
    else              sprintf(seconds_str, "%d", seconds);
    char time_buff[100];
    bzero(time_buff, 100);
    sprintf(time_buff, "%s:%s:%s", hours_str, minutes_str, seconds_str);

    sh1106_clear(display);
    screen_print(&display, (char *)time_buff, 3, 15, 2);
}



void timer_task(void *arg) {
    timer_initialization(TIMER_0, TIMER_INTERVAL1_SEC);

    // turning on display.
    sh1106_t display;
    sh1106_init(&display);
    sh1106_clear(&display);

    while (1) {
        xTaskNotifyWait(0x00000000, 0x00000000, NULL, portMAX_DELAY);
        if (current_time == DAY_IN_SECONDS)
            current_time = 0;
        print_current_time_on_display(&display);
        current_time += 1;
    }
}


