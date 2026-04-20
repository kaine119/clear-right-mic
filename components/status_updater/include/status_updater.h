#ifndef _STATUS_UPDATER_H
#define _STATUS_UPDATER_H
#include <stdbool.h>
#endif

void status_updater_init();

struct {
    bool is_understandable;
} _Status_Updater_Call_Param;

typedef struct _Status_Updater_Call_Param Status_Updater_Call_Param;