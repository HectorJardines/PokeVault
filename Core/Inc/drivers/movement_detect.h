/**
 * @author Hector Jardines
 * 
 * This module handles initialization and configuration of 
 * BMI160 IMU for remote storage unit motion detection. Configured
 * to detect any motion and no motion. Used to generate notifications
 * for when the unit has moved in the case that the security mechanisms
 * have not yet been disarmed.
 * 
 */
#include "common/defines.h"
#include "../../../Drivers/bmi160/bmi160.h"

#ifndef _MOVEMENT_H
#define _MOVEMENT_H

/*****************
 * ENUMS/STRUCTS
 *****************/
typedef struct {
    uint8_t motion_detected;
    uint8_t tap_detected;
} movement_state_t;


typedef struct {
    struct bmi160_dev conf;
    struct bmi160_int_settg anym;
    struct bmi160_int_settg dtap;
} imu_handle_t;


/******************
 * PUBLIC APIs
 ******************/
/**
 * @brief Initialize the BMI160 IMU peripheral
 * 
 * 
 * @return 0 on successful initialization; else 1
 */
uint8_t movement_init(void);



/**
 * @brief Checks whether movement of the storage unit has been detected
 * 
 * 
 * 
 * @return 1 on movement detected; 0 else
 */
uint8_t movement_detected(void);


/**
 * @brief Checks whether movement of storage unit has stopped
 * 
 * 
 * 
 * @return 1 on no movement deteceted; 0 else
 */
uint8_t movement_stopped(void);



/**
 * @brief Checks if tap on the storage unit has been detected
 * 
 * 
 * @return 1 on tap detected; 0 else
 */
uint8_t movement_tap_detected(void);


void clear_movement(void);


#endif /* _MOVEMENT_H */