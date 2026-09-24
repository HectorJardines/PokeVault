#include "state_disarmed.h"


/***********************
 * STATIC DECLARATIONS
 ***********************/

/**
 * @brief Executes the substate actions
 * 
 * 
 * @param[in] data 
 * 
 * @return 0 as long as state action has not been completed; 1 when 
 * state action is complete
 */
static uint8_t disarmed_state_run(struct state_disarmed_data *data);


/*******************
 * PUBLIC APIs
 *********************/



/**
 * @brief Initialize the disarmed state of the peer node
 * 
 * 
 * 
 */
void disarmed_state_init(struct state_disarmed_data *data) {
    data->state = DISARMED_IDLE;
}



/**
 * @brief Enter disarmed state of peer node
 * 
 * 
 * 
 */
void disarmed_state_enter(struct state_disarmed_data *data, state_e from, event_e event) {
    switch (from) {
    case SECURITY_ARMED:
    case SECURITY_BREACHED:
        switch (event) {
        case EVENT_TAG_AUTH:
        case EVENT_REMOTE_AUTH:
            data->state = DISARMED_IDLE;
            disarmed_state_run(data);
            break;
        case EVENT_ITEM_SCAN:
        case EVENT_UNIT_OPENED:
        case EVENT_UNIT_CLOSED:
        case EVENT_UNIT_MOVED:
        case EVENT_NONE:
            break;
        }
        break;
    case SECURITY_DISARMED:
        switch (event) {
        case EVENT_ITEM_SCAN:
            if (data->state == DISARMED_OPEN) {
                data->state = DISARMED_TRANS;
                disarmed_state_run(data);
            }
            break;
        case EVENT_UNIT_OPENED:
            if (data->state == DISARMED_CLOSED || data->state == DISARMED_IDLE)
                data->state = DISARMED_OPEN;
            disarmed_state_run(data);
            break;
        case EVENT_UNIT_CLOSED:
            if (data->state == DISARMED_OPEN)
                data->state = DISARMED_CLOSED;
            disarmed_state_run(data);
            break;
        case EVENT_TAG_AUTH:
        case EVENT_REMOTE_AUTH:
        case EVENT_UNIT_MOVED:
        case EVENT_NONE:
            break;
        }
        break;
    }
}



/********************
 * STATIC DEFS
 ********************/

static uint8_t disarmed_state_run(struct state_disarmed_data *data) {
    uint8_t status = STATUS_OK;
    msg disarmed_msg = msg_init_default;
    disarmed_msg.node_id = NODE_ID;

    switch (data->state) {
    case DISARMED_CLOSED:
        disarmed_msg.which_payload = msg_type_event_tag;
        disarmed_msg.payload.type_event.type = MSG_EVENT_UNIT_CLOSE;
        break;
    case DISARMED_OPEN:
        disarmed_msg.which_payload = msg_type_event_tag;
        disarmed_msg.payload.type_event.type = MSG_EVENT_UNIT_OPEN;
        break;
    case DISARMED_TRANS:
        status = inventory_item_update();
        if (status == STATUS_OK)
            data->state = DISARMED_OPEN;
        return status;
    case DISARMED_IDLE:
        display_refresh_value(LABEL_STATUS, 0);
        disarmed_msg.which_payload = msg_type_alert_tag;
        disarmed_msg.payload.type_alert.type = ALERT_SEC_STATUS_CHANGE;
        disarmed_msg.payload.type_alert.value = 0;
        break;
    default:
        break;
    }
    message_send(&disarmed_msg);
    return status;
}


