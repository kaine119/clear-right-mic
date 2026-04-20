#include "mic.h"
#include "driver/i2s_common.h"
#include "driver/i2s_types.h"
#include "esp_err.h"
#include "hal/i2s_types.h"
#include "soc/gpio_num.h"

#include "esp_log.h"
#include "driver/i2s_std.h"
#include "wav_header.h"

#include <freertos/FreeRTOS.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define TAG "mic_task"

#define BYTE_RATE (SAMPLE_RATE * 3)
#define BUFFER_LENGTH (1 * BYTE_RATE)
#define TOTAL_FILE_LENGTH (RECORDING_DURATION_SEC * BYTE_RATE)

i2s_chan_handle_t rx_handle;
char i2s_raw_buffer[1 * SAMPLE_RATE * 3];

static QueueHandle_t* recording_queue;
int current_recording_idx = 0;
Recording_Item current_recording;

/**
 * Initialize the I2S microphone.
 */
void init_microphone() {
    i2s_chan_config_t chan_config = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_config, NULL, &rx_handle));

    i2s_std_config_t rx_std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(8000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_24BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = GPIO_NUM_3,
            .ws = GPIO_NUM_2,
            .din = GPIO_NUM_10,
            .dout = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false
            }
        }
    };

    rx_std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    rx_std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_384;

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &rx_std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
}

/**
 * Fill a file with .wav data over one recording duration.
 */
void record_sample(int slot_no, Recording_Item* record) {
    sprintf(record->filename, "/spiffs/%d.wav", slot_no);

    const wav_header_t wav_header = WAV_HEADER_PCM_DEFAULT(
        RECORDING_DURATION_SEC * SAMPLE_RATE,
        24,
        SAMPLE_RATE,
        1
    );

    ESP_LOGI(TAG "/record_sample", "Beginning file %d", slot_no);



    FILE* f = fopen(record->filename, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }


    fwrite(&wav_header, sizeof(wav_header_t), 1, f);

    size_t written_bytes = 0;
    size_t bytes_read;

    while (written_bytes < TOTAL_FILE_LENGTH) {
        if (i2s_channel_read(rx_handle, i2s_raw_buffer, SAMPLE_RATE * 3 / 2, &bytes_read, 1000) == ESP_OK) {
            fwrite(i2s_raw_buffer, bytes_read, 1, f);
        } else {
            ESP_LOGE(TAG, "Write failed!");
        }
        written_bytes += bytes_read;
    }

    fclose(f);
    struct stat new_file;
    stat(record->filename, &new_file);
    record->length = new_file.st_size;
    ESP_LOGI(TAG "/record_sample", "Written file %d", slot_no);
}

void mic_task(void* params) {
    
    ESP_LOGI(TAG, "started");
    recording_queue = (QueueHandle_t*) params;


    init_microphone();

    while (1) {
        record_sample(current_recording_idx, &current_recording);

        xQueueSend(*recording_queue, &current_recording, 0);
        current_recording_idx = (current_recording_idx + 1) % NUM_RECORDING_BUFFERS;
        vTaskDelay(60000 / portTICK_PERIOD_MS);
    }
}