#include "ff.h"

/* Create a sync object */
int ff_cre_syncobj (BYTE vol, _SYNC_t* sobj) {
    *sobj = xSemaphoreCreateMutex();
    if (*sobj == NULL) {
        return 0;
    }
    return 1;
}


/* Lock sync object */
int ff_req_grant (_SYNC_t sobj) {
    BaseType_t res = xSemaphoreTake(sobj, _FS_TIMEOUT);
    if (res == pdTRUE)
        return 1;
    else
        return 0;
}

/* Unlock sync object */			
void ff_rel_grant (_SYNC_t sobj) {
    xSemaphoreGive(sobj);
}


/* Delete a sync object */		
int ff_del_syncobj (_SYNC_t sobj) {
    vQueueDelete((QueueHandle_t) sobj);
    return 1;
}