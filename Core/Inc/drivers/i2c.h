/**
 * This modules implements I2C peripheral configurations for 
 * SSD11306 oled display and AHT20 temp/humidty sensor drivers.
 */
#ifndef _I2C_H
#define _I2C_H

#include "./io.h"
#include "Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_i2c.h"

/***************
 *   ENUMS
 ***************/
typedef enum {
    I2C_DEVICE_SSD1306,
    I2C_DEVICE_AHT20
}i2c_device_e;

/***************
 * PUBLIC APIs
 ***************/

/**
 * @brief Initializes the I2C peripheral corresponding to the specified device
 * 
 * The system utilizes two I2C devices, SSD1306 oled display driver and AHT20 
 * humidity/temp sensor. Calls to this API specify which device the I2C peripheral
 * is being configured for.
 * 
 * @param i2c_dev the device for which the I2C peripheral is to be configured
 */
void i2c_init(i2c_device_e i2c_dev);

/**
 * @brief Wrapper for HAL i2c transmit, blocking call
 * 
 * @param dev_addr device I2C address
 * @param send_data data buffer to send
 * @param data_len length of data buffer
 */
uint8_t i2c_transmit(uint8_t dev_addr, uint8_t *send_data, uint32_t data_len);

/**
 * @brief Wrapper for HAL i2c receive, blocking call
 * 
 * @param dev_addr device I2C address
 * @param send_data data buffer to receive bytes into
 * @param data_len length of data buffer
 */
uint8_t i2c_receive(uint8_t dev_addr, uint8_t *rcv_data, uint32_t data_len);

#endif