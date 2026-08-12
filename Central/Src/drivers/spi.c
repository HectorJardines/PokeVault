#include "../../../FreeRTOS_WrkSpace/include/FreeRTOS.h"
#include "../../../FreeRTOS_WrkSpace/include/semphr.h"
#include "stm32f4xx.h"
#include "../../Inc/drivers/spi.h"

/**************
 * MACROS
 **************/
#define SPI_TX_GET_IRQn(spix)   ((spix)->Instance == SPI1 ? DMA2_Stream2_IRQn : DMA1_Stream4_IRQn)
#define SPI_RX_GET_IRQn(spix)   ((spix)->Instance == SPI1 ? DMA2_Stream0_IRQn : DMA1_Stream3_IRQn)

typedef struct {
    uint8_t curr_dev;
    SPI_HandleTypeDef hspi;
    DMA_HandleTypeDef hdmatx;
    DMA_HandleTypeDef hdmarx;

    BaseType_t hpt_trigger;
    SemaphoreHandle_t mutx;
    StaticSemaphore_t _mutx;
    TaskHandle_t curr_task;
} spi_conf_t;

/*****************
 * STATIC DECS
 ******************/
static void spi1_configure(void);
static void spi2_configure(void);


static spi_conf_t spi1;
static spi_conf_t spi2;
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
    if (!initialized) {
        RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
        RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
        RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
        RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;

        spi1_configure();
        spi2_configure();

        spi1.mutx = xSemaphoreCreateMutexStatic(&spi1._mutx);
        spi2.mutx = xSemaphoreCreateMutexStatic(&spi2._mutx);
        initialized = 1;
    }
}


void spi_set_freq(spi_dev_e dev) {
    if (dev == DEV_DISP || dev == DEV_SD) {
        while (__HAL_SPI_GET_FLAG(&spi1.hspi, SPI_SR_BSY));
        spi1.hspi.Instance->CR1 &= ~(SPI_CR1_SPE);
        spi1.hspi.Instance->CR1 &= ~(SPI_BAUDRATEPRESCALER_256); // clear current BR
        spi1.hspi.Instance->CR1 |= (SPI_BAUDRATEPRESCALER_4);
        spi1.hspi.Instance->CR1 |= (SPI_CR1_SPE);
    }
    else {
        while (__HAL_SPI_GET_FLAG(&spi2.hspi, SPI_SR_BSY));
        spi2.hspi.Instance->CR1 &= ~(SPI_CR1_SPE);
        spi2.hspi.Instance->CR1 &= ~(SPI_BAUDRATEPRESCALER_256); // clear current BR
        spi2.hspi.Instance->CR1 |= (SPI_BAUDRATEPRESCALER_2);
        spi2.hspi.Instance->CR1 |= (SPI_CR1_SPE);
    }
}


/**
 * @brief 
 */
uint8_t spi_transmit(spi_dev_e dev, uint8_t *data, uint32_t len) {
    uint16_t retry = 500;
    SPI_HandleTypeDef *spix;
    if (dev == DEV_DISP || dev == DEV_SD) {
        spix = &spi1.hspi;
        spi1.curr_dev = dev;
    }
    else {
        spix = &spi2.hspi;
        spi2.curr_dev = dev;
    }

    if (!(spix->Instance->CR1 & SPI_CR1_SPE))
        __HAL_SPI_ENABLE(spix);

    // for (uint32_t i = 0; i < len; ++i) {
    //     while (!__HAL_SPI_GET_FLAG(spix, (SPI_SR_TXE)) && --retry);
    //     if (retry == 0) return HAL_ERROR;
    //     spix->Instance->DR = *data++;
    //     retry = 500;
    // }
    HAL_SPI_Transmit(spix, data, len, 500);

    return HAL_OK;
}


/**
 * @brief 
 */
uint8_t spi_receive(spi_dev_e dev, uint8_t *read_data, uint32_t read_len) {
    uint8_t dummy = 0x00;
    SPI_HandleTypeDef *spix;
    uint16_t retry = 500;
    if (dev == DEV_DISP || dev == DEV_SD) {
        spix = &spi1.hspi;
        spi1.curr_dev = dev;
    }
    else {
        spix = &spi2.hspi;
        spi2.curr_dev = dev;
    }

    if (!(spix->Instance->CR1 & SPI_CR1_SPE))
        __HAL_SPI_ENABLE(spix);

    // for (uint32_t i = 0; i < read_len; ++i) {
    //     while (!__HAL_SPI_GET_FLAG(spix, (SPI_FLAG_TXE)) && --retry);
    //     if (retry == 0) return HAL_ERROR;
    //     spix->Instance->DR = dummy;
    //     retry = 500;
        
    //     while (!__HAL_SPI_GET_FLAG(spix, (SPI_FLAG_RXNE)) && --retry);
    //     if (retry == 0) return HAL_ERROR;
    //     *read_data++ = (uint8_t)spix->Instance->DR;
    //     retry = 500;
    // }
    HAL_SPI_Receive(spix, read_data, read_len, 500);

    return HAL_OK;
}


/**
 * @brief Thread-safe spi transmit dma implementation
 * 
 * This function assumes that the SPI bus corresponding
 * to the SPI device has been locked before calling...
 * 
 */
uint8_t spi_transmit_dma(spi_dev_e dev, uint8_t *data, uint32_t len) {
    uint8_t status = HAL_OK;
    SPI_HandleTypeDef *spix;
    if (dev == DEV_DISP || dev == DEV_SD) {
        spix = &spi1.hspi;
        spi1.curr_dev = dev;
        spi1.curr_task = xTaskGetCurrentTaskHandle();
    }
    else {
        spix = &spi2.hspi;
        spi2.curr_dev = dev;
        spi2.curr_task = xTaskGetCurrentTaskHandle();
    }

    status = HAL_SPI_Transmit_DMA(spix, data, len);
    // ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
    // NVIC_DisableIRQ(SPI_RX_GET_IRQn(spix));
    return status;
}

/**
 * @brief Thread-safe spi receive dma implementation
 */
uint8_t spi_receive_dma(spi_dev_e dev, uint8_t *read_data, uint32_t read_len) {
    uint8_t status = HAL_OK;
    SPI_HandleTypeDef *spix;
    if (dev == DEV_DISP || dev == DEV_SD) {
        spix = &spi1.hspi;
        spi1.curr_dev = dev;
        spi1.curr_task = xTaskGetCurrentTaskHandle();
    }
    else {
        spix = &spi2.hspi;
        spi2.curr_dev = dev;
        spi2.curr_task = xTaskGetCurrentTaskHandle();
    }

    status = HAL_SPI_Receive_DMA(spix, read_data, read_len);
    // ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
    return status;
}


/**
 * @brief Sleeps the thread until the lock is obtained for the device
 * 
 * 
 * 
 */
uint8_t spi_lock(spi_dev_e dev) {
    BaseType_t lock_obtained = pdFALSE;
    
    if (dev == DEV_DISP || dev == DEV_SD) {
        if (__HAL_SPI_GET_FLAG(&spi1.hspi, SPI_SR_OVR)) {
            __HAL_SPI_CLEAR_OVRFLAG(&spi1.hspi);
        }
        lock_obtained = xSemaphoreTake(spi1.mutx, portMAX_DELAY);
    }
    else
        lock_obtained = xSemaphoreTake(spi2.mutx, portMAX_DELAY);
    
    return lock_obtained;
}


/**
 * @brief Release the lock associated with the SPI device
 * 
 * 
 * 
 */
uint8_t spi_unlock(spi_dev_e dev) {
    BaseType_t lock_release = pdFALSE;
    if (dev == DEV_DISP || dev == DEV_SD)
        lock_release = xSemaphoreGive(spi1.mutx);
    else
        lock_release = xSemaphoreGive(spi2.mutx);
    
    return lock_release;
}


/**
 * @brief Sleeps the task until it is notified by DMA interrupt
 * 
 * 
 */
uint32_t spi_wait(spi_dev_e dev) {
    
    uint8_t ret = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if(dev == DEV_DISP || dev == DEV_SD)
        while(spi1.hspi.Instance->SR & SPI_SR_BSY);
    else
        while(spi2.hspi.Instance->SR & SPI_SR_BSY);
    return ret;
}

/*****************
 * STATIC DEFS
 *****************/

/**
 * @brief Configures the SPI peripheral used by ETH/DISP
 */
static void spi1_configure(void) {
    /************* CONFIGURE SPI TX DMA *******************/
    spi1.hdmatx.Instance = DMA2_Stream2;
    spi1.hdmatx.Init.Channel = DMA_CHANNEL_2;
    spi1.hdmatx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    spi1.hdmatx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    spi1.hdmatx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    spi1.hdmatx.Init.MemInc = DMA_MINC_ENABLE;
    spi1.hdmatx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    spi1.hdmatx.Init.PeriphInc = DMA_PINC_DISABLE;
    spi1.hdmatx.Init.Mode = DMA_NORMAL;

    /************* CONFIGURE SPI RX DMA *******************/
    spi1.hdmarx.Instance = DMA2_Stream0;
    spi1.hdmarx.Init.Channel = DMA_CHANNEL_3;
    spi1.hdmarx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    spi1.hdmarx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    spi1.hdmarx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    spi1.hdmarx.Init.MemInc = DMA_MINC_ENABLE;
    spi1.hdmarx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    spi1.hdmarx.Init.PeriphInc = DMA_PINC_DISABLE;
    spi1.hdmarx.Init.Mode = DMA_NORMAL;

    /************** CONFIGURE SPI PERIPH ***************/
    spi1.hspi.Instance = SPI1;
    spi1.hspi.Init.CLKPolarity = SPI_POLARITY_LOW;
    spi1.hspi.Init.CLKPhase = SPI_PHASE_1EDGE;
    spi1.hspi.Init.Mode = SPI_MODE_MASTER;
    spi1.hspi.Init.DataSize = SPI_DATASIZE_8BIT;
    spi1.hspi.Init.FirstBit = SPI_FIRSTBIT_MSB;
    spi1.hspi.Init.NSS = SPI_NSS_SOFT;
    spi1.hspi.Init.TIMode = SPI_TIMODE_DISABLE;
    spi1.hspi.Init.Direction = SPI_DIRECTION_2LINES;
    spi1.hspi.Init.CRCPolynomial = SPI_CRCCALCULATION_DISABLE;
    spi1.hspi.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;

    HAL_DMA_Init(&spi1.hdmatx);
    HAL_DMA_Init(&spi1.hdmarx);
    HAL_SPI_Init(&spi1.hspi);

    __HAL_LINKDMA(&spi1.hspi, hdmatx, spi1.hdmatx);
    __HAL_LINKDMA(&spi1.hspi, hdmarx, spi1.hdmarx);
    NVIC_SetPriority(SPI_RX_GET_IRQn(&spi1.hspi), 5);
    NVIC_SetPriority(SPI_TX_GET_IRQn(&spi1.hspi), 5);
    NVIC_EnableIRQ(SPI_RX_GET_IRQn(&spi1.hspi));
    NVIC_EnableIRQ(SPI_TX_GET_IRQn(&spi1.hspi));
}



/**
 * @brief Config SPI periph used by MFRC/SD reader
 */
static void spi2_configure(void) {
    /************* CONFIGURE SPI TX DMA *******************/
    spi2.hdmatx.Instance = DMA1_Stream4;
    spi2.hdmatx.Init.Channel = DMA_CHANNEL_0;
    spi2.hdmatx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    spi2.hdmatx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    spi2.hdmatx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    spi2.hdmatx.Init.MemInc = DMA_MINC_ENABLE;
    spi2.hdmatx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    spi2.hdmatx.Init.PeriphInc = DMA_PINC_DISABLE;
    spi2.hdmatx.Init.Mode = DMA_NORMAL;

    /************* CONFIGURE SPI RX DMA *******************/
    spi2.hdmarx.Instance = DMA1_Stream3;
    spi2.hdmarx.Init.Channel = DMA_CHANNEL_0;
    spi2.hdmarx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    spi2.hdmarx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    spi2.hdmarx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    spi2.hdmarx.Init.MemInc = DMA_MINC_ENABLE;
    spi2.hdmarx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    spi2.hdmarx.Init.PeriphInc = DMA_PINC_DISABLE;
    spi2.hdmarx.Init.Mode = DMA_NORMAL;

    /************** CONFIGURE SPI PERIPH ***************/
    spi2.hspi.Instance = SPI2;
    spi2.hspi.Init.CLKPolarity = SPI_POLARITY_LOW;
    spi2.hspi.Init.CLKPhase = SPI_PHASE_1EDGE;
    spi2.hspi.Init.Mode = SPI_MODE_MASTER;
    spi2.hspi.Init.DataSize = SPI_DATASIZE_8BIT;
    spi2.hspi.Init.FirstBit = SPI_FIRSTBIT_MSB;
    spi2.hspi.Init.NSS = SPI_NSS_SOFT;
    spi2.hspi.Init.TIMode = SPI_TIMODE_DISABLE;
    spi2.hspi.Init.Direction = SPI_DIRECTION_2LINES;
    spi2.hspi.Init.CRCPolynomial = SPI_CRCCALCULATION_DISABLE;
    spi2.hspi.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128;

    HAL_DMA_Init(&spi2.hdmatx);
    HAL_DMA_Init(&spi2.hdmarx);
    HAL_SPI_Init(&spi2.hspi);

    __HAL_LINKDMA(&spi2.hspi, hdmatx, spi2.hdmatx);
    __HAL_LINKDMA(&spi2.hspi, hdmarx, spi2.hdmarx);
    NVIC_SetPriority(SPI_RX_GET_IRQn(&spi2.hspi), 5);
    NVIC_SetPriority(SPI_TX_GET_IRQn(&spi2.hspi), 5);
    NVIC_EnableIRQ(SPI_RX_GET_IRQn(&spi2.hspi));
    NVIC_EnableIRQ(SPI_TX_GET_IRQn(&spi2.hspi));
}


/**
 * @brief 
 */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (hspi->Instance == SPI1) {
        spi1.hpt_trigger = pdFALSE;
        if (spi1.curr_dev == DEV_SD) {
            // handle whatever SD stufff we need to do

        }
        else {
            // signal flush complete to display
            
        }

        vTaskNotifyGiveFromISR(spi1.curr_task, &spi1.hpt_trigger);
        portYIELD_FROM_ISR(spi1.hpt_trigger);
    }
    else if (hspi->Instance == SPI2) {
        spi2.hpt_trigger = pdFALSE;
        if (spi2.curr_dev == DEV_ETH) {
            
        }

        vTaskNotifyGiveFromISR(spi2.curr_task, &spi2.hpt_trigger);
        portYIELD_FROM_ISR(spi2.hpt_trigger);
    }
}


/**
 * @brief 
 */
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (hspi->Instance == SPI1) {
        spi1.hpt_trigger = pdFALSE;
        if (spi1.curr_dev == DEV_SD) {
            // signal eth controller dk what we receiving rn tbh...

        }
        else {
            // we shouldn't need to receive anything from display
        }
        vTaskNotifyGiveFromISR(spi1.curr_task, &spi1.hpt_trigger);
        portYIELD_FROM_ISR(spi1.hpt_trigger);
    }
    else if (hspi->Instance == SPI2) {
        spi2.hpt_trigger = pdFALSE;
        if (spi2.curr_dev == DEV_ETH) {

        }
        vTaskNotifyGiveFromISR(spi2.curr_task, &spi2.hpt_trigger);
        portYIELD_FROM_ISR(spi2.hpt_trigger);
    }
}


void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi) {
    HAL_SPI_TxRxCpltCallback(hspi);
}

/**
 * @brief Handler for SPI1 TX (ethernet/display)
 */
void DMA2_Stream2_IRQHandler(void) {
    HAL_DMA_IRQHandler(&spi1.hdmatx);
}


/**
 * @brief Handler for SPI1 RX
 */
void DMA2_Stream0_IRQHandler(void) {
    HAL_DMA_IRQHandler(&spi1.hdmarx);
}


/**
 * @brief Handler for SPI2 TX (tag/sd card)
 */
void DMA1_Stream4_IRQHandler(void) {
    HAL_DMA_IRQHandler(&spi2.hdmatx);
}


/**
 * @brief Handler for SPI2 RX
 */
void DMA1_Stream3_IRQHandler(void) {
    HAL_DMA_IRQHandler(&spi2.hdmarx);
}
