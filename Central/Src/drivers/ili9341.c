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
    if (spi_lock(DEV_DISP) == 1) {
        DISP_CS_LOW();
        DISP_CMD_PIN();

        status = spi_transmit(DEV_DISP, cmd, cmd_size);
        if (param_size > 0) {
            DISP_DATA_PIN();
            status |= spi_transmit(DEV_DISP, param, param_size);
        }

        DISP_CS_HIGH();
        status = spi_unlock(DEV_DISP);
    }
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
    if (spi_lock(DEV_DISP) == 1) {
        DISP_CS_LOW();
        DISP_CMD_PIN();
        if (cmd_size > 0)
            status = spi_transmit(DEV_DISP, cmd, cmd_size);

        DISP_DATA_PIN();
        // memset((void *)param, 0, param_size);
        status |= spi_transmit_dma(DEV_DISP, param, param_size);
        spi_wait(DEV_DISP);

        DISP_CS_HIGH();
        lv_display_flush_ready(disp);
        spi_unlock(DEV_DISP);
    }
    (void)status;
}



/****************
 * STATIC DEFS
 *****************/
