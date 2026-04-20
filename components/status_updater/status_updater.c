#include "status_updater.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "app_network.h"
#include "esp_log.h"
#include "esp_rmaker_core.h"
#include "esp_rmaker_standard_types.h"


#define TAG "status_updater"

static esp_rmaker_param_t* my_param;

void status_updater_init() {
    // Initialze network config
    app_network_init();

    /* Initialize the ESP RainMaker Agent.
     * Note that this should be called after app_network_init() but before app_network_start()
     */
    esp_rmaker_config_t rainmaker_cfg = {
        .enable_time_sync = false,
    };
    esp_rmaker_node_t *node = esp_rmaker_node_init(&rainmaker_cfg, "ESP RainMaker Device", "Switch");
    if (!node) {
        ESP_LOGE(TAG, "Could not initialise node. Aborting!!!");
        vTaskDelay(5000/portTICK_PERIOD_MS);
        abort();
    }

    esp_rmaker_device_t* switch_device = esp_rmaker_device_create("My Switch", ESP_RMAKER_DEVICE_OTHER, NULL);

    my_param =  esp_rmaker_param_create("My Param", NULL, esp_rmaker_bool(false), PROP_FLAG_READ);
    esp_rmaker_device_add_param(switch_device, my_param);

    esp_rmaker_node_add_device(node, switch_device);

    esp_err_t err = app_network_start(POP_TYPE_RANDOM);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not start Wifi. Aborting!!!");
        vTaskDelay(5000/portTICK_PERIOD_MS);
        abort();    
    }

    esp_rmaker_timezone_service_enable();
    esp_rmaker_start();
}

void status_updater_task(void* params) {
    QueueHandle_t queue = (QueueHandle_t) params;
    Status_Updater_Queue_Param param;

    while (1) {
        if (xQueueReceive(queue, &param, 0)) {
            esp_rmaker_param_update_and_report(my_param, esp_rmaker_bool(param.is_understandable));
        }
        vTaskDelay(1);
    }
}