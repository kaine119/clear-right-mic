#ifndef _MIC_H
#define _MIC_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "wav_header.h"

#define NUM_RECORDING_BUFFERS 5
#define RECORDING_DURATION_SEC 10
#define SAMPLE_RATE 16000

struct _Recording_Item {
    char filename[16]; // File name of new .wav file
    int length;   // length of file, in bytes
};
typedef struct _Recording_Item Recording_Item;

void mic_task(void* params);

#endif