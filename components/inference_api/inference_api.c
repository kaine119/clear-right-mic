#include "inference_api.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "freertos/idf_additions.h"
#include "portmacro.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "gemini_config.h"

#define TAG "inference_api"

#define HTTP_REQUEST_BUFFER_SIZE 2048
#define HTTP_RESPONSE_BUFFER_SIZE 8192


static char request_buffer[HTTP_REQUEST_BUFFER_SIZE];
static char response_buffer[HTTP_RESPONSE_BUFFER_SIZE];
static bool response_done;

esp_err_t _http_event_handler(esp_http_client_event_t* e) {
    static int output_len;
    switch (e->event_id) {
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", e->data_len);
            // Clean the buffer in case of a new request
            if (output_len == 0) {
                // we are just starting to copy the output data into the use
                memset(response_buffer, 0, HTTP_RESPONSE_BUFFER_SIZE);
            }

            // If user_data buffer is configured, copy the response into the buffer
            int copy_len = 0;
            
            // The last byte in the response buffer is kept for the NULL character in case of out-of-bound access.
            copy_len = MIN(e->data_len, (HTTP_RESPONSE_BUFFER_SIZE - output_len));
            if (copy_len) {
                memcpy(response_buffer + output_len, e->data, copy_len);
                output_len += copy_len;
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            response_done = true;
            break;
        case HTTP_EVENT_HEADERS_SENT:
            output_len = 0;
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
    // Build request body
    cJSON* request_json = cJSON_CreateObject();

    // contents[0]: {role: String, parts: {text}[]}
    cJSON* contents = cJSON_AddArrayToObject(request_json, "contents");
    
    cJSON* contents_0 = cJSON_CreateObject();

    cJSON_AddStringToObject(contents_0, "role", "user");
    
    cJSON* contents_0_parts = cJSON_CreateArray();
    cJSON* contents_0_parts_0 = cJSON_CreateObject();
    cJSON_AddStringToObject(contents_0_parts_0, "text", TEST_PROMPT);
    cJSON_AddItemToArray(contents_0_parts, contents_0_parts_0);
    
    cJSON_AddItemToObject(contents_0, "parts", contents_0_parts);

    cJSON_AddItemToArray(contents, contents_0);

    // generation config
    cJSON* generationConfig = cJSON_AddObjectToObject(request_json, "generationConfig");
    cJSON* thinkingConfig = cJSON_AddObjectToObject(generationConfig, "thinkingConfig");
    cJSON_AddStringToObject(thinkingConfig, "thinkingLevel", "MINIMAL");

    // System instruction
    cJSON* systemInstruction = cJSON_AddObjectToObject(request_json, "systemInstruction");
    cJSON* systemInstruction_parts = cJSON_AddArrayToObject(systemInstruction, "parts");
    
    cJSON* systemInstruction_parts_0 = cJSON_CreateObject();
    cJSON_AddStringToObject(systemInstruction_parts_0, "text", SYSTEM_INSTRUCTION);

    cJSON_AddItemToArray(systemInstruction_parts, systemInstruction_parts_0);

    cJSON_PrintPreallocated(request_json, request_buffer, HTTP_REQUEST_BUFFER_SIZE, false);
    ESP_LOGI(TAG, "Calling Gemini API with: \n%s", cJSON_Print(request_json));
    
    cJSON_Delete(request_json);


    esp_http_client_config_t config = {
        .method = HTTP_METHOD_POST,
        .url = GEMINI_URL,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = _http_event_handler
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "x-goog-api-key", GEMINI_API_KEY);
    esp_http_client_set_post_field(client, request_buffer, strlen(request_buffer));
    esp_http_client_set_header(client, "Content-Type", "application/json");

    if (esp_http_client_perform(client) != ESP_OK) {
        esp_http_client_close(client);
        
        return;
    }


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
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}
