/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "main.h"
#include <stdio.h>
#include "api.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "app_reset.h"
#include "status_updater.h"

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

    // Create button using app_reset helper
    button_handle_t btn_handle = app_reset_button_create(RESET_BUTTON_GPIO, RESET_BUTTON_ACTIVE_LEVEL);
    if (btn_handle) {
        // Register Wi-Fi reset (3 seconds) and Factory reset (10 seconds)
        app_reset_button_register(btn_handle, 3, 10);
    }

    // Initialize NVS.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    status_updater_init();

    // Create queues
    // QueueHandle_t api_call_queue = xQueueCreate(100, sizeof(Api_Queue_Param));

    // Create tasks
    // TaskHandle_t api_task_handle;
    // xTaskCreate(api_task, "api_task", 40 * 1024, api_call_queue, 2, &api_task_handle);
    

    while (1) {
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}
