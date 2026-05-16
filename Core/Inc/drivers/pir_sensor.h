#ifndef _PIR_SENSOR_H
#define _PIR_SENSOR_H

/******************
 * ENUMS
 ***************/
typedef enum {
    PIR_OK,
    PIR_BSY,
    PIR_ERR
} pir_status_e;

typedef enum {
    SENSOR_UNIT1,
    SENSOR_UNIT2,
    SENSOR_UNIT3
} pir_sensor_e;

typedef enum {
    PRESNCE_NONE,
    PRESENCE_DETECETD
} pir_presence_e;

typedef struct {
    pir_presence_e presence_unit1;
    pir_presence_e presence_unit2;
    pir_presence_e presence_unit3;
} pir_sensors_t;


/*****************
 * PUBLIC APIs
 ****************/

/**
 * @brief Initializes the PIR sensor module
 * 
 * This module shares the ADC peripheral with the IR sensor module. 
 */
void pir_init(void);

/**
 * @brief Samples the PIR sensors and computes whether a presence is detected or not
 * 
 * This API retrieves samples from the ADC peripheral corresponding to the 
 * PIR sensors. It performs value checks on the samples, if they pass the threshold
 * for a presence detection, the corresponding field in the struct is set.
 * 
 * @param presence_sensors struct with a field for each units PIR sensor
 */
pir_status_e pir_check_sensors(pir_sensors_t *presence_sensors);

#endif