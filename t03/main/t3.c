#include "t03.h"

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
    i2c_master_write_byte(cmd, 0x81, true); // contrast
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
*/

void draw_image() {
    char buff[20];

    bzero(&buff, 20);
    set_time();
    clear_display();
    sprintf(&buff, "  %d:%d:%d", HOURS, MINUTES, seconds);
    print_string(buff , 2, 3);
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
 *            wait_status
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

void app_main() {
    queue = xQueueCreate(1, CMD_MAX_LEN);
    error = xQueueCreate(1, ERR_MAX_LEN);

    uart_init();
    gpio_set_direction(GPIO_NUM_32, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_32, 1);
    init_i2c();
    display.addr = SH1106_ADDR;
    display.port = SH1106_PORT;
    init_sh1106();
    
    xTaskCreate(time_output, "time_output", 12040u, NULL, 3, &xTaskTimeOutput);
    xTaskCreate(input_getter, "input_getter", 4048u, NULL, 1, NULL);
    xTaskCreate(handle_cmd, "handle_cmd", 4048u, NULL, 1, NULL);
}
