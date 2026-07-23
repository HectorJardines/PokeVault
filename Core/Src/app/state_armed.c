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
    case SECURITY_DISARMED:
        switch (event) {
        case EVENT_UNIT_CLOSED:
            display_refresh_value(LABEL_STATUS, 1);
            msg closed_msg = msg_init_default;
            closed_msg.node_id = NODE_ID;
            closed_msg.which_payload = msg_type_alert_tag;
            closed_msg.payload.type_alert.type = ALERT_SEC_STATUS_CHANGE;
            closed_msg.payload.type_alert.value = 1;
            message_send(&closed_msg);

            armed_state_run(data);
            break;
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
    case SECURITY_ARMED:
    default:
        break;
    }
}



/*********************
 * STATIC DEFS
 */


static uint8_t armed_state_run(struct state_armed_data *data) {
    // switch (data->state) {
    // case ARMED_IDLE:
    //     display_off();
    //     break;
    // case ARMED_DISPLAY:
    //     display_on();
    //     break;
    // }

    // DO NOTHING, THIS STATE ONLY SERVES TO TRANSITION TO AND FROM
    // OTHERS AND POST ALERTS ABOUT SUCH TRANSITIONS
}
