#include "../../../Core/Inc/common/defines.h"
#include "../../Inc/app/central_node.h"
#include "../../Inc/app/central_message.h"
#include "../../Inc/app/inventory.h"
#include "../../Inc/app/rfid_tag.h"
#include "../../Inc/app/client.h"
#include "ring_buffer.h"
#include <stdio.h>

#define MAX_PENDING_MSGS    (15U)
#define MAX_PEER_NODE_CNT   (1U)

/*************************
 * STATIC DECLARATIONS
 ***********************/
static uint8_t handle_command_msg(msg* cmd_msg);
static uint8_t handle_msg(msg *message);
static uint8_t handle_alert_msg(msg *alert);
static uint8_t handle_event_msg(msg *event);
static void node_poll_complete_cb(void);

static node_state_t central_node = {0,0,0};
STATIC_RING_BUFFER(pending_msgs, MAX_PENDING_MSGS, msg);
/**************
 * PUB APIs
 *************/

/**
 * @brief Initialize the central node subsystems
 * 
 * Initializes the client, inventory, messaging,
 * and rfid_tag subsystems of the central node.
 * 
 */
void central_node_init(void) {
    register_peer_rx_cplt_cb(node_poll_complete_cb);
    c_message_init();
    client_init();
    client_connect();
    tag_init();
    // c_inventory_init();
    
    // NO RECEPTION IN PROGRESS INITIALLY
    central_node.flags = 0x00;
    central_node.flags |= (PEER_RX_CPLT_Msk);
}


/**
 * @brief Polls the peer nodes for any pending messages
 * 
 * This function polls one peer node and returns. Should not be 
 * called again until the peer node has sent it's pending messages or 
 * a tiemout occurs.
 * 
 */
uint8_t central_node_poll_peer(void) {
    uint8_t status = STATUS_ERR;

    if (central_node.flags & PEER_RX_CPLT_Msk) {
        msg cts_msg = msg_init_default;
        cts_msg.node_id = central_node.curr_node;
        cts_msg.command = MSG_CMD_CTS;

        status = c_message_send(&cts_msg);
        if (status == STATUS_OK) {
            central_node.flags &= ~(PEER_RX_CPLT_Msk); // cleared until RX cplt
            status = STATUS_WAIT;
        }
    }

    if (central_node.flags & PEER_MSG_READY_Msk) {
        msg_array arr = msg_array_init_default;
        status = c_message_receive(&arr);
        if (status == STATUS_OK) {
            if (arr.msgs[arr.msgs_count - 1].command != MSG_CMD_SEND_CPLT)
                status = STATUS_ERR;
            for (uint8_t i = 0; i < arr.msgs_count - 1; ++i)
                ring_buffer_push(&pending_msgs, (void *)&arr.msgs[i]);
            central_node.pending_msg_cnt = ring_buffer_count(&pending_msgs);
        }
        central_node.flags |= PEER_RX_CPLT_Msk;
        central_node.flags &= ~PEER_MSG_READY_Msk;
        central_node.curr_node = (central_node.curr_node + 1) % MAX_PEER_NODE_CNT;
    }

    return status;
}



/**
 * @brief Processes a single pending message
 * 
 * This function retrieves and processes a message received
 * from a peer node. E.g. if the buffered message contains 
 * a security breach alert, the central node will send
 * the alert via the ethernet module to the mobile device.
 * 
 * @return 0 on success; 1 else
 * 
 */
uint8_t central_node_process(void) {
    uint8_t status = STATUS_ERR;
    msg curr_msg = msg_init_default;

    if (!ring_buffer_empty(&pending_msgs)) {
        ring_buffer_pop(&pending_msgs, (void *)&curr_msg);
        central_node.pending_msg_cnt = ring_buffer_count(&pending_msgs);

        if (curr_msg.command != MSG_CMD_NONE)
            status = handle_command_msg(&curr_msg);
        else
            status = handle_msg(&curr_msg);
    }

    return status;
}


/*************************
 * STATIC DECLARATIONS
 *************************/
static uint8_t handle_command_msg(msg* cmd_msg) {
    uint8_t status = STATUS_OK;

    switch (cmd_msg->command) {
    case MSG_CMD_CTS:
    case MSG_CMD_SEND_CPLT:
    case MSG_CMD_NONE:
        break;
    }
}

static uint8_t handle_msg(msg *message) {
    uint8_t status = STATUS_OK;

    switch (message->which_payload) {
    case msg_type_alert_tag:
        status = handle_alert_msg(message);
        break;
    case msg_type_event_tag:
        status = handle_event_msg(message);
        break;
    case msg_type_transaction_tag:
        status = inventory_process_transaction(message);
        break;
    }

    return status;
}

static uint8_t handle_alert_msg(msg *alert) {
    uint8_t status = STATUS_OK, len = 0;
    uint8_t alert_body[MAX_HTTPS_BODY_LEN];
    memset((void *)alert_body, 0, MAX_HTTPS_BODY_LEN);

    switch (alert->payload.type_alert.type) {
    case ALERT_PRESENCE:
        msg event = msg_init_default;
        event.node_id = alert->node_id;
        event.which_payload = msg_type_event_tag;
        event.payload.type_event.type = MSG_EVENT_NO_PRESENCE;
        if (alert->payload.type_alert.value == 1) {
            len = snprintf((char *)alert_body, MAX_HTTPS_BODY_LEN, "PRESENCE DETECTED: NODE - %d", alert->node_id);
            status = client_post_message(alert_body, len);
            event.payload.type_event.type = MSG_EVENT_PRESENCE;
        }
        status = handle_event_msg(&event);
        break;
    case ALERT_SEC_STATUS_CHANGE:
        len = snprintf((char *)alert_body, MAX_HTTPS_BODY_LEN, 
                alert->payload.type_alert.value == 0 ? "UNIT %d DISARMED" : "UNIT %d ARMED", 
                alert->node_id);
        status = client_post_message(alert_body, len);
        break;
    case ALERT_SECURITY_BREACH:
        len = snprintf((char *)alert_body, MAX_HTTPS_BODY_LEN, "URGENT: UNIT %d BREACHED", alert->node_id);
        status = client_post_message(alert_body, len);
        break;
    case ALERT_SYS_HUM:
        len = snprintf((char *)alert_body, MAX_HTTPS_BODY_LEN, "WARN: UNIT %d EXCESS HUMIDITITY - %d\%", 
                alert->node_id, alert->payload.type_alert.value);
        status = client_post_message(alert_body, len);
        break;
    case ALERT_SYS_TEMP:
        len = snprintf((char *)alert_body, MAX_HTTPS_BODY_LEN, "WARN: UNIT %d EXCESS TEMP - %d°C",
                alert->node_id, alert->payload.type_alert.value);
        status = client_post_message(alert_body, len);
        break;
    }

    return status;
}


static uint8_t handle_event_msg(msg *event) {
    uint8_t log_buf[MAX_LOG_BODY_LEN], len = 0, status = STATUS_OK;

    switch (event->payload.type_event.type) {
    case MSG_EVENT_DISARMED:
        snprintf(log_buf, MAX_LOG_BODY_LEN, "UNIT %d - DISARMED\r\n", event->node_id);
        status = log_event(log_buf);
        break;
    case MSG_EVENT_PRESENCE:
        snprintf(log_buf, MAX_LOG_BODY_LEN, "UNIT %d - PRESENCE DETECTED\r\n", event->node_id);
        status = log_event(log_buf);
        break;
    case MSG_EVENT_NO_PRESENCE:
        snprintf(log_buf, MAX_LOG_BODY_LEN, "UNIT %d - PRESENCE GONE\r\n", event->node_id);
        status = log_event(log_buf);
        break;
    case MSG_EVENT_UNIT_CLOSE:
        snprintf(log_buf, MAX_LOG_BODY_LEN, "UNIT %d - CLOSED\r\n", event->node_id);
        status = log_event(log_buf);
        break;
    case MSG_EVENT_UNIT_OPEN:
        snprintf(log_buf, MAX_LOG_BODY_LEN, "UNIT %d - OPEN\r\n", event->node_id);
        status = log_event(log_buf);
        break;
    case MSG_EVENT_UNIT_MOVE:
        snprintf(log_buf, MAX_LOG_BODY_LEN, "UNIT %d - MOVED\r\n", event->node_id);
        status = log_event(log_buf);
        break;
    }

    return status;
}

void timeout_peer_poll(void) {
    central_node.flags &= ~PEER_MSG_READY_Msk;
    central_node.flags |= PEER_RX_CPLT_Msk;
}

static void node_poll_complete_cb(void) {
    central_node.flags |= (PEER_MSG_READY_Msk);
}

