#include "../../Inc/drivers/ili9341.h"
#include "../../Inc/drivers/spi.h"
#include "../../Inc/drivers/io.h"
#include <string.h>

/************
 * MACROS
 *************/
#define DISP_CS_LOW()         (io_set_out(IO_SPI_CS_DISP, IO_OUT_LOW))
#define DISP_CS_HIGH()        (io_set_out(IO_SPI_CS_DISP, IO_OUT_HIGH))
#define DISP_CMD_PIN()        (io_set_out(IO_DISP_CTL, IO_OUT_LOW))
#define DISP_DATA_PIN()       (io_set_out(IO_DISP_CTL, IO_OUT_HIGH))

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
 * @brief This functions performs the actual SPI transmit of pixels
 * 
 * This function is owned by the SPI task and only accessible
 * via a request to the SPI task. Designed to eliminate any 
 * sort of concurrency issues.
 */
uint8_t ili9341_spi_send_pixels(lv_display_t * disp, const uint8_t * cmd, size_t cmd_size, uint8_t * param, size_t param_size) {
    uint8_t status = 0;
    if (spi_lock(DEV_DISP) == 1) {
        
        DISP_CS_LOW();
        DISP_CMD_PIN();
        if (cmd_size > 0)
            status = spi_transmit(DEV_DISP, cmd, cmd_size);

        DISP_DATA_PIN();
        status |= spi_transmit(DEV_DISP, param, param_size);
        DISP_CS_HIGH();
        lv_display_flush_ready(disp);
        spi_unlock(DEV_DISP);
    }
    return status;
}


/**
 * @brief This functions performs the actual SPI transmit of CMDs
 * 
 * 
 * 
 */
uint8_t ili9341_spi_send_cmd(lv_display_t * disp, const uint8_t * cmd, size_t cmd_size, const uint8_t *param, size_t param_size) {
    uint8_t status = 0;
    if (spi_lock(DEV_DISP) == 1) {
        DISP_CS_LOW();
        DISP_CMD_PIN();

        status = spi_transmit(DEV_DISP, cmd, cmd_size);
        if (param_size > 0) {
            DISP_DATA_PIN();
            status |= spi_transmit(DEV_DISP, param, param_size);
        }

        DISP_CS_HIGH();
        spi_unlock(DEV_DISP);
    }
    return status;
}

/****************
 * STATIC DEFS
 *****************/
