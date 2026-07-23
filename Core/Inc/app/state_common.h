#ifndef _STATE_COMM_H
#define _STATE_COMM_H

#include "system_status.h"
#include "inventory.h"
#include "message.h"
#include "display.h"

typedef enum {
    SECURITY_ARMED,
    SECURITY_DISARMED,
    SECURITY_BREACHED
} state_e;

typedef enum {
    EVENT_NONE,
    EVENT_UNIT_OPENED,
    EVENT_UNIT_CLOSED,
    EVENT_TAG_AUTH,
    EVENT_REMOTE_AUTH,
    EVENT_UNIT_MOVED,
    EVENT_ITEM_SCAN
} event_e;

extern struct security_sm_t;
extern transaction_t;
typedef uint32_t timer_t;

struct state_common_data {
    struct security_sm_t *sec_sm_data;

    timer_t *timer;
    system_info_t sys_sens_status;
    transaction_t trans_info;
    uint8_t is_open;
};

#endif