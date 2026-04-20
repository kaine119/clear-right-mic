#include "inference_api.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "freertos/idf_additions.h"
#include "portmacro.h"
#include "esp_crt_bundle.h"

#define TAG "inference_api"
#define MAX_HTTP_OUTPUT_BUFFER 2048

static char response_buffer[MAX_HTTP_OUTPUT_BUFFER];
static bool response_done;

esp_err_t _http_event_handler(esp_http_client_event_t* e) {
    static int output_len;
    switch (e->event_id) {
        case HTTP_EVENT_ON_DATA:
            assert(e->user_data);

            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", e->data_len);
            // Clean the buffer in case of a new request
            if (output_len == 0) {
                // we are just starting to copy the output data into the use
                memset(e->user_data, 0, MAX_HTTP_OUTPUT_BUFFER);
            }

            // If user_data buffer is configured, copy the response into the buffer
            int copy_len = 0;
            
            // The last byte in e->user_data is kept for the NULL character in case of out-of-bound access.
            copy_len = MIN(e->data_len, (MAX_HTTP_OUTPUT_BUFFER - output_len));
            if (copy_len) {
                memcpy(e->user_data + output_len, e->data, copy_len);
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            response_done = true;
            break;
        case HTTP_EVENT_HEADERS_SENT:
            response_done = false;
            break;
        case HTTP_EVENT_ERROR:
        case HTTP_EVENT_ON_CONNECTED:
        case HTTP_EVENT_ON_HEADER:
        case HTTP_EVENT_DISCONNECTED:
        case HTTP_EVENT_REDIRECT:
          break;
    }

    return ESP_OK;
}

void call_gemini_api() {
    esp_http_client_config_t config = {
        .method = HTTP_METHOD_GET,
        .url = "https://www.example.com",
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = _http_event_handler,
        .user_data = response_buffer
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    ESP_ERROR_CHECK(esp_http_client_perform(client));

    ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %" PRId64,
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));

    while (!response_done) {
        vTaskDelay(10);
    }

    ESP_LOGI(TAG, "Response from server: %s", response_buffer);

    esp_http_client_close(client);
}

void api_task(void* call_queue) {
    Api_Queue_Param call_params;
    while (1) {
        if (xQueueReceive(call_queue, &call_params, 0)) {
            
        }
        call_gemini_api();
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}
