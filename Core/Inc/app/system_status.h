/**
 * @author Hector Jardines 
 * 
 * This module monitors the systems sensors, e.g.
 * PIR sensor, IR sensor, accelerometer, temp/hum sensor.
 * Provides an interface to easily retrieve the overall status
 * of the system.
 * 
 */

#ifndef _SYS_STAT_H
#define _SYS_STAT_H

#include <stdint.h>
#include "movement_detect.h"
#include "ir_sensors.h"
#include "aht20.h"

/**********************
 * STRUCTS/ENUMS
 **********************/

typedef struct {
    uint8_t unit_opened;
    uint8_t unit_closed;
    uint8_t presence;
    uint8_t no_presence;
    movement_state_t unit_movement;
    aht20_data_t temp_hum_readings;
} system_info_t;


/*********************
 * PUBLIC APIs
 *********************/

/**
 * @brief Initialize system monitoring sensors
 * 
 * 
 * Initializes the required sensor submodules.
 * 
 */
uint8_t system_monitor_init(void);



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
uint8_t system_retrieve_state(system_info_t *sys_state);



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
uint8_t system_process_state(void);

#endif /* _SYS_STAT_H*/