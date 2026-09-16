#include "../../../Core/Inc/common/defines.h"
#include "../../Inc/app/central_node.h"
#include "../../Inc/app/inventory.h"
#include "../../Inc/app/client.h"
#include "../../Inc/common/printf-stdarg.h"
// #include <stdio.h>

#include "../../../FreeRTOS_WrkSpace/include/FreeRTOS.h"
#include "../../../FreeRTOS_WrkSpace/include/task.h"
#include "../../../FreeRTOS_WrkSpace/include/queue.h"

#define MAX_PENDING_MSGS    (15U)
#define MAX_PEER_NODE_CNT   (1U)

#define CENTRAL_NODE_STACK_DEPTH    (512U)
#define CENTRAL_NODE_PRIO           (5U)
#define RX_TIMEOUT_TICKS            (pdMS_TO_TICKS(500))

/*************************
 * STATIC DECLARATIONS
 ***********************/
static uint8_t handle_command_msg(msg* cmd_msg);
static uint8_t handle_msg(msg *message);
static uint8_t handle_alert_msg(msg *alert);
static uint8_t handle_event_msg(msg *event);
static uint8_t central_node_process(msg *message);
static void node_poll_complete_cb(void);
static void task_central_node(void *arg);

static node_state_t central_node = {0,0,0};


static QueueHandle_t msg_arr_q;
static StaticQueue_t _msg_arr_q;
static uint8_t msg_arr_q_buf[MAX_PENDING_MSGS * sizeof(msg)];

static TaskHandle_t central_tsk;
static StaticTask_t _central_tsk;
static StackType_t tsk_stk[CENTRAL_NODE_STACK_DEPTH];
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
    // NO RECEPTION IN PROGRESS INITIALLY
    msg_arr_q = xQueueCreateStatic(MAX_PENDING_MSGS, sizeof(msg), msg_arr_q_buf, &_msg_arr_q);
    central_tsk = xTaskCreateStatic(task_central_node, "Central", CENTRAL_NODE_STACK_DEPTH,
                                    NULL, CENTRAL_NODE_PRIO, tsk_stk, &_central_tsk);
    
    if (central_tsk == NULL) {
        while (1) {}
    }
}

/**
 * @brief Posts a message to the central node
 * 
 * This function posts a msg receives over RS-485 
 * to the central node to be handled by one of the 
 * subsystems.
 * 
 * @return pdFALSE on failure/timeout; pdTRUE on success
 */
uint8_t central_post_msg(msg *message) {
    uint8_t ret = pdFALSE;
    ret = xQueueSendToBack(msg_arr_q, (const void *)message, pdMS_TO_TICKS(100)); // will process errors eventually
    return ret;
}

/*************************
 * STATIC DECLARATIONS
 *************************/

/**
 * @brief This task is responsible for receiving and processing messages
 * 
 * This task blocks on a counting sem(?) waiting for messages
 * sends them to ethernet controller, sd card reader, etc. as 
 * needed. 
 */
static void task_central_node(void *arg) {
    msg msg_in = msg_init_default;
    msg cts_msg = msg_init_default;
    uint8_t ret = STATUS_OK;
    cts_msg.node_id = central_node.curr_node;
    cts_msg.command = MSG_CMD_CTS;
    
    for (;;) {
        ret = c_message_post_out(&cts_msg);
        if (ret == STATUS_OK) {
            if (xQueueReceive(msg_arr_q, (void *)&msg_in, RX_TIMEOUT_TICKS) == pdTRUE)
                ret = central_node_process(&msg_in);
        }
        central_node.curr_node = (central_node.curr_node + 1) % MAX_PEER_NODE_CNT;
        cts_msg.node_id = central_node.curr_node;

        UBaseType_t high_stk_usage = uxTaskGetStackHighWaterMark(NULL);
        if(high_stk_usage <= 25) {
            for(;;);
        }
    }
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
static uint8_t central_node_process(msg *message) {
    uint8_t status = STATUS_ERR;

    if (message->command != MSG_CMD_NONE)
        status = handle_command_msg(message);
    else
        status = handle_msg(message);

    return status;
}



static uint8_t handle_command_msg(msg* cmd_msg) {
    uint8_t status = STATUS_OK;

    switch (cmd_msg->command) {
    case MSG_CMD_CTS:
    case MSG_CMD_SEND_CPLT:
    case MSG_CMD_NONE:
        break;
    }

    return status;
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
        status = inventory_post_event(message);
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
            snprintf((char *)alert_body, MAX_HTTPS_BODY_LEN, "PRESENCE DETECTED: NODE - %d", alert->node_id);
            status = client_post_message(alert_body, strlen(alert_body));
            event.payload.type_event.type = MSG_EVENT_PRESENCE;
        }
        status = handle_event_msg(&event);
        break;
    case ALERT_SEC_STATUS_CHANGE:
        snprintf((char *)alert_body, MAX_HTTPS_BODY_LEN, 
                alert->payload.type_alert.value == 0 ? "UNIT %d DISARMED" : "UNIT %d ARMED", 
                alert->node_id);
        status = client_post_message(alert_body, strlen(alert_body));
        alert->command = CMD_UNIT_STAT_CH;
        inventory_post_event(alert);
        break;
    case ALERT_SECURITY_BREACH:
        snprintf((char *)alert_body, MAX_HTTPS_BODY_LEN, "URGENT: UNIT %d BREACHED", alert->node_id);
        status = client_post_message(alert_body, strlen(alert_body));
        alert->payload.type_alert.value = 2;
        alert->command = CMD_UNIT_STAT_CH;
        inventory_post_event(alert);
        break;
    case ALERT_SYS_HUM:
        snprintf((char *)alert_body, MAX_HTTPS_BODY_LEN, "WARN: UNIT %d EXCESS HUMIDITITY - %d\%", 
                alert->node_id, alert->payload.type_alert.value);
        status = client_post_message(alert_body, strlen(alert_body));
        break;
    case ALERT_SYS_TEMP:
        snprintf((char *)alert_body, MAX_HTTPS_BODY_LEN, "WARN: UNIT %d EXCESS TEMP - %dC",
                alert->node_id, alert->payload.type_alert.value);
        status = client_post_message(alert_body, strlen(alert_body));
        break;
    }

    return status;
}


static uint8_t handle_event_msg(msg *event) {
    uint8_t log_buf[MAX_LOG_BODY_LEN], len = 0, status = STATUS_OK;
    memset((void *)log_buf, 0, sizeof(log_buf));

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

