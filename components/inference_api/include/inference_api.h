#ifndef _API_H
#define _API_H

#include "freertos/idf_additions.h"
#include <stdint.h>

/**
 * The API caller task.
 * @param call_queue Queue that fires off API calls. Pass in audio data buffers. 
 */
void api_task(void* call_queue);

struct _Api_Task_Params {
    QueueHandle_t call_queue;
    QueueHandle_t response_queue;
};

typedef struct _Api_Task_Params Api_Task_Params;

struct _Api_Call_Param {
    char filename[16];
    int length;
};

typedef struct _Api_Call_Param Api_Call_Param;

struct _Api_Response {
    bool is_understandable;
};

typedef struct _Api_Response Api_Response;

#endif
