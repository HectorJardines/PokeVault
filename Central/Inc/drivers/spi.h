#ifndef _SPI_H
#define _SPI_H

#include <stdint.h>

/********
 * ENUMS
 *********/
typedef enum {
    DEV_MFRC,
    DEV_ETH,
    DEV_SD,
    DEV_DISP
} spi_dev_e;


/**********
 * PUB APIs
 ************/

/**
 * @brief Initialize and configure SPI peripheral
 * 
 * 
 */
void spi_init(void);



/**
 * @brief 
 */
uint8_t spi_transmit(spi_dev_e dev, uint8_t *data, uint32_t len);


/**
 * @brief 
 */
uint8_t spi_receive(spi_dev_e dev, uint8_t *read_data, uint32_t read_len);


/**
 * @brief
 */
uint8_t spi_transmit_dma(spi_dev_e dev, uint8_t *data, uint32_t len);

/**
 * @brief 
 */
uint8_t spi_receive_dma(spi_dev_e dev, uint8_t *read_data, uint32_t read_len);


#endif
