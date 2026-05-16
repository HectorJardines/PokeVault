/**
 * This module implements a simple AHT20 driver utilizing
 * the I2C peripheral. Going to need an I2C multiplexer 
 * or going to have to use more pins since AHT20 address is 
 * fixed...
 */
#ifndef _AHT20_H
#define _AHT20_H

#include <stdint.h>

/******************
 * AHT20 MACROS
 ******************/
#define AHT20_DEV_ADDR (0x38U)

#define AHT20_CMD_INIT      (0xBE)
#define AHT20_INIT_BYTE1    (0x08)
#define AHT20_INIT_BYTE2    (0x00)

#define AHT20_CMD_STRT_MEAS (0xAC)
#define AHT20_MEAS_BYTE1    (0x33)
#define AHT20_MEAS_BYTE2    (0x00)

#define AHT20_GET_STATUS    (0x71)

#define AHT20_CMD_RESET     (0xBA)

#define AHT20_BUSY_Msk      (0x80)
#define AHT20_CALIBRATION_Msk (0x08)
/********************
 * ENUMS/STRUCTS
 ********************/
typedef struct {
    uint8_t(*transmit)(uint8_t dev_addr, uint8_t *data, uint32_t len); // I2C transmit function pointer
    uint8_t (*receive)(uint8_t dev_addr, uint8_t *data, uint32_t len); // I2C receive function pointer
} aht20_t;

typedef struct {
    uint32_t temp;
    uint32_t humidity;
} aht20_data_t;

typedef struct {
    aht20_data_t UNIT0_DATA;
    aht20_data_t UNIT1_DATA;
    aht20_data_t UNIT2_DATA;
} aht20_sensor_measurements_t;

// three storage units, each with their own temp sensor
typedef enum {
    AHT20_UNIT0,
    AHT20_UNIT1,
    AHT20_UNIT2,
    AHT20_ALL_UNITS
} aht20_sensor_e;

 /*******************
 *  USER APIs
 *******************/

/**
 * @brief Performs hte AHT20 power on sequence and confirms operation
 */
uint8_t aht20_init(void);

/**
 * @brief Retrieve sensor values for temperature and humidity
 * 
 * Retrieves the sensor data using the user provided I2C receive function.
 * 
 * @param data aht20_data_t pointer to which retrieved data will be stored
 * @return 0 on success; 1 else
 */
uint8_t aht20_read_data(aht20_sensor_e sensor, aht20_sensor_measurements_t *data);

#endif