#ifndef _SH1106_H_
#define _SH1106_H_
#include "driver/i2c.h"
#include <unistd.h>
#include <stdlib.h>

/*
 * SH1106 OLED I2C display driver for ESP32 ESP-IDF.
 */
#define SH1106_DEFAULT_ADDR 0x3C      // default I2C address
#define SH1106_DEFAULT_PORT I2C_NUM_0 // default I2C interface port

/*
 * Main SH1106 display struct type. Stores all state for the display including
 * page buffers.
 */
typedef struct {
    uint8_t  addr;           // I2C address
    i2c_port_t port;          // I2C interface port
    uint16_t changes;        // page change bit to optimize writes
    uint8_t pages[8][128];   // pixel grid (8 * 8bit) * 128
} sh1106_t;

/*
 *   Initialize SH1106 display. This method requires the ".addr" and ".port"
 *   properties to be set on the sh1106_t struct before calling.
 */
void sh1106_init(sh1106_t *display);


/*
 * Update the display by writing all pages that have changed since the last
 * update.
 */
void sh1106_update(sh1106_t *display);

// Clears display.
void sh1106_clear(sh1106_t *display);

void print_char(sh1106_t **display, char c, int page, int position);
void init_display(sh1106_t *display);
void init_i2c_driver();
void sh1106_contrast(sh1106_t *display, uint8_t value);
void screen_print(sh1106_t **display, char *str, int page, int position, int size);
#endif