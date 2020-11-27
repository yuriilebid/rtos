#include "sh1106.h"
#include "font6x8.h"

#define SDA_PIN  21
#define SCL_PIN  22



// Initialize I2C driver.
void init_i2c_driver() {
    i2c_config_t i2c_config = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = SDA_PIN,
        .scl_io_num       = SCL_PIN,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 1000000
    };
    i2c_param_config(SH1106_DEFAULT_PORT,   &i2c_config);
    i2c_driver_install(SH1106_DEFAULT_PORT, I2C_MODE_MASTER, 0, 0, 0);
}



void sh1106_init(sh1106_t *display) {
    // Filling display structure with 
    display->addr = SH1106_DEFAULT_ADDR;
    display->port = SH1106_DEFAULT_PORT;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create(); // command link initialize.
    i2c_master_start(cmd); // start bit.

    /*
     * Бит мода(чтение/запись) является частью адресса.
     * slave adderess - представляет собой 7-битное число.
     * 00111100 - адресс.
     * 00000000 - I2C_MASTER_WRITE
     * (display->addr << 1) => 01111000
     * 01111000 | 00000000 => 0111100(0) - бит для мода (чтение/запись)
     */
    i2c_master_write_byte(cmd, (display->addr << 1) | I2C_MASTER_WRITE, true);

    i2c_master_write_byte(cmd, 0x00, true); // command stream
    i2c_master_write_byte(cmd, 0xAE, true); // turn display off
    i2c_master_write_byte(cmd, 0xD5, true); // clock div
    i2c_master_write_byte(cmd, 0x80, true);
    i2c_master_write_byte(cmd, 0xA8, true); // multiplex
    i2c_master_write_byte(cmd, 0xFF, true);

    i2c_master_write_byte(cmd, 0x20, true);
    i2c_master_write_byte(cmd, 0x00, true);
    
    i2c_master_write_byte(cmd, 0x8D, true); // charge pump
    i2c_master_write_byte(cmd, 0x14, true);
    i2c_master_write_byte(cmd, 0x10, true); // high column
    i2c_master_write_byte(cmd, 0xB0, true);
    i2c_master_write_byte(cmd, 0xC8, true);

    i2c_master_write_byte(cmd, 0x00, true); // low column
    i2c_master_write_byte(cmd, 0x10, true);
    i2c_master_write_byte(cmd, 0x40, true);
    i2c_master_write_byte(cmd, 0xA1, true); // segment remap
    i2c_master_write_byte(cmd, 0xA6, true); // `normal/reverse display (A6 = светится когда 1 и A7 = наоборот )
    i2c_master_write_byte(cmd, 0x81, true); // Настройка контраста.
    i2c_master_write_byte(cmd, 0xFF, true); //  | контраст = от 00 до FF
    i2c_master_write_byte(cmd, 0xAF, true); // on

    i2c_master_stop(cmd); // stop bit.
    i2c_master_cmd_begin(display->port, cmd, 10 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
}



/*
 * Prints character on specified page and position.
 * gage must be >= 0 && <= 7
 * position must be >= 0 && <= 126
 */
void print_char(sh1106_t **display, char c, int page, int position) {
    // args validation.
    if (page < 0 || page > 7 || position < 0 || position > 126) 
        return;

    // index of line, which represent character 'c' in font6x8 array
    int index_in_font = (c - 32) * 6;

    sh1106_t *display1 = *display;
    display1->pages[page][position + 0] = font6x8[index_in_font];
    display1->pages[page][position + 1] = font6x8[index_in_font + 1];
    display1->pages[page][position + 2] = font6x8[index_in_font + 2];
    display1->pages[page][position + 3] = font6x8[index_in_font + 3];
    display1->pages[page][position + 4] = font6x8[index_in_font + 4];
    display1->pages[page][position + 5] = font6x8[index_in_font + 5];
}



// @ Reveres bits in byte.
uint8_t reverse(uint8_t b) {
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}



// @ Prints char. on display with 12x16 font.
void print_char_x2(sh1106_t **display, char c, int page, int position) {
    // index of line, which represent character 'c' in font6x8 array
    int index_in_font = (c - 32) * 6;

    sh1106_t *display1 = *display;

    int pos = position;
    for (int x = 0; x < 6; x++) {

        // duplicate first four bits.
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 2; ++j) {
                if ((font6x8[index_in_font + x] >> i & 1) == 0) {
                     display1->pages[page][pos] <<= 1;
                     display1->pages[page][pos + 1] <<= 1;
                }
                else {
                    display1->pages[page][pos] <<= 1;
                    display1->pages[page][pos] += 1;
                    display1->pages[page][pos + 1] <<= 1;
                    display1->pages[page][pos + 1] += 1;
                }
            }
        }
        display1->pages[page][pos]     = reverse(display1->pages[page][pos]);
        display1->pages[page][pos + 1] = reverse(display1->pages[page][pos + 1]);

        // duplicate last four bits.
        for (int i = 4; i < 8; ++i) {
            for (int j = 0; j < 2; ++j) {
                if ((font6x8[index_in_font + x] >> i & 1) == 0) {
                     display1->pages[page+ 1][pos] <<= 1; 
                     display1->pages[page+ 1][pos + 1] <<= 1; 
                }
                else {
                    display1->pages[page+ 1][pos] <<= 1;
                    display1->pages[page+ 1][pos] += 1;
                    display1->pages[page+ 1][pos + 1] <<= 1;
                    display1->pages[page+ 1][pos + 1] += 1;
                }
            }
        }
        display1->pages[page + 1][pos]     = reverse(display1->pages[page + 1][pos]);
        display1->pages[page + 1][pos + 1] = reverse(display1->pages[page + 1][pos + 1]);
        pos += 2;
    }
}



void screen_print(sh1106_t **display, char *str, int page, int position, int size) {
    if (page < 0 || page > 7)
        return;

    int pos = position;
    if (size == 1) {
        for (int i = 0; str[i]; ++i) {
            print_char(display, str[i], page, pos);
            pos += 6;
            if (pos >= 126)
                break;
        }
    }
    else if (size == 2) {
        for (int i = 0; str[i]; ++i) {
            print_char_x2(display, str[i], page, pos);
            pos += 12;
            if (pos >= 126)
                break;
        }
    }
    sh1106_update(*display);
}



/* @ Updates screen.
 * Needed to be used after each print function.
 */
void sh1106_update(sh1106_t *display) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);

    uint8_t arr[8][128];
    for (int i = 0; i < 8; ++i) //need to change
        for (int j = 0; j < 128; ++j)
            arr[i][j] = display->pages[i][j];

    i2c_master_write_byte(cmd, (display->addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x80, true); // single command
    i2c_master_write_byte(cmd, 0xB0, true);
    i2c_master_write_byte(cmd, 0x40, true); // data stream
    i2c_master_write(cmd, *arr, 128 * 8, true);

    i2c_master_stop(cmd);
    i2c_master_cmd_begin(display->port, cmd, 10 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
}



// Sets all pixels in display with 0.
void sh1106_clear(sh1106_t *display) {
    for (uint8_t i = 0; i < 8; i++) {
        for (uint8_t j = 0; j < 128; j++)
            display->pages[i][j] = 0x00;
    }
}



/*
 * Sets contrast of display.
 * Contrast is represented by uint8_t value = 0x00 - 0xFF
 */
void sh1106_contrast(sh1106_t *display, uint8_t value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create(); // command link initialize.
    i2c_master_start(cmd); // start bit.
    i2c_master_write_byte(cmd, (display->addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x00, true); // command stream
    i2c_master_write_byte(cmd, 0x81, true); // Настройка контраста.
    i2c_master_write_byte(cmd, value, true); //  | контраст = от 00 до FF
    i2c_master_stop(cmd); // stop bit.
    i2c_master_cmd_begin(display->port, cmd, 10 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
}



