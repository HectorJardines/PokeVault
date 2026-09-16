#include "state_breached.h"

/*************************
 * STATIC DECLARATIONS
 *************************/
#define BREACHED_ALERT_T    (15000) // 15 second breach alert period

/**
 * @brief 
 * 
 * 
 * @param[in] data
 * 
 * @return
 */
static uint8_t breached_state_run(struct state_breached_data *data);
/*****************
 * PUBLIC APIs
 *****************/



/**
 * @brief Initialize the breached state of the peer node
 * 
 * 
 * 
 */
void breached_state_init(struct state_breached_data *data) {
    data->state = BREACHED_INIT;
}



/**
 * @brief Enter breached state of peer node
 * 
 * 
 * 
 */
void breached_state_enter(struct state_breached_data *data, state_e from, event_e event) {
    switch (from) {
    case SECURITY_ARMED:
        switch (event) {
        case EVENT_UNIT_MOVED:
            display_refresh_value(LABEL_STATUS, 2);
            msg unauth_move = msg_init_default;
            unauth_move.node_id = NODE_ID;
            unauth_move.which_payload = msg_type_event_tag;
            unauth_move.payload.type_event.type = MSG_EVENT_UNIT_MOVE;
            message_send(&unauth_move);

            breached_state_run(data);
            break;
        case EVENT_UNIT_OPENED:
            display_refresh_value(LABEL_STATUS, 2);
            msg unauth_open = msg_init_default;
            unauth_open.node_id = NODE_ID;
            unauth_open.which_payload = msg_type_event_tag;
            unauth_open.payload.type_event.type = MSG_EVENT_UNIT_OPEN;
            message_send(&unauth_open);

            breached_state_run(data);
            break;
        case EVENT_ITEM_SCAN:
        case EVENT_REMOTE_AUTH:
        case EVENT_TAG_AUTH:
        case EVENT_UNIT_CLOSED:
        case EVENT_NONE:
        default:
            break; // NONE OF THE ABOVE SHOULD ENTER THIS STATE
        }
        break;
    case SECURITY_BREACHED:
        switch (event) {
        case EVENT_ITEM_SCAN: // SHOULD STILL REPORT ANY INVENTORY CHANGES
            inventory_item_update();
            breached_state_run(data);
            break;
        case EVENT_NONE:
            breached_state_run(data);
            break;
        case EVENT_REMOTE_AUTH:
        case EVENT_TAG_AUTH:
        case EVENT_UNIT_MOVED:
        case EVENT_UNIT_OPENED:
        case EVENT_UNIT_CLOSED:
        default:
            break; // NONE OF THE ABOVE SHOULD ENTER THIS STATE
        }
        break;
    case SECURITY_DISARMED: // SHOUDLN'T COME FROM HERE
    default:
        break;
    }
}



/****************
 * STATIC DEFS
 ****************/
static uint8_t breached_state_run(struct state_breached_data *data) {
    msg breached_msg = msg_init_default;
    breached_msg.node_id = NODE_ID;
    breached_msg.which_payload = msg_type_alert_tag;
    breached_msg.payload.type_alert.type = ALERT_SECURITY_BREACH;
    
    uint32_t curr_tick = HAL_GetTick();
    switch (data->state) {
    case BREACHED_INIT:
    case BREACHED_TICK:
        breached_msg.payload.type_alert.value = 0; // first alert issued
        message_send(&breached_msg);
        data->state = BREACHED_WAIT;
        break;
    case BREACHED_WAIT:
        if (curr_tick - data->last_breached_tick > BREACHED_ALERT_T) {
            data->state = BREACHED_TICK;
            data->last_breached_tick = curr_tick;
        }
        break;
    }
}


