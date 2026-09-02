#include "system_status.h"
#include "message.h"
#include "display.h"
#include "rfid_tag.h"


#define TEMP_DELTA          (2U)
#define HUM_DELTA           (2U)
#define EXCESS_TEMP_THRSH   (26) // in celcius
#define EXCESS_HUM_THRSH    (60) // percentage
#define TEMP    (0U)
#define HUM     (1U)

/************************
 * STATIC DECLARATIONS
 ************************/
static uint8_t handle_excess_temp_hum(uint32_t value, uint8_t temp_or_hum);
static uint8_t handle_presence_detect(void);
static uint8_t handle_presence_gone(void);

static system_info_t active_sys_state;
/*********************
 * PUB APIs
 ***************/

/**
 * @brief Initialize system monitoring sensors
 * 
 * 
 * Initializes the required sensor submodules.
 * 
 */
uint8_t system_monitor_init(void) {
    uint8_t status = aht20_init();
    if (status == STATUS_OK)
        status = ir_init();
    if (status == STATUS_OK)
        status = movement_init();
    memset((void *)&active_sys_state, 0, sizeof(system_info_t));

    return status;
}



/**
 * @brief Retrieve the current system state
 * 
 * Retrieves the current state of system sensors 
 * i.e. PIR, IR, IMU, and temp/humidity sensors. Primarily
 * used for stateful condition checks. E.g. IR line connected 
 * when unit is armed == unit security breach
 * 
 * @param[out] sys_state
 */
uint8_t system_retrieve_state(system_info_t *sys_state) {
    uint8_t status = STATUS_OK;

    sys_state->unit_movement.motion_detected = movement_detected();
    sys_state->unit_movement.tap_detected = movement_tap_detected();

    // unit open/close and presence detection logic
    ir_sens_state_t sens_status;
    ir_read_sens_state(&sens_status);
    sys_state->line_connected = (sens_status.line_state == IR_LINE_CONNECTED);
    sys_state->line_broken = (sens_status.line_state == IR_LINE_BROKEN);
    sys_state->presence = (sens_status.presence_state == PIR_PRESENCE);
    sys_state->no_presence = (sens_status.presence_state == PIR_NO_PRESENCE);

    status = aht20_read_data(&sys_state->temp_hum_readings);
    if (status == 2)
        status = STATUS_OK;
    return status;
}



/**
 * @brief Handle internal state conditions
 * 
 * State conditions like excessive temp/humidity are handled 
 * internally as they do not rely on other system state e.g.
 * movement detect/unit open when unit not unlocked. This function 
 * is called periodically to alert central node of stateless 
 * system conditions.
 * 
 */
uint8_t system_process_state(void) {
    system_info_t tmp;
    tmp.temp_hum_readings.humidity = active_sys_state.temp_hum_readings.humidity;
    tmp.temp_hum_readings.temp = active_sys_state.temp_hum_readings.temp;
    uint8_t status = system_retrieve_state(&tmp);

    if (status == STATUS_OK) {

        if (tmp.temp_hum_readings.temp != active_sys_state.temp_hum_readings.temp)
            status |= display_refresh_value(LABEL_TEMP, tmp.temp_hum_readings.temp);
        if (tmp.temp_hum_readings.humidity != active_sys_state.temp_hum_readings.humidity)
            status |= display_refresh_value(LABEL_HUM, tmp.temp_hum_readings.humidity);

        if (tmp.temp_hum_readings.temp > EXCESS_TEMP_THRSH && 
            (tmp.temp_hum_readings.temp > active_sys_state.temp_hum_readings.temp + TEMP_DELTA)) // don't want to spam messages
            status |= handle_excess_temp_hum(tmp.temp_hum_readings.temp, TEMP);
        if (tmp.temp_hum_readings.humidity > EXCESS_HUM_THRSH && 
            (tmp.temp_hum_readings.humidity > active_sys_state.temp_hum_readings.humidity + HUM_DELTA))
            status |= handle_excess_temp_hum(tmp.temp_hum_readings.humidity, HUM);
        if (tmp.presence && active_sys_state.no_presence) // if hasn't changed back to no presence don't alert again
            status |= handle_presence_detect();
        if (tmp.no_presence && active_sys_state.presence) // same logic
            status |= handle_presence_gone();

        memcpy((void *)&active_sys_state, (void *)&tmp, sizeof(system_info_t));
    }
    return status;
}



/**
 * @brief Scans for nearby PICC, checks if it is an AUTH card
 * 
 * This function is periodically called to scan for nearby PICC,
 * in the case that a PICC is deteceted the ITEM sector block
 * is read to retrieve the type of PICC (e.g. item or auth card).
 * If an auth card is detected this function posts a message to 
 * the security sm and updates armed status on display.
 * 
 * 
 * @return 1 if a PICC is detected and it is of type AUTH card;
 * 0 else
 */
uint8_t system_check_card_auth(void) {
    uint8_t is_auth = 1, status = STATUS_OK;
    uint8_t dummy;

    status = tag_read_keycard_data(NULL);
    if (status != STATUS_OK)
        is_auth = 0;

    return is_auth;
}



/*****************
 * STATIC DEFS
 ****************/
static uint8_t handle_excess_temp_hum(uint32_t value, uint8_t temp_or_hum) {
    uint8_t status = STATUS_OK;
    msg temp_hum_msg = msg_init_default;
    temp_hum_msg.node_id = NODE_ID;
    temp_hum_msg.which_payload = msg_type_alert_tag;

    if (temp_or_hum == TEMP)
        temp_hum_msg.payload.type_alert.type = ALERT_SYS_TEMP;
    else
        temp_hum_msg.payload.type_alert.type = ALERT_SYS_HUM;

    temp_hum_msg.payload.type_alert.value = value;
    message_send(&temp_hum_msg);

    return status;
}



static uint8_t handle_presence_detect(void) {
    uint8_t status = STATUS_OK;
    msg presence_msg = msg_init_default;

    presence_msg.node_id = NODE_ID;
    presence_msg.which_payload = msg_type_alert_tag;
    presence_msg.payload.type_alert.type = ALERT_PRESENCE;
    presence_msg.payload.type_alert.value = 1; // presence detected 
    
    display_wake();
    message_send(&presence_msg);

    

    return status;
}

static uint8_t handle_presence_gone(void) {
    uint8_t status = STATUS_OK;
    msg presence_msg = msg_init_default;

    presence_msg.node_id = NODE_ID;
    presence_msg.which_payload = msg_type_alert_tag;
    presence_msg.payload.type_alert.type = ALERT_PRESENCE;
    presence_msg.payload.type_alert.value = 0; // presence gone

    display_sleep();
    message_send(&presence_msg);

    return status;
}


