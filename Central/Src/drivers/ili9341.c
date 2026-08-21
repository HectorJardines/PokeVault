#include "../../Inc/drivers/ili9341.h"
#include "../../Inc/drivers/spi.h"
#include "../../Inc/drivers/io.h"
#include <string.h>

/************
 * MACROS
 *************/
#define DISP_CS_LOW()         (GPIOA->BSRR |= GPIO_BSRR_BR7)
#define DISP_CS_HIGH()        (GPIOA->BSRR |= GPIO_BSRR_BS7)
#define DISP_CMD_PIN()        (GPIOB->BSRR |= GPIO_BSRR_BR0)
#define DISP_DATA_PIN()       (GPIOB->BSRR |= GPIO_BSRR_BS0)

/*************
 * STATIC DECS
 **************/


/****************
 * PUB APIs
 ***************/

/**
 * @brief Reset and initialize the ILI9341 disp driver
 * 
 * 
 */
void ili9341_init(void) {
    spi_init();
}


/**
 * @brief Sends commands to the ILI9341 disp driver
 * 
 * 
 * 
 */
void ili9341_send_cmd(lv_display_t * disp, const uint8_t * cmd, size_t cmd_size, const uint8_t *param, size_t param_size) {
    uint8_t status = 0;
    // sleep thread until spi periph is free
    spi1_req_t ili_cmd_send = {.req_type = ILI9341_SEND_CMD,
                                .req_task = xTaskGetCurrentTaskHandle(), 
                                .ili9341_io = {.cmd = cmd, .cmd_size = cmd_size, 
                                                .param = param, .param_size = param_size}
                                };
    status = spi1_post_request(&ili_cmd_send);
    if (status == HAL_OK)
        status = spi1_wait_notify();
    (void)status;
}


/**
 * @brief Flushes a partial display buffer to the display
 * 
 * 
 * 
 */
void ili9341_send_pixels(lv_display_t * disp, const uint8_t * cmd, size_t cmd_size, uint8_t * param, size_t param_size) {
    uint8_t status = 0;
    spi1_req_t ili_pixels_send = {.req_type = ILI9341_SEND_PIXELS, 
                                .req_task = xTaskGetCurrentTaskHandle(),
                                .ili9341_io = {.cmd = cmd, .cmd_size = cmd_size, 
                                                .param = param, .param_size = param_size}
                                };
    status = spi1_post_request(&ili_pixels_send);
    if (status == HAL_OK)
        status = spi1_wait_notify();
    lv_display_flush_ready(disp);
    (void)status;
}


/**
 * @brief This functions performs the actual SPI transmit of pixels
 * 
 * This function is owned by the SPI task and only accessible
 * via a request to the SPI task. Designed to eliminate any 
 * sort of concurrency issues.
 */
uint8_t ili9341_spi_send_pixels(const uint8_t * cmd, size_t cmd_size, uint8_t * param, size_t param_size) {
    uint8_t status = 0;
    DISP_CS_LOW();
    DISP_CMD_PIN();
    if (cmd_size > 0)
        status = spi_transmit(DEV_DISP, cmd, cmd_size);

    DISP_DATA_PIN();
    status |= spi_transmit_dma(DEV_DISP, param, param_size);
    DISP_CS_HIGH();

    return status;
}


/**
 * @brief This functions performs the actual SPI transmit of CMDs
 * 
 * 
 * 
 */
uint8_t ili9341_spi_send_cmd(const uint8_t * cmd, size_t cmd_size, const uint8_t *param, size_t param_size) {
    uint8_t status = 0;
    DISP_CS_LOW();
    DISP_CMD_PIN();

    status = spi_transmit(DEV_DISP, cmd, cmd_size);
    if (param_size > 0) {
        DISP_DATA_PIN();
        status |= spi_transmit(DEV_DISP, param, param_size);
    }

    DISP_CS_HIGH();
    return status;
}

/****************
 * STATIC DEFS
 *****************/
