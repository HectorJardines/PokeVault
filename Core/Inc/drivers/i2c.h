/**
 * This modules implements I2C peripheral configurations for 
 * SSD11306 oled display and AHT20 temp/humidty sensor drivers.
 */
#ifndef _I2C_H
#define _I2C_H

#include "./io.h"
#include "../../Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_i2c.h"

/***************
 *   ENUMS
 ***************/
typedef enum {
    I2C_DEVICE_SSD1306,
    I2C_DEVICE_AHT20
}i2c_device_e;

typedef enum {
    I2C_OK,
    I2C_BUSY_IN_TX,
    I2C_BUSY_IN_RX,
    I2C_ERR
} i2c_status_e;

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

/**
 * @brief Non-blocking DMA transmit wrapper
 * 
 * @note Should likely change this to utilize double buffering
 * 
 * @param dev_addr device I2C address
 * @param send_data data buffer to receive bytes into
 * @param data_len length of data buffer
 */
uint8_t i2c_transmit_dma(uint8_t dev_addr, uint8_t *data, uint16_t data_len);

/**
 * @brief registers the i2c tx complete cb used with DMA tx
 * 
 * This API will be used to register the cb function that will call
 * the LVGL flush ready API to notify LVGL that the transmission of 
 * display buffer has finished
 * 
 * @param tx_cmplt_cb pointer to callback function
 */
void i2c_set_dma_tx_cplt_cb(void(*tx_cmplt_cb)(DMA_HandleTypeDef * hdma));

#endif