#ifndef _STATE_DISARMED_H
#define _STAT_DISARMED_H

#include "state_common.h"

typedef enum {
    DISARMED_IDLE,
    DISARMED_CLOSED,
    DISARMED_TRANS,
    DISARMED_OPEN
} disarmed_state_e;


struct state_disarmed_data {
    struct state_common_data *comm;
    disarmed_state_e state;
};


/****************
 * PUB APIS
 ***************/
/**
 * @brief Initialize the disarmed state of the peer node
 * 
 * 
 * 
 */
void disarmed_state_init(struct state_disarmed_data *data);



/**
 * @brief Enter disarmed state of peer node
 * 
 * 
 * 
 */
void disarmed_state_enter(struct state_disarmed_data *data, state_e from, event_e event);

#endif /* _DISARMED_H */