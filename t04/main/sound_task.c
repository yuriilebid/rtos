#include "header.h"

#define EN_AMP 23

void sound_task() {
	esp_err_t err;
    gpio_set_direction(EN_AMP, GPIO_MODE_OUTPUT);
    gpio_set_level(EN_AMP, 1);
    dac_output_enable(DAC_CHANNEL_1);

    static const int i2s_num = 0;
    static const i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN,
        .sample_rate      = 44100,
        .bits_per_sample  = 16,
        .channel_format   = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .intr_alloc_flags = 0,
        .dma_buf_count    = 2,
        .dma_buf_len      = 1024,
        .use_apll         = 1
    };
    i2s_driver_install(i2s_num, &i2s_config, 0, NULL); 
    i2s_set_pin(i2s_num, NULL);
    size_t i2s_bytes_write = 0;

    uint8_t audio_table[]= {0xFF};
    i2s_stop(0);
    while (true) {
        i2s_write(0, audio_table, 1, &i2s_bytes_write, 0);
        vTaskDelay(1000/portTICK_RATE_MS);
    }
}
