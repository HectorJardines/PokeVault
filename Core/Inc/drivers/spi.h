/**
 * This module implements the SPI peripheral configuration
 * and read/write APIs for the systems SPI devices. There are 
 * three such devices W5500 ethernet module, MFRC522 rfid reader module,
 * and BMI160 imu.
 */

#ifndef _SPI_H
#define _SPI_H

#include "./io.h"
#include "../../Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_spi.h"

/***********
 * ENUMS
 ***********/

typedef enum {
    SPI_DEVICE_W5500,
    SPI_DEVICE_MFRC522,
    SPI_DEVICE_BMI160
} spi_device_e;

/*****************
 * PUBLIC APIs
 *****************/

/**
 * @brief Initializes the SPI peripheral corresponding to the specified SPI device
 * 
 * The system utilizes three SPI devices: W5500 ethernet module, MFRC522 RFID reader, 
 * and BMI160 IMU. Some of these peripherals may share a single SPI bus, others may 
 * get their own dedicated bus. This API configures said SPI peripherals.
 * 
 * @param spi_dev specifies the device whose dedicated SPI peripheral will be configured
 */
void spi_init(spi_device_e spi_dev);

/**
 * @brief HAL_SPITransmit wrapper for mfrc522 transmission
 * 
 * @param data data buffer to be transmitted
 * @param len length of data buffer in bytes
 */
uint8_t spi_transmit_mfrc(uint8_t *data, uint32_t len);

/**
 * @brief HAL_SPIReceive wrapper for mfrc522 reception
 * 
 * @param data data buffer to store received bytes
 * @param len length of data buffer in bytes
 */
uint8_t spi_receive_mfrc(uint8_t *data, uint32_t len);

#endif