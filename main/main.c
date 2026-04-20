/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "main.h"

#include "inference_api.h"
#include "mic.h"
#include "status_updater.h"

#include "app_reset.h"
#include "esp_chip_info.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include <stdio.h>

// Full reset button config
#define RESET_BUTTON_GPIO          0
#define RESET_BUTTON_ACTIVE_LEVEL  0

#define TAG "main"

/**
 * Print information about the ESP32 device in use, including supported RF features.
 */
void print_chip_info() {
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), %s%s%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
           (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    printf("silicon revision v%d.%d, ", major_rev, minor_rev);
    if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        printf("Get flash size failed");
        return;
    }

    printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());
}

void app_main(void)
{
    print_chip_info();

    // // Create button using app_reset helper
    // button_handle_t btn_handle = app_reset_button_create(RESET_BUTTON_GPIO, RESET_BUTTON_ACTIVE_LEVEL);
    // if (btn_handle) {
    //     // Register Wi-Fi reset (3 seconds) and Factory reset (10 seconds)
    //     app_reset_button_register(btn_handle, 3, 10);
    // }

    // // Initialize NVS.
    // esp_err_t err = nvs_flash_init();
    // if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    //     ESP_ERROR_CHECK(nvs_flash_erase());
    //     err = nvs_flash_init();
    // }
    // ESP_ERROR_CHECK( err );
    
    // Initialise SPIFFS on the storage partition of flash.
    ESP_LOGI(TAG, "Initializing SPIFFS");
    esp_vfs_spiffs_conf_t spiffs_config = {
        .base_path = "/spiffs",
        .partition_label = NULL, // i.e. the first subtype=spiffs partition
        .max_files = 5,
        .format_if_mount_failed = true
    };

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&spiffs_config);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    // Use POSIX and C standard library functions to work with files.
    // First create a file.
    ESP_LOGI(TAG"/spiffs_test", "Opening file");
    FILE* f = fopen("/spiffs/hello.txt", "w");
    if (f == NULL) {
        ESP_LOGE(TAG"/spiffs_test", "Failed to open file for writing");
        return;
    }
    fprintf(f, "Hello World!\n");
    fclose(f);
    ESP_LOGI(TAG"/spiffs_test", "File written");

    // Open renamed file for reading
    ESP_LOGI(TAG"/spiffs_test", "Reading file");
    f = fopen("/spiffs/hello.txt", "r");
    if (f == NULL) {
        ESP_LOGE(TAG"/spiffs_test", "Failed to open file for reading");
        return;
    }
    char line[64];
    fgets(line, sizeof(line), f);
    fclose(f);
    // strip newline
    char* pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    ESP_LOGI(TAG, "Read from file: '%s'", line);

    // status_updater_init();

    // Create queues
    QueueHandle_t recording_queue = xQueueCreate(5, sizeof(Recording_Item));
    QueueHandle_t api_call_queue = xQueueCreate(10, sizeof(Api_Call_Param));
    QueueHandle_t api_response_queue = xQueueCreate(10, sizeof(Api_Response));
    QueueHandle_t status_updater_queue = xQueueCreate(10, sizeof(Status_Updater_Queue_Param));

    // Create tasks
    TaskHandle_t mic_task_handle;
    TaskHandle_t status_updater_task_handle;
    TaskHandle_t api_task_handle;

    Api_Task_Params api_task_params = {
        .call_queue = api_call_queue,
        .response_queue = api_response_queue
    };

    
    xTaskCreate(mic_task, "mic_task", 40 * 1024, &recording_queue, 10, &mic_task_handle);
    // xTaskCreate(api_task, "api_task", 40 * 1024, &api_task_params, 1, &api_task_handle);
    // xTaskCreate(status_updater_task, "status_updater_task", 40 * 1024, &status_updater_queue, 1, &status_updater_task_handle);
    ESP_LOGI(TAG, "Tasks created");

    Status_Updater_Queue_Param status_updater_param;

    Recording_Item recording;
    Api_Call_Param api_call_param;
    Api_Response api_response;

    while (1) {
        // if (xQueueReceive(recording_queue, &recording, 0)) {
        //     api_call_param.audio_data = recording.buffer;
        //     api_call_param.length = recording.length;
        //     xQueueSend(api_call_queue, &api_call_param, 0);
        // }

        // if (xQueueReceive(api_response_queue, &api_response, 0)) {
        //     status_updater_param.is_understandable = api_response.is_understandable;
        //     xQueueSend(status_updater_queue, &status_updater_param, 0);
        // }

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
