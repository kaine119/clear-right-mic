#include "inference_api.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "freertos/idf_additions.h"
#include "portmacro.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "gemini_config.h"
#include <string.h>

#define TAG "inference_api"

#define HTTP_REQUEST_BUFFER_SIZE 2048
#define HTTP_RESPONSE_BUFFER_SIZE 8192
#define HTTP_HEADER_BUFFER_SIZE 256


static char http_request_buffer[HTTP_REQUEST_BUFFER_SIZE];
static char http_response_buffer[HTTP_RESPONSE_BUFFER_SIZE];
static char http_response_header_buffer[HTTP_HEADER_BUFFER_SIZE];
static bool http_response_done;

esp_err_t _http_event_handler(esp_http_client_event_t* e) {
    static int output_len;
    switch (e->event_id) {
        case HTTP_EVENT_ON_CONNECTED:
            output_len = 0;
            http_response_done = false;
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", e->data_len);
            // Clean the buffer in case of a new request
            if (output_len == 0) {
                // we are just starting to copy the output data into the use
                memset(http_response_buffer, 0, HTTP_RESPONSE_BUFFER_SIZE);
            }

            // If user_data buffer is configured, copy the response into the buffer
            int copy_len = 0;
            
            // The last byte in the response buffer is kept for the NULL character in case of out-of-bound access.
            copy_len = MIN(e->data_len, (HTTP_RESPONSE_BUFFER_SIZE - output_len));
            if (copy_len) {
                memcpy(http_response_buffer + output_len, e->data, copy_len);
                output_len += copy_len;
            }
            break;
        case HTTP_EVENT_ON_HEADER:
            if (strncmp("X-Goog-Upload-URL", e->header_key, strlen("X-Goog-Upload-URL")) == 0) {
                ESP_LOGI(TAG, "Got an upload url: %s", e->header_value);
                strcpy(http_response_header_buffer, e->header_value);
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            http_response_done = true;
            break;
        case HTTP_EVENT_HEADERS_SENT:
        case HTTP_EVENT_ERROR:
        case HTTP_EVENT_DISCONNECTED:
        case HTTP_EVENT_REDIRECT:
          break;
    }

    return ESP_OK;
}

/**
 * Begin a "resumable" upload, returning a URI to which a file can be uploaded.
 * @param content_length Length of file to upload.
 * @param mime_type      MIME type of file to upload.
 * @param upload_url_buf Buffer to write upload URL to. Typically <300 chars.
 */
esp_err_t initiate_audio_upload(int content_length, char* upload_url_buf) {
    const char post_data[36] = "{\"file\": {\"display_name\": \"AUDIO\"}}";
    
    char content_length_str[10];
    sprintf(content_length_str, "%d", content_length);

    esp_http_client_config_t config = {
        .method = HTTP_METHOD_POST,
        .url = GEMINI_FILE_INITIATE_URL,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = _http_event_handler
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "X-Goog-Api-Key", GEMINI_API_KEY);
    esp_http_client_set_header(client, "X-Goog-Upload-Protocol", "resumable");
    esp_http_client_set_header(client, "X-Goog-Upload-Command", "start");
    esp_http_client_set_header(client, "X-Goog-Upload-Header-Content-Length", content_length_str);
    esp_http_client_set_header(client, "X-Goog-Upload-Header-Content-Type", "audio/wav");


    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t ret = esp_http_client_perform(client);
    if (ret != ESP_OK) {
        esp_http_client_close(client);
        return ret;
    };


    while (!http_response_done) {
        vTaskDelay(10);
    }

    ESP_LOGI(TAG "initiate_audio_upload()", "Response HTTP Status = %d, got upload url: %s", 
        esp_http_client_get_status_code(client), 
        http_response_header_buffer
    );
    
    strcpy(upload_url_buf, http_response_header_buffer);

    return ESP_OK;
}

/**
 * Start uploading audio to the specified URL.
 * @param  file_buf   Pointer to the bytes to upload.
 * @param  length     Byte length of uploaded file. Needs to match the original length specified when initiating the upload.
 * @param  upload_url Where to upload the file.
 * @param  file_uri   Final location of the URI will be copied to this buffer, to be used with future prompts.
 * @return            Upload status
 */
esp_err_t start_audio_upload(char* file_buf, int length, char* upload_url, char* file_uri) {
    esp_http_client_config_t config = {
        .method = HTTP_METHOD_POST,
        .url = upload_url,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = _http_event_handler
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "X-Goog-Upload-Offset", "0");
    esp_http_client_set_header(client, "X-Goog-Upload-Command", "upload,finalize");
    esp_http_client_set_post_field(client, file_buf, length);

    esp_err_t ret = esp_http_client_perform(client);
    if (ret != ESP_OK) {
        esp_http_client_close(client);
        return ret;
    };

    while (!http_response_done) {
        vTaskDelay(10);
    }

    // Parse returned JSON to grab URI. We want "server_response.file.uri".
    cJSON* server_response = cJSON_Parse(http_response_buffer);
    cJSON* server_response_file = cJSON_GetObjectItem(server_response, "file");
    cJSON* server_response_file_uri = cJSON_GetObjectItem(server_response_file, "uri");
    strcpy(file_uri, cJSON_GetStringValue(server_response_file_uri));
    cJSON_Delete(server_response);

    ESP_LOGI(TAG "start_audio_upload()", "responded with file_uri: %s", file_uri);

    return ESP_OK;
}

bool call_model(char* file_uri) {
    // Build request body
    cJSON* request_json = cJSON_CreateObject();

    // contents[0]: {role: String, parts: {text}[]}
    cJSON* contents = cJSON_AddArrayToObject(request_json, "contents");
    
    cJSON* contents_0 = cJSON_CreateObject();

    cJSON_AddStringToObject(contents_0, "role", "user");
    
    cJSON* contents_0_parts = cJSON_CreateArray();
    cJSON* contents_0_parts_0 = cJSON_CreateObject();
    cJSON* contents_0_parts_0_filedata = cJSON_AddObjectToObject(contents_0_parts_0, "file_data");
    cJSON_AddStringToObject(contents_0_parts_0_filedata, "mime_type", "audio/wav");
    cJSON_AddStringToObject(contents_0_parts_0_filedata, "file_uri", file_uri);

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

    cJSON_PrintPreallocated(request_json, http_request_buffer, HTTP_REQUEST_BUFFER_SIZE, false);
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
    esp_http_client_set_post_field(client, http_request_buffer, strlen(http_request_buffer));
    esp_http_client_set_header(client, "Content-Type", "application/json");

    if (esp_http_client_perform(client) != ESP_OK) {
        esp_http_client_close(client);
        
        return false;
    }


    ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %" PRId64,
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));

    while (!http_response_done) {
        vTaskDelay(10);
    }

    ESP_LOGI(TAG "call_model()", "Response:  %s", http_response_buffer);
    esp_http_client_close(client);

    return (strncmp(http_response_buffer, "YES", 3)) == 0;
}

void api_task(void* p) {
    Api_Task_Params* params = (Api_Task_Params*) p;
    Api_Call_Param call_params;
    Api_Response response;

    char upload_uri[300];
    char file_uri[100];

    ESP_LOGI(TAG, "Started");

    while (1) {
        if (xQueueReceive(params->call_queue, &call_params, 0)) {
            initiate_audio_upload(call_params.length, upload_uri);
            start_audio_upload(call_params.audio_data, call_params.length, upload_uri, file_uri);
            response.is_understandable = call_model(file_uri);
            xQueueSend(params->response_queue, &response, 0);
        }
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}
