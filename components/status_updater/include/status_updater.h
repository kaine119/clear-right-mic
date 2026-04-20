#ifndef _STATUS_UPDATER_H
#define _STATUS_UPDATER_H
#include <stdbool.h>
#endif

void status_updater_init();
void status_updater_task(void* params);

struct _Status_Updater_Queue_Param {
    bool is_understandable;
};

typedef struct _Status_Updater_Queue_Param Status_Updater_Queue_Param;