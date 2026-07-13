#ifndef _ARMED_H
#define _ARMED_H



#include "state_common.h"

typedef enum {
    ARMED_IDLE,
    ARMED_DISPLAY
} armed_state_e;


struct state_armed_data {
    struct state_common_data *comm;
    armed_state_e state;
};


/****************
 * PUB APIS
 ***************/
/**
 * @brief Initialize the armed state of the peer node
 * 
 * 
 * 
 */
void armed_state_init(struct state_armed_data *);



/**
 * @brief Enter armed state of peer node
 * 
 * 
 * 
 */
void armed_state_enter(struct state_armed_data *data, state_e from, event_e event);


#endif