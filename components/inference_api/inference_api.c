#include "inference_api.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "freertos/idf_additions.h"

#define TAG "api"

void call_gemini_api() {
    esp_http_client_config_t config = {
        .method = HTTP_METHOD_GET,
        .url = "https://example.com"
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    ESP_ERROR_CHECK(esp_http_client_perform(client));

    ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %" PRId64,
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));

    esp_http_client_close(client);
}

void api_task(void* call_queue) {
    Api_Queue_Param* call_params = malloc(sizeof(Api_Queue_Param));
    while (1) {
        if (xQueueReceive(call_queue, call_params, 0)) {

        }
        call_gemini_api();
        vTaskDelay(2000);
    }
}
