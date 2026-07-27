#include "stm32f4xx.h"
#include "../../Inc/drivers/spi.h"

typedef struct {
    SPI_HandleTypeDef hspi;
    DMA_HandleTypeDef hdmatx;
    DMA_HandleTypeDef hdmarx;
    uint8_t curr_dev;
} spi_conf_t;

/*****************
 * STATIC DECS
 ******************/
static void spi1_configure(void);
static void spi2_configure(void);


static spi_conf_t spi_eth_disp;
static spi_conf_t spi_tag_sd;
/*************
 * PUB APIs
 **************/

static uint8_t initialized = 0; 
/**
 * @brief Initialize and configure SPI peripheral
 * 
 * 
 */
void spi_init(void) {
    spi1_configure();
    spi2_configure();
    initialized = 1;
}

/**
 * @brief 
 */
uint8_t spi_transmit(spi_dev_e dev, uint8_t *data, uint32_t len) {
    uint16_t retry = 500;
    SPI_HandleTypeDef *spix;
    if (dev == DEV_ETH || dev == DEV_DISP) {
        spix = &spi_eth_disp.hspi;
        spi_eth_disp.curr_dev = dev;
    }
    else {
        spix = &spi_tag_sd.hspi;
        spi_tag_sd.curr_dev = dev;
    }

    for (uint32_t i = 0; i < len; ++i) {
        while (!__HAL_SPI_GET_FLAG(spix, SPI_SR_TXE) && --retry);
        if (retry == 0) return HAL_ERROR;
        spix->Instance->DR = *data++;
        retry = 500;
    }

    return HAL_OK;
}


/**
 * @brief 
 */
uint8_t spi_receive(spi_dev_e dev, uint8_t *read_data, uint32_t read_len) {
    uint8_t dummy = 0xFF;
    SPI_HandleTypeDef *spix;
    uint16_t retry = 500;
    if (dev == DEV_ETH || dev == DEV_DISP) {
        spix = &spi_eth_disp.hspi;
        spi_eth_disp.curr_dev = dev;
    }
    else {
        spix = &spi_tag_sd.hspi;
        spi_tag_sd.curr_dev = dev;
    }

    for (uint32_t i = 0; i < read_len; ++i) {
        while (!__HAL_SPI_GET_FLAG(spix, (SPI_SR_TXE | SPI_SR_RXNE)) && --retry);
        if (retry == 0) return HAL_ERROR;
        spix->Instance->DR = dummy;
        retry = 500;
        
        while (!__HAL_SPI_GET_FLAG(spix, (SPI_SR_TXE | SPI_SR_RXNE)) && --retry);
        if (retry == 0) return HAL_ERROR;
        *read_data++ = *((uint8_t *)spix->Instance->DR);
        retry = 500;
    }

    return HAL_OK;
}


/**
 * @brief
 */
uint8_t spi_transmit_dma(spi_dev_e dev, uint8_t *data, uint32_t len) {
    uint8_t status = HAL_OK;
    SPI_HandleTypeDef *spix;
    if (dev == DEV_ETH || dev == DEV_DISP) {
        spix = &spi_eth_disp.hspi;
        spi_eth_disp.curr_dev = dev;
    }
    else {
        spix = &spi_tag_sd.hspi;
        spi_tag_sd.curr_dev = dev;
    }

    status = HAL_SPI_Transmit_DMA(spix, data, len);
    return status;
}

/**
 * @brief 
 */
uint8_t spi_receive_dma(spi_dev_e dev, uint8_t *read_data, uint32_t read_len) {
    uint8_t status = HAL_OK;
    SPI_HandleTypeDef *spix;
    if (dev == DEV_ETH || dev == DEV_DISP) {
        spix = &spi_eth_disp.hspi;
        spi_eth_disp.curr_dev = dev;
    }
    else {
        spix = &spi_tag_sd.hspi;
        spi_tag_sd.curr_dev = dev;
    }

    status = HAL_SPI_Receive_DMA(spix, read_data, read_len);
    return status;
}


/*****************
 * STATIC DEFS
 *****************/

/**
 * @brief Configures the SPI peripheral used by ETH/DISP
 */
static void spi1_configure(void) {
    /************* CONFIGURE SPI TX DMA *******************/
    spi_eth_disp.hdmatx.Instance = DMA1_Stream2;
    spi_eth_disp.hdmatx.Init.Channel = DMA_CHANNEL_2;
    spi_eth_disp.hdmatx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    spi_eth_disp.hdmatx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    spi_eth_disp.hdmatx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    spi_eth_disp.hdmatx.Init.MemInc = DMA_MINC_ENABLE;
    spi_eth_disp.hdmatx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    spi_eth_disp.hdmatx.Init.PeriphInc = DMA_PINC_ENABLE;
    spi_eth_disp.hdmatx.Init.Mode = DMA_NORMAL;

    /************* CONFIGURE SPI RX DMA *******************/
    spi_eth_disp.hdmatx.Instance = DMA1_Stream0;
    spi_eth_disp.hdmatx.Init.Channel = DMA_CHANNEL_3;
    spi_eth_disp.hdmatx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    spi_eth_disp.hdmatx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    spi_eth_disp.hdmatx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    spi_eth_disp.hdmatx.Init.MemInc = DMA_MINC_ENABLE;
    spi_eth_disp.hdmatx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    spi_eth_disp.hdmatx.Init.PeriphInc = DMA_PINC_ENABLE;
    spi_eth_disp.hdmatx.Init.Mode = DMA_NORMAL;

    /************** CONFIGURE SPI PERIPH ***************/
    spi_eth_disp.hspi.Instance = SPI1;
    spi_eth_disp.hspi.Init.CLKPolarity = SPI_POLARITY_LOW;
    spi_eth_disp.hspi.Init.CLKPhase = SPI_PHASE_1EDGE;
    spi_eth_disp.hspi.Init.Mode = SPI_MODE_MASTER;
    spi_eth_disp.hspi.Init.DataSize = SPI_DATASIZE_8BIT;
    spi_eth_disp.hspi.Init.FirstBit = SPI_FIRSTBIT_MSB;
    spi_eth_disp.hspi.Init.NSS = SPI_NSS_SOFT;
    spi_eth_disp.hspi.Init.TIMode = SPI_TIMODE_DISABLE;
    spi_eth_disp.hspi.Init.Direction = SPI_DIRECTION_2LINES;
    spi_eth_disp.hspi.Init.CRCPolynomial = SPI_CRCCALCULATION_DISABLE;
    spi_eth_disp.hspi.Init.CRCPolynomial = SPI_CRCCALCULATION_DISABLE;
    spi_eth_disp.hspi.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;

    HAL_DMA_Init(&spi_eth_disp.hdmatx);
    HAL_DMA_Init(&spi_eth_disp.hdmarx);
    HAL_SPI_Init(&spi_eth_disp.hspi);

    __HAL_LINKDMA(&spi_eth_disp.hspi, hdmatx, spi_eth_disp.hdmatx);
    __HAL_LINKDMA(&spi_eth_disp.hspi, hdmatx, spi_eth_disp.hdmarx);
}



/**
 * @brief Config SPI periph used by MFRC/SD reader
 */
static void spi2_configure(void) {
    /************* CONFIGURE SPI TX DMA *******************/
    spi_eth_disp.hdmatx.Instance = DMA2_Stream4;
    spi_eth_disp.hdmatx.Init.Channel = DMA_CHANNEL_0;
    spi_eth_disp.hdmatx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    spi_eth_disp.hdmatx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    spi_eth_disp.hdmatx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    spi_eth_disp.hdmatx.Init.MemInc = DMA_MINC_ENABLE;
    spi_eth_disp.hdmatx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    spi_eth_disp.hdmatx.Init.PeriphInc = DMA_PINC_ENABLE;
    spi_eth_disp.hdmatx.Init.Mode = DMA_NORMAL;

    /************* CONFIGURE SPI RX DMA *******************/
    spi_eth_disp.hdmatx.Instance = DMA2_Stream3;
    spi_eth_disp.hdmatx.Init.Channel = DMA_CHANNEL_0;
    spi_eth_disp.hdmatx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    spi_eth_disp.hdmatx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    spi_eth_disp.hdmatx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    spi_eth_disp.hdmatx.Init.MemInc = DMA_MINC_ENABLE;
    spi_eth_disp.hdmatx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    spi_eth_disp.hdmatx.Init.PeriphInc = DMA_PINC_ENABLE;
    spi_eth_disp.hdmatx.Init.Mode = DMA_NORMAL;

    /************** CONFIGURE SPI PERIPH ***************/
    spi_eth_disp.hspi.Instance = SPI2;
    spi_eth_disp.hspi.Init.CLKPolarity = SPI_POLARITY_LOW;
    spi_eth_disp.hspi.Init.CLKPhase = SPI_PHASE_1EDGE;
    spi_eth_disp.hspi.Init.Mode = SPI_MODE_MASTER;
    spi_eth_disp.hspi.Init.DataSize = SPI_DATASIZE_8BIT;
    spi_eth_disp.hspi.Init.FirstBit = SPI_FIRSTBIT_MSB;
    spi_eth_disp.hspi.Init.NSS = SPI_NSS_SOFT;
    spi_eth_disp.hspi.Init.TIMode = SPI_TIMODE_DISABLE;
    spi_eth_disp.hspi.Init.Direction = SPI_DIRECTION_2LINES;
    spi_eth_disp.hspi.Init.CRCPolynomial = SPI_CRCCALCULATION_DISABLE;
    spi_eth_disp.hspi.Init.CRCPolynomial = SPI_CRCCALCULATION_DISABLE;
    spi_eth_disp.hspi.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;

    HAL_DMA_Init(&spi_eth_disp.hdmatx);
    HAL_DMA_Init(&spi_eth_disp.hdmarx);
    HAL_SPI_Init(&spi_eth_disp.hspi);

    __HAL_LINKDMA(&spi_eth_disp.hspi, hdmatx, spi_eth_disp.hdmatx);
    __HAL_LINKDMA(&spi_eth_disp.hspi, hdmatx, spi_eth_disp.hdmarx);

}


/**
 * @brief 
 */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (hspi->Instance == SPI1) {
        if (spi_eth_disp.curr_dev == DEV_ETH) {
            // handle whatever ETH stufff we need to do
        }
        else {
            // signal flush complete to display
        }
    }
    else if (hspi->Instance == SPI2) {
        if (spi_tag_sd.curr_dev == DEV_SD) {

        }
    }
}


/**
 * @brief 
 */
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (hspi->Instance == SPI1) {
        if (spi_eth_disp.curr_dev == DEV_ETH) {
            // signal eth controller dk what we receiving rn tbh...
        }
        else {
            // we shouldn't need to receive anything from display
        }
    }
    else if (hspi->Instance == SPI2) {
        if (spi_tag_sd.curr_dev == DEV_SD) {

        }
    }
}


/**
 * @brief Handler for SPI1 TX (ethernet/display)
 */
void DMA1_Stream2_IRQHandler(void) {
    HAL_DMA_IRQHandler(&spi_eth_disp.hdmatx);
}


/**
 * @brief Handler for SPI1 RX
 */
void DMA1_Stream0_IRQHandler(void) {
    HAL_DMA_IRQHandler(&spi_eth_disp.hdmarx);
}

/**
 * @brief Handler for SPI2 TX (tag/sd card)
 */
void DMA2_Stream0_IRQHandler(void) {
    HAL_DMA_IRQHandler(&spi_tag_sd.hdmatx);
}


/**
 * @brief Handler for SPI2 RX
 */
void DMA2_Stream0_IRQHandler(void) {
    HAL_DMA_IRQHandler(&spi_tag_sd.hdmatx);
}
