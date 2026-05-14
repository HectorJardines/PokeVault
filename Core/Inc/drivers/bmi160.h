/**
 * This module implements a basic bmi160 IMU driver based utilizing
 * the SPI peripheral. Includes configuration, self-testing, and 
 * data retrieval APIs that will be used in the system.
 */
#ifndef _BMI160_H
#define _BMI160_H

#include <stdint.h>

/*************************
 * REGISTER DEFINITIONS
 *************************/


/*************************
 *     ENUMS/STRUCTS
 *************************/
typedef struct {
    uint8_t placeholder;
}bmi160_dev_t;


/*************************
 *      USER APIs
 *************************/
uint8_t bmi160_init(bmi160_dev_t *dev);

#endif