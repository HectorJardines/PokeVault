#include "system_status.h"
#include "message.h"


#define EXCESS_TEMP_THRSH   (26) // in celcius
#define EXCESS_HUM_THRSH    (60) // percentage
#define TEMP    (0U)
#define HUM     (1U)

/************************
 * STATIC DECLARATIONS
 ************************/
static uint8_t handle_excess_temp_hum(uint32_t value, uint8_t temp_or_hum);
static uint8_t handle_presence_detect(void);

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
    sys_state->unit_opened = (sens_status.line_state == IR_LINE_CONNECTED && sys_state->unit_opened == 0);
    sys_state->unit_closed = (sens_status.line_state == IR_LINE_BROKEN && sys_state->unit_closed == 0);
    sys_state->presence = (sens_status.presence_state == PIR_PRESENCE && sys_state->presence == 0);
    sys_state->no_presence = (sens_status.presence_state == PIR_NO_PRESENCE && sys_state->no_presence == 0);


    status = aht20_read_data(&sys_state->temp_hum_readings);

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
    uint8_t status = system_retrieve_state(&active_sys_state);

    if (active_sys_state.temp_hum_readings.temp > EXCESS_TEMP_THRSH)
        status |= handle_excess_temp_hum(active_sys_state.temp_hum_readings.temp, TEMP);
    if (active_sys_state.temp_hum_readings.humidity > EXCESS_HUM_THRSH)
        status |= handle_excess_temp_hum(active_sys_state.temp_hum_readings.humidity, HUM);
    if (active_sys_state.presence)
        status |= handle_presence_detect();

    return status;
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
    status = message_send(&temp_hum_msg);

    return status;
}



static uint8_t handle_presence_detect(void) {
    uint8_t status = STATUS_OK;
    msg presence_msg = msg_init_default;

    presence_msg.node_id = NODE_ID;
    presence_msg.which_payload = msg_type_alert_tag;
    presence_msg.payload.type_event.type = ALERT_PRESENCE;
    
    status = message_send(&presence_msg);

    return status;
}


