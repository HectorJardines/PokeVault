#include "state_armed.h"


/**********************
 * STATIC DECLARATIONS
 */

/**
 * @brief 
 * 
 * 
 * @param[in] data
 * 
 * @return 
 */
static uint8_t armed_state_run(struct state_armed_data *data);



/*******************
 * PUBCLIC APIs
 */


/**
 * @brief Initialize the armed state of the peer node
 * 
 * 
 * 
 */
void armed_state_init(struct state_armed_data *data) {
    data->state = ARMED_IDLE;
}



/**
 * @brief Enter armed state of peer node
 * 
 * 
 * 
 */
void armed_state_enter(struct state_armed_data *data, state_e from, event_e event) {
    switch (from) {
    case SECURITY_ARMED:
        switch (event) {
        case EVENT_PRESENCE:
            if (data->state == ARMED_IDLE)
                data->state = ARMED_DISPLAY;
            armed_state_run(data);
            break;
        case EVENT_NO_PRESENCE:
            if (data->state == ARMED_DISPLAY)
                data->state = ARMED_IDLE;
            armed_state_run(data);
            break;
        case EVENT_NONE: // SHOULD DO NOTHING IN THESE EVENT
        case EVENT_REMOTE_AUTH:
        case EVENT_TAG_AUTH:
        case EVENT_ITEM_SCAN:
        case EVENT_UNIT_MOVED:
        case EVENT_UNIT_OPENED:
        case EVENT_UNIT_CLOSED:
        default:
            break;
        }
        break;
    case SECURITY_DISARMED:
        switch (event) {
        case EVENT_UNIT_CLOSED:
            msg closed_msg = msg_init_default;
            closed_msg.node_id = NODE_ID;
            closed_msg.which_payload = msg_type_event_tag;
            closed_msg.payload.type_event.type = MSG_EVENT_UNIT_CLOSE;
            message_send(&closed_msg);

            armed_state_run(data);
            break;
        case EVENT_PRESENCE: // SHOULD DO NOTHING IN THESE EVENT
        case EVENT_NO_PRESENCE:
        case EVENT_NONE: 
        case EVENT_REMOTE_AUTH:
        case EVENT_TAG_AUTH:
        case EVENT_ITEM_SCAN:
        case EVENT_UNIT_MOVED:
        case EVENT_UNIT_OPENED:
        default:
            break;
        }
        break;
    case SECURITY_BREACHED: // SHOULD GO BACK TO DISARMED AFTER BREACH
    default:
        break;
    }
}



/*********************
 * STATIC DEFS
 */


static uint8_t armed_state_run(struct state_armed_data *data) {
    switch (data->state) {
    case ARMED_IDLE:
        display_off();
        break;
    case ARMED_DISPLAY:
        display_on();
        break;
    }
}
