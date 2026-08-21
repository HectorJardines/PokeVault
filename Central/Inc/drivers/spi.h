#ifndef _SPI_H
#define _SPI_H

#include <stdint.h>
#include "../../../FreeRTOS_WrkSpace/include/FreeRTOS.h"
#include "../../../FreeRTOS_WrkSpace/include/task.h"

/***********
 * MACROS
 ************/
#define SPI_OK_Msk  (1U << 6)
#define SPI_ERR_Msk (1U << 7)

/********
 * ENUMS
 *********/
typedef enum {
    DEV_MFRC,
    DEV_ETH,
    DEV_SD,
    DEV_DISP,
    DEV_TOUCH
} spi_dev_e;


typedef enum {
    SD_READ_BLOCKS,
    SD_WRITE_BLOCKS,
    ILI9341_SEND_CMD,
    ILI9341_SEND_PIXELS
} spi1_req_type_e;


typedef struct {
    spi1_req_type_e req_type;
    TaskHandle_t req_task;
    union {
        struct {uint8_t *buff; uint32_t sector; uint32_t count; } sd_io; 
        struct {const uint8_t *cmd; uint32_t cmd_size; const uint8_t *param; uint32_t param_size; } ili9341_io;
    };
} spi1_req_t;


typedef struct {
    
} spi2_req_t;
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



/**
 * @brief Sets the max SPI clock freq for the specified device
 * 
 * 
 * 
 */
void spi_set_freq(spi_dev_e dev);


/**
 * @brief Sleeps the thread until the lock is obtained for the device
 * 
 * 
 * 
 */
uint8_t spi_lock(spi_dev_e dev);

/**
 * @brief Sleeps the thread until the lock is obtained for the device
 * 
 * 
 * 
 */
uint8_t spi_unlock(spi_dev_e dev);


/**
 * @brief
 * 
 * 
 */
uint8_t spi1_post_request(spi1_req_t *req);



/**
 * @brief
 * 
 * 
 */
uint8_t spi1_wait_init(void);


/**
 * @brief
 * 
 * 
 */
uint8_t spi1_wait_notify(void);

#endif
