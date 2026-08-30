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
    DEV_TOUCH,
    DEV_SD_INIT
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
 * @brief Blocking transmit API
 * 
 *
 * 
 */
uint8_t spi_transmit(spi_dev_e dev, uint8_t *data, uint32_t len);


/**
 * @brief Blocking receive API
 * 
 *  
 */
uint8_t spi_receive(spi_dev_e dev, uint8_t *read_data, uint32_t read_len);


/**
 * @brief Non-blocking, DMA-based transmit API
 * 
 * 
 */
uint8_t spi_transmit_dma(spi_dev_e dev, uint8_t *data, uint32_t len);

/**
 * @brief Non-blocking, DMA-based receive API
 * 
 *  
 */
uint8_t spi_receive_dma(spi_dev_e dev, uint8_t *read_data, uint32_t read_len);



/**
 * @brief Requests SPI bus lock
 * 
 * 
 * 
 * @note This function need only be called
 * when accessing SPI2 bus
 */
uint8_t spi_lock(spi_dev_e dev);

/**
 * @brief Release the SPI bus lock
 */
uint8_t spi_unlock(spi_dev_e dev);


/**
 * @brief Request SPI1 bus operation
 * 
 * Calling thread requests a SPI1 operation 
 * to be performed by the SPI1 actor task. The calling 
 * thread blocks until the request is completed.
 * 
 */
uint8_t spi1_post_request(spi1_req_t *req);


/**
 * @brief Sleep the calling task until SPI1 operation complete
 * 
 * This function should be called after a call to
 * spi1_post_request. This function sleeps the thread
 * until the previous request is completed.
 * 
 */
uint8_t spi1_wait_notify(uint8_t dev);


/**
 * @brief Calling task blocks until the SPI1 bus has been initialized
 */
uint8_t spi1_wait_init(void);




#endif
