#ifndef SPI_H_
#define SPI_H_
 
#include <stdint.h>
 
/**
 * Logical SPI devices. The bus each device lives on, and its default
 * clock prescaler, are set in ONE place: spi_dev_cfg[] in spi.c.
 */
typedef enum {
    DEV_SD = 0,     /* SPI2 */
    DEV_FLASH,      /* SPI1 */
    DEV_ETH,        /* SPI4 */
    DEV_DISP,       /* SPI4 */
    DEV_TOUCH,      /* SPI4 */
    DEV_MFRC,       /* SPI4 */
    DEV_COUNT
} spi_dev_e;
 
/**
 * Call once, before the scheduler starts (or at least before any other
 * spi_* call). Safe to call more than once.
 */
void spi_init(void);
 
/**
 * Take / release the bus the device lives on. The mutex is RECURSIVE, so a
 * driver can hold the bus across several transfers (e.g. CS asserted for a
 * whole SD command) while each spi_* call below still locks internally.
 * Return HAL_OK / HAL_ERROR.
 */
uint8_t spi_lock(spi_dev_e dev);
uint8_t spi_unlock(spi_dev_e dev);
 
/**
 * Change a device's clock prescaler (SPI_BAUDRATEPRESCALER_x) at runtime.
 * Example: SD init at /256, then spi_set_dev_speed(DEV_SD, SPI_BAUDRATEPRESCALER_2).
 */
uint8_t spi_set_dev_speed(spi_dev_e dev, uint32_t prescaler);
 
/* ---- Polling transfers: lock, transfer, unlock. Block the calling task. ---- */
uint8_t spi_transmit(spi_dev_e dev, const uint8_t *data, uint32_t len);
uint8_t spi_receive(spi_dev_e dev, uint8_t *data, uint32_t len);
uint8_t spi_transfer(spi_dev_e dev, const uint8_t *tx, uint8_t *rx, uint32_t len);
 
/* ---- DMA, blocking: task sleeps until the DMA ISR notifies it. ---- */
uint8_t spi_transmit_dma(spi_dev_e dev, const uint8_t *data, uint32_t len);
uint8_t spi_receive_dma(spi_dev_e dev, uint8_t *data, uint32_t len);
 
/* ---- DMA, split: start, do other work, then wait.
 *
 * start() locks the bus and KEEPS it locked; the matching spi_dma_wait()
 * (same task) sleeps until the ISR notification, then unlocks. Returns
 * HAL_OK / HAL_ERROR / HAL_TIMEOUT.
 * ---- */
uint8_t spi_transmit_dma_start(spi_dev_e dev, const uint8_t *data, uint32_t len);
uint8_t spi_receive_dma_start(spi_dev_e dev, uint8_t *data, uint32_t len);
uint8_t spi_dma_wait(spi_dev_e dev);
 
#endif /* SPI_H_ */

// #ifndef _SPI_H
// #define _SPI_H

// #include <stdint.h>
// #include "../../../FreeRTOS_WrkSpace/include/FreeRTOS.h"
// #include "../../../FreeRTOS_WrkSpace/include/task.h"

// /***********
//  * MACROS
//  ************/
// #define SPI_OK_Msk  (1U << 6)
// #define SPI_ERR_Msk (1U << 7)

// /********
//  * ENUMS
//  *********/
// typedef enum {
//     DEV_MFRC,
//     DEV_ETH,
//     DEV_SD,
//     DEV_DISP,
//     DEV_TOUCH,
//     DEV_SD_INIT,
//     DEV_FLASH
// } spi_dev_e;


// typedef enum {
//     SD_READ_BLOCKS,
//     SD_WRITE_BLOCKS,
//     ILI9341_SEND_CMD,
//     ILI9341_SEND_PIXELS
// } spi1_req_type_e;


// typedef struct {
//     spi1_req_type_e req_type;
//     TaskHandle_t req_task;
//     union {
//         struct {uint8_t *buff; uint32_t sector; uint32_t count; } sd_io; 
//         struct {const uint8_t *cmd; uint32_t cmd_size; const uint8_t *param; uint32_t param_size; } ili9341_io;
//     };
// } spi1_req_t;


// typedef struct {
    
// } spi2_req_t;
// /**********
//  * PUB APIs
//  ************/

// /**
//  * @brief Initialize and configure SPI peripheral
//  * 
//  * 
//  */
// void spi_init(void);


// /**
//  * @brief Blocking transmit API
//  * 
//  *
//  * 
//  */
// uint8_t spi_transmit(spi_dev_e dev, uint8_t *data, uint32_t len);


// /**
//  * @brief Blocking receive API
//  * 
//  *  
//  */
// uint8_t spi_receive(spi_dev_e dev, uint8_t *read_data, uint32_t read_len);


// /**
//  * @brief Non-blocking, DMA-based transmit API
//  * 
//  * 
//  */
// uint8_t spi_transmit_dma(spi_dev_e dev, uint8_t *data, uint32_t len);

// /**
//  * @brief Non-blocking, DMA-based receive API
//  * 
//  *  
//  */
// uint8_t spi_receive_dma(spi_dev_e dev, uint8_t *read_data, uint32_t read_len);



// /**
//  * @brief Requests SPI bus lock
//  * 
//  * 
//  * 
//  * @note This function need only be called
//  * when accessing SPI2 bus
//  */
// uint8_t spi_lock(spi_dev_e dev);

// /**
//  * @brief Release the SPI bus lock
//  */
// uint8_t spi_unlock(spi_dev_e dev);


// /**
//  * @brief Request SPI1 bus operation
//  * 
//  * Calling thread requests a SPI1 operation 
//  * to be performed by the SPI1 actor task. The calling 
//  * thread blocks until the request is completed.
//  * 
//  */
// uint8_t spi1_post_request(spi1_req_t *req);


// /**
//  * @brief Sleep the calling task until SPI1 operation complete
//  * 
//  * This function should be called after a call to
//  * spi1_post_request. This function sleeps the thread
//  * until the previous request is completed.
//  * 
//  */
// uint8_t spi1_wait_notify(uint8_t dev);


// /**
//  * @brief Calling task blocks until the SPI1 bus has been initialized
//  */
// uint8_t spi1_wait_init(void);




// #endif
