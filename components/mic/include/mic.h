#ifndef _MIC_H
#define _MIC_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "wav_header.h"

#define NUM_RECORDING_BUFFERS 3
#define RECORDING_DURATION_SEC 15
#define SAMPLE_RATE 8000

struct _Mic_Task_Queue_Item {
    char* buffer; // .wav data, when available
    int length;   // length of file, in bytes
};
typedef struct _Mic_Task_Queue_Item Recording_Item;

struct _Audio_Buffer {
    wav_header_t header;
    char audio_data[RECORDING_DURATION_SEC * SAMPLE_RATE * 3];
};
typedef struct _Audio_Buffer Audio_Buffer;

void mic_task(void* params);

#endif