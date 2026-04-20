#ifndef _API_H
#define _API_H
#include <stdint.h>
#endif

/**
 * The API caller task.
 * @param call_queue Queue that fires off API calls. Pass in audio data buffers. 
 */
void api_task(void* call_queue);

struct _Api_Queue_Param {
    uint8_t* audio_data;
};

typedef struct _Api_Queue_Param Api_Queue_Param;