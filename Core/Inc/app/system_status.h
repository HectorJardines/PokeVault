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

/**********************
 * STRUCTS/ENUMS
 **********************/

typedef struct {
    uint8_t presence_detected;
    uint8_t unit_open;
    accel_info_t unit_movement;
    aht20_data_t temp_hum_readings;
} system_info_t;


/*********************
 * PUBLIC APIs
 *********************/

/**
 * @brief Initialize system monitoring sensors
 * 
 * 
 */
void system_monitor_init(void);



/**
 * @brief 
 */


#endif /* _SYS_STAT_H*/