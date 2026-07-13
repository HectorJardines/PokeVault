#ifndef _BREACHED_H
#define _BREACHED_H

#include "state_common.h"

typedef enum {
    BREACHED_INIT,
    BREACHED_BUZZER,
    BREACHED_ALERT,
    BREACHED_CHECK_DISARMED
} breached_state_e;


struct state_breached_data {
    struct state_common_data *comm;
    breached_state_e state;
};


/****************
 * PUB APIS
 ***************/
/**
 * @brief Initialize the breached state of the peer node
 * 
 * 
 * 
 */
void breached_state_init(struct state_breached_data *);



/**
 * @brief Enter breached state of peer node
 * 
 * 
 * 
 */
void breached_state_enter(struct state_breached_data *data, state_e from, event_e event);


#endif