/**
 * This module implements a simple AHT20 driver utilizing
 * the I2C peripheral. Implements configuration 
 */
#ifndef _AHT20_H
#define _AHT20_H

#include <stdint.h>

/******************
 * AHT20 MACROS
 ******************/

/********************
 * ENUMS/STRUCTS
 ********************/
typedef struct {
    uint8_t(*transmit)(uint8_t *data, uint32_t len); // I2C transmit function pointer
    uint8_t (*receive)(uint8_t *data, uint32_t len); // I2C receive function pointer
} aht20_t;

typedef struct {
    uint32_t temp;
    uint32_t humidity;
} aht20_data_t;

 /*******************
 *  USER APIs
 *******************/

/**
 * @brief Performs hte AHT20 power on sequence and confirms operation
 */
void aht20_init(void);

/**
 * @brief Retrieve sensor values for temperature and humidity
 * 
 * Retrieves the sensor data using the user provided I2C receive function.
 * 
 * @param data aht20_data_t pointer to which retrieved data will be stored
 * @return 0 on success; 1 else
 */
uint8_t aht20_read_data(aht20_data_t *data);

#endif